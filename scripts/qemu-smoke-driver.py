#!/usr/bin/env python3
"""Prompt-synchronized QEMU serial smoke-test driver (stdlib only).

Uses a raw pseudo-terminal. The guest now boots to its framebuffer desktop, so the
driver sends `q` over COM1 to leave the GUI before waiting for the shell prompt.
"""
from __future__ import annotations
import os
import pty
import select
import signal
import socket
import subprocess
import sys
import time
import tty
from pathlib import Path

PROMPT = b"fluxmax> "

class SmokeFailure(RuntimeError):
    pass

def capture_monitor_screenshot(monitor_path: Path, screenshot_path: Path) -> bool:
    """Ask QEMU's human monitor for a VGA screenshot when boot/serial fails."""
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline and not monitor_path.exists():
        time.sleep(0.05)
    if not monitor_path.exists():
        return False
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.settimeout(0.4)
    try:
        client.connect(str(monitor_path))
        greeting = bytearray()
        end = time.monotonic() + 1.5
        while time.monotonic() < end and b"(qemu)" not in greeting:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                break
            greeting.extend(chunk)
        client.sendall(f"screendump {screenshot_path.resolve()}\n".encode())
        response = bytearray()
        end = time.monotonic() + 1.5
        while time.monotonic() < end and b"(qemu)" not in response:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                break
            response.extend(chunk)
        return screenshot_path.is_file() and screenshot_path.stat().st_size > 0
    except OSError:
        return False
    finally:
        client.close()


def print_ppm_ascii(path: Path, columns: int = 96, rows: int = 36) -> None:
    """Print a small P6/P3 PPM screen preview into the CI log (no artifact needed)."""
    try:
        raw = path.read_bytes()
    except OSError as exc:
        print(f"(screenshot preview unavailable: {exc})", file=sys.stderr)
        return

    i = 0
    def token() -> bytes:
        nonlocal i
        n = len(raw)
        while i < n:
            if raw[i] in b" \t\r\n\v\f":
                i += 1
                continue
            if raw[i] == ord('#'):
                while i < n and raw[i] not in b"\r\n":
                    i += 1
                continue
            break
        start = i
        while i < n and raw[i] not in b" \t\r\n\v\f#":
            i += 1
        return raw[start:i]

    try:
        magic = token()
        width, height, maxval = int(token()), int(token()), int(token())
        if width <= 0 or height <= 0 or maxval <= 0 or maxval > 255:
            raise ValueError("unsupported dimensions or channel range")
        if magic == b"P6":
            # P6 uses binary RGB bytes; consume exactly the header terminator.
            if i >= len(raw) or raw[i] not in b" \t\r\n":
                raise ValueError("missing PPM header delimiter")
            if raw[i:i+2] == b"\r\n":
                i += 2
            else:
                i += 1
            pixels = raw[i:]
            if len(pixels) < width * height * 3:
                raise ValueError("truncated PPM pixel buffer")
            def lum_at(x: int, y: int) -> int:
                j = (y * width + x) * 3
                return (299*pixels[j] + 587*pixels[j+1] + 114*pixels[j+2]) // 1000
        elif magic == b"P3":
            values = []
            needed = width * height * 3
            for _ in range(needed):
                values.append(int(token()))
            def lum_at(x: int, y: int) -> int:
                j = (y * width + x) * 3
                return (299*values[j] + 587*values[j+1] + 114*values[j+2]) // 1000
        else:
            raise ValueError(f"unsupported PPM format {magic!r}")
        ramp = " .:-=+*#%@"
        out_w = min(columns, width)
        out_h = min(rows, max(1, int(height * out_w / width * 0.5)))
        print(f"----- VGA screen preview ({width}x{height}, ASCII) -----", file=sys.stderr)
        for oy in range(out_h):
            sy = min(height - 1, int((oy + 0.5) * height / out_h))
            line = []
            for ox in range(out_w):
                sx = min(width - 1, int((ox + 0.5) * width / out_w))
                brightness = lum_at(sx, sy) * (len(ramp)-1) // maxval
                line.append(ramp[min(len(ramp)-1, brightness)])
            print(''.join(line), file=sys.stderr)
        print("----- end VGA screen preview -----", file=sys.stderr)
    except (ValueError, IndexError) as exc:
        print(f"(could not decode PPM screenshot: {exc})", file=sys.stderr)

