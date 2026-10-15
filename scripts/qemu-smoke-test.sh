#!/usr/bin/env bash
# Reliable interactive QEMU BIOS smoke test. Requires the framebuffer desktop to
# start, exits it over COM1, then waits for shell prompts before issuing test commands.
set -euo pipefail
ISO_PATH="${1:-build/Fluxmax-x86_64.iso}"
LOG_PATH="${2:-build/qemu-smoke.log}"
QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"
HTTP_SERVER_PID=""
HTTP_SMOKE_PORT=""
mkdir -p "$(dirname "$LOG_PATH")"
HTTP_PORT_FILE="${LOG_PATH%.log}.http-port"
if [[ "${FLUXMAX_HTTP_SMOKE:-0}" == "1" ]]; then
  command -v python3 >/dev/null || { echo "python3 is required for the HTTP smoke fixture." >&2; exit 2; }
  rm -f "$HTTP_PORT_FILE"
  python3 scripts/http-smoke-server.py --port 0 --port-file "$HTTP_PORT_FILE" >"${LOG_PATH%.log}.http-server.log" 2>&1 &
  HTTP_SERVER_PID=$!
  HTTP_SERVER_LOG="${LOG_PATH%.log}.http-server.log"
  cleanup_http_fixture() {
    local rc=$?
    trap - EXIT
    if [[ $rc -ne 0 && -f "$HTTP_SERVER_LOG" ]]; then
      echo "----- HTTP smoke fixture log -----" >&2
      cat "$HTTP_SERVER_LOG" >&2 || true
      echo "----- end HTTP smoke fixture log -----" >&2
    fi
    if [[ -n "$HTTP_SERVER_PID" ]]; then
      kill "$HTTP_SERVER_PID" 2>/dev/null || true
      wait "$HTTP_SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$HTTP_PORT_FILE"
    exit "$rc"
  }
  trap cleanup_http_fixture EXIT
  for _ in $(seq 1 50); do [[ -s "$HTTP_PORT_FILE" ]] && break; sleep .1; done
  [[ -s "$HTTP_PORT_FILE" ]] || { echo "HTTP smoke fixture failed to start or publish its port." >&2; exit 2; }
  HTTP_SMOKE_PORT="$(cat "$HTTP_PORT_FILE")"
  python3 - "$HTTP_SMOKE_PORT" <<'PYREADY'
import socket, sys
s = socket.socket(); s.settimeout(1.0)
try: s.connect(("127.0.0.1", int(sys.argv[1])))
finally: s.close()
PYREADY
fi
[[ -s "$ISO_PATH" ]] || { echo "ISO not found: $ISO_PATH" >&2; exit 2; }
command -v "$QEMU_BIN" >/dev/null || { echo "QEMU executable not found: $QEMU_BIN" >&2; exit 2; }
export QEMU_BIN FLUXMAX_HTTP_SMOKE HTTP_SMOKE_PORT
python3 scripts/qemu-smoke-driver.py "$ISO_PATH" "$LOG_PATH"