def print_file_tail(label: str, path: Path, max_bytes: int = 10000) -> None:
    print(f"----- {label} -----", file=sys.stderr)
    try:
        data = path.read_bytes()
        if not data:
            print("(empty)", file=sys.stderr)
        else:
            print(data[-max_bytes:].decode("utf-8", errors="replace"), file=sys.stderr)
    except OSError as exc:
        print(f"(not available: {exc})", file=sys.stderr)

class Guest:
    def __init__(self, proc: subprocess.Popen[bytes], master: int, log_path: Path):
        self.proc, self.master = proc, master
        self.log_path = log_path
        self.log = log_path.open("wb")
        self.buffer = bytearray()

    def _read_once(self, timeout: float) -> None:
        if self.proc.poll() is not None:
            raise SmokeFailure(f"QEMU exited unexpectedly with code {self.proc.returncode}")
        ready, _, _ = select.select([self.master], [], [], max(0.0, timeout))
        if not ready:
            return
        try:
            chunk = os.read(self.master, 8192)
        except OSError as exc:
            raise SmokeFailure(f"could not read guest serial PTY: {exc}") from exc
        if not chunk:
            return
        self.log.write(chunk)
        self.log.flush()
        self.buffer.extend(chunk)
        # The full transcript is persisted; keep in-memory matching bounded.
        if len(self.buffer) > 2 * 1024 * 1024:
            del self.buffer[:-1024 * 1024]

    def wait_for(self, text: str, timeout: float, *, start: int = 0) -> int:
        needle = text.encode("utf-8")
        deadline = time.monotonic() + timeout
        while True:
            pos = self.buffer.find(needle, start)
            if pos >= 0:
                return pos + len(needle)
            if time.monotonic() >= deadline:
                raise SmokeFailure(f"timed out after {timeout:.1f}s waiting for: {text!r}")
            self._read_once(min(0.2, deadline - time.monotonic()))

    def wait_for_any(self, texts: list[str], timeout: float, *, start: int = 0) -> str:
        """Wait for the first success/failure marker so application failures are immediate."""
        needles = [(text, text.encode("utf-8")) for text in texts]
        deadline = time.monotonic() + timeout
        while True:
            candidates = [(self.buffer.find(raw, start), text) for text, raw in needles]
            candidates = [(pos, text) for pos, text in candidates if pos >= 0]
            if candidates:
                return min(candidates, key=lambda item: item[0])[1]
            if time.monotonic() >= deadline:
                raise SmokeFailure(f"timed out after {timeout:.1f}s waiting for any of: {texts!r}")
            self._read_once(min(0.2, deadline - time.monotonic()))

    def send(self, command: str) -> int:
        start = len(self.buffer)
        os.write(self.master, command.encode("ascii") + b"\r")
        return start

    def command(self, command: str, expected: list[str], timeout: float = 20.0) -> None:
        start = self.send(command)
        pos = start
        for text in expected:
            pos = self.wait_for(text, timeout, start=pos)
        self.wait_for(PROMPT.decode(), 12.0, start=pos)

    def close(self) -> None:
        try: self.log.close()
        except Exception: pass
        try: os.close(self.master)
        except OSError: pass




def monitor_command(monitor_path: Path, command: str, timeout: float = 2.0) -> bytes:
    """Issue one HMP command through the local QEMU monitor socket."""
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.settimeout(0.2)
    transcript = bytearray()
    try:
        client.connect(str(monitor_path))
        end = time.monotonic() + timeout
        while time.monotonic() < end and b"(qemu)" not in transcript:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                break
            transcript.extend(chunk)
        if b"(qemu)" not in transcript:
            raise SmokeFailure("QEMU HMP monitor did not present its prompt")
        client.sendall((command + "\n").encode("ascii"))
        response_start = len(transcript)
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                break
            transcript.extend(chunk)
            if b"(qemu)" in transcript[response_start:]:
                break
        response = bytes(transcript[response_start:])
        if b"unknown command" in response.lower() or b"invalid parameter" in response.lower():
            raise SmokeFailure(f"QEMU monitor rejected {command!r}: {response.decode(errors='replace')}")
        return response
    except OSError as exc:
        raise SmokeFailure(f"could not send {command!r} to QEMU monitor: {exc}") from exc
    finally:
        client.close()


def wait_debugcon_marker(path: Path, marker: bytes, timeout: float) -> bool:
    """Wait for an instrumented guest input path to emit a debugcon marker."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if marker in path.read_bytes():
                return True
        except OSError:
            pass
        time.sleep(0.05)
    try:
        return marker in path.read_bytes()
    except OSError:
        return False

def capture_monitor_diagnostics(monitor_path: Path, output_path: Path) -> bool:
    """Collect final guest CPU state and virtual device topology via HMP."""
    if not monitor_path.exists():
        return False
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.settimeout(0.35)
    transcript = bytearray()
    try:
        client.connect(str(monitor_path))
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline and b"(qemu)" not in transcript:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                break
            transcript.extend(chunk)
        for command in ("info registers", "info block", "info roms", "info pci", "info bootindex"):
            client.sendall((command + "\n").encode("ascii"))
            response = bytearray()
            deadline = time.monotonic() + 1.2
            while time.monotonic() < deadline:
                try:
                    chunk = client.recv(8192)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                response.extend(chunk)
                if b"(qemu)" in response:
                    break
            transcript.extend(b"\n===== HMP: " + command.encode() + b" =====\n")
            transcript.extend(response)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(transcript)
        return output_path.is_file() and output_path.stat().st_size > 0
    except OSError as exc:
        try:
            output_path.write_text(f"Could not query QEMU monitor: {exc}\n", encoding="utf-8")
        except OSError:
            pass
        return False
    finally:
        client.close()

def main() -> int:
    if len(sys.argv) != 3:
        print("usage: qemu-smoke-driver.py ISO LOG", file=sys.stderr)
        return 2
    iso, log_path = Path(sys.argv[1]), Path(sys.argv[2])
    qemu = os.environ.get("QEMU_BIN", "qemu-system-x86_64")
    nic_model = os.environ.get("FLUXMAX_QEMU_NIC", "rtl8139").strip().lower()
    if nic_model not in {"rtl8139", "e1000"}:
        print("FLUXMAX_QEMU_NIC must be rtl8139 or e1000.", file=sys.stderr)
        return 2
    http_enabled = os.environ.get("FLUXMAX_HTTP_SMOKE", "0") == "1"
    http_port = os.environ.get("HTTP_SMOKE_PORT", "")
    if http_enabled and not http_port.isdigit():
        print("HTTP smoke enabled but HTTP_SMOKE_PORT is not a numeric port.", file=sys.stderr)
        return 2
    if not iso.is_file() or iso.stat().st_size == 0:
        print(f"ISO not found or empty: {iso}", file=sys.stderr)
        return 2
    log_path.parent.mkdir(parents=True, exist_ok=True)
    try: log_path.unlink()
    except FileNotFoundError: pass

    debugcon_path = Path(str(log_path) + ".debugcon.log").resolve()
    qemu_trace_path = Path(str(log_path) + ".qemu-trace.log").resolve()
    screenshot_path = Path(str(log_path) + ".screen.ppm").resolve()
    monitor_path = Path(str(log_path) + ".monitor.sock").resolve()
    for generated in (debugcon_path, qemu_trace_path, screenshot_path, monitor_path, Path(str(log_path) + ".monitor-state.txt").resolve()):
        try: generated.unlink()
        except FileNotFoundError: pass

    master, slave = pty.openpty()
    tty.setraw(slave)
    cmd = [qemu, "-machine", "pc,accel=tcg", "-cpu", "qemu64", "-m", "256M",
           "-drive", f"file={iso},media=cdrom,readonly=on,if=ide,index=2", "-boot", "order=d,strict=on", "-nic", f"user,model={nic_model}",
           "-display", "none", "-monitor", f"unix:{monitor_path},server=on,wait=off",
           "-serial", "stdio", "-debugcon", f"file:{debugcon_path}",
           "-global", "isa-debugcon.iobase=0xe9",
           "-d", "guest_errors,cpu_reset,unimp", "-D", str(qemu_trace_path),
           "-no-reboot", "-no-shutdown"]
    proc: subprocess.Popen[bytes] | None = None
    guest: Guest | None = None
    try:
        proc = subprocess.Popen(cmd, stdin=slave, stdout=slave, stderr=slave,
                                close_fds=True, start_new_session=True)
        os.close(slave)
        guest = Guest(proc, master, log_path)
        # The regular boot path enters the framebuffer desktop first. Inject a real
        # QEMU mouse event and PS/2 keyboard keystroke before checking shell commands.
        desktop_marker = "Fluxmax window manager started."
        boot_stage = guest.wait_for_any([desktop_marker, PROMPT.decode()], 35.0)
        if boot_stage != desktop_marker:
            raise SmokeFailure("kernel reached the shell without starting the framebuffer desktop; QEMU graphics smoke check failed")

        monitor_command(monitor_path, "mouse_move 72 36")
        if not wait_debugcon_marker(debugcon_path, b"FLUXMAX:MOUSE_EVENT_SEEN", 5.0):
            raise SmokeFailure("QEMU mouse_move did not reach the desktop PS/2 mouse event loop")
        print("PS/2 mouse event reached the desktop input loop.")

        gui_quit_start = len(guest.buffer)
        monitor_command(monitor_path, "sendkey q")
        guest.wait_for(PROMPT.decode(), 20.0, start=gui_quit_start)
        if not wait_debugcon_marker(debugcon_path, b"FLUXMAX:KEYBOARD_EVENT_SEEN", 3.0):
            raise SmokeFailure("QEMU sendkey did not reach the desktop PS/2 keyboard input queue")
        print(f"QEMU desktop keyboard input returned to the interactive shell (NIC: {nic_model}).")

        guest.command("selftest", ["SELFTEST SUMMARY: 14 passed, 0 failed."], 25.0)
        print("Kernel self-tests passed (14/14).")
        guest.command("run /bin/hello", ["Hello from Fluxmax user mode (Ring 3)!",
                                           "exited with status 0"], 25.0)
        print("Ring-3 ELF demo passed.")

        # Network initialization happens during boot and DHCP is asynchronous.
        nic_marker = "RTL8139 ready in polling mode" if nic_model == "rtl8139" else "Intel E1000 ready in polling mode"
        if nic_marker.encode() not in guest.buffer:
            guest.wait_for(nic_marker, 15.0, start=0)
        if b"Lease ACK accepted" not in guest.buffer:
            guest.wait_for("Lease ACK accepted", 25.0, start=0)
        print(f"{nic_model} initialization and DHCP lease passed.")
        guest.command("ping 10.0.2.2", ["ICMP echo reply received."], 15.0)
        guest.command("resolve example.com", ["DNS A record:"], 18.0)
        print("ICMP gateway ping and DNS lookup passed.")

        if http_enabled:
            start = guest.send(f"browser http://10.0.2.2:{http_port}/")
            outcome = guest.wait_for_any([
                "Fluxmax HTTP browser smoke page",
                "TCP connect failed or timed out.",
                # Match complete diagnostic suffixes; the earlier prefix marker
                # stopped collection before the TCP error number/reason arrived.
                ": local transmit/driver failure).",
                ": ACK/retransmit timeout).",
                ": peer reset).",
                ": send deadline elapsed).",
                ": unspecified transport state).",
                "request send failed.",
                "no HTTP response was received.",
                "response header is missing or too large.",
                "response timed out before completion.",
                PROMPT.decode(),
            ], 25.0, start=start)
            if outcome != "Fluxmax HTTP browser smoke page":
                # Preserve a short direct diagnosis in Actions rather than waiting
                # the full timeout when the guest has already returned to its shell.
                raise SmokeFailure(f"HTTP browser command stopped at marker: {outcome!r}")
            pos = guest.buffer.find(outcome.encode("utf-8"), start) + len(outcome)
            guest.wait_for(PROMPT.decode(), 12.0, start=pos)
            if b"this_text_must_not_be_rendered" in guest.buffer:
                raise SmokeFailure("HTML renderer exposed script content.")
            print("Built-in plain-HTTP browser path passed.")

        halt_start = guest.send("halt")
        guest.wait_for("CPU halted. Reset the virtual machine to continue.", 5.0, start=halt_start)
        print("QEMU boot smoke test passed all requested checks.")
        return 0
    except (SmokeFailure, OSError, subprocess.SubprocessError) as exc:
        print(f"QEMU smoke test failed: {exc}", file=sys.stderr)
        monitor_state_path = Path(str(log_path) + ".monitor-state.txt").resolve()
        monitor_ok = capture_monitor_diagnostics(monitor_path, monitor_state_path)
        print(f"QEMU monitor state: {monitor_state_path if monitor_ok else 'not available'}", file=sys.stderr)
        if monitor_ok:
            print_file_tail("QEMU monitor final state", monitor_state_path, 20000)
        if guest:
            print_file_tail("guest serial log", log_path)
        shot_ok = capture_monitor_screenshot(monitor_path, screenshot_path)
        print(f"QEMU VGA screenshot: {'captured at ' + str(screenshot_path) if shot_ok else 'not available'}", file=sys.stderr)
        debugcon_data = debugcon_path.read_bytes() if debugcon_path.exists() else b""
        serial_data = log_path.read_bytes() if log_path.exists() else b""
        if b"FLUXMAX:ENTRY" not in debugcon_data:
            if not serial_data.strip():
                print("BOOT TRIAGE: No COM1 text and no FLUXMAX:ENTRY marker. Failure is before the kernel entry or the bootloader is not using the expected config/serial route.", file=sys.stderr)
            else:
                print("BOOT TRIAGE: COM1 produced text but the kernel entry marker is absent. Inspect the Limine messages below; the selected kernel probably was not entered.", file=sys.stderr)
        elif b"FLUXMAX:SHELL_READY" not in debugcon_data:
            print("BOOT TRIAGE: Kernel entry ran but did not reach SHELL_READY. The last FLUXMAX marker narrows the failing init stage.", file=sys.stderr)
        else:
            print("BOOT TRIAGE: Kernel reported SHELL_READY; failure is in shell input/protocol smoke interaction.", file=sys.stderr)
        print_file_tail("early kernel debugcon (port 0xE9)", debugcon_path)
        print_file_tail("QEMU guest error/reset trace (interrupt spam disabled)", qemu_trace_path, 30000)
        if shot_ok:
            print_ppm_ascii(screenshot_path)
        print(f"Full serial log: {log_path}", file=sys.stderr)
        return 1
    finally:
        if proc is not None and proc.poll() is None:
            try: proc.terminate()
            except OSError: pass
            try: proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill(); proc.wait(timeout=3)
        if guest:
            guest.close()
        else:
            try: os.close(master)
            except OSError: pass
            try: os.close(slave)
            except OSError: pass

if __name__ == "__main__":
    raise SystemExit(main())
