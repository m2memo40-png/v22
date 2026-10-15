#!/usr/bin/env python3
"""Validate the x86-64 ELF/Limine handoff contract without third-party modules."""
from __future__ import annotations
import struct
import subprocess
import sys
from pathlib import Path

PT_LOAD = 1
PF_X = 1
ELF64_EHDR = struct.Struct("<HHIQQQIHHHHHH")
ELF64_PHDR = struct.Struct("<IIQQQQQQ")
ELF64_SHDR = struct.Struct("<IIQQQQIIQQ")


def fail(message: str) -> int:
    print(f"KERNEL ELF CHECK FAILED: {message}", file=sys.stderr)
    return 1


def main() -> int:
    if len(sys.argv) != 2:
        return fail("usage: check-kernel-elf.py build/kernel.elf")
    path = Path(sys.argv[1])
    try:
        data = path.read_bytes()
    except OSError as exc:
        return fail(f"cannot read {path}: {exc}")
    if len(data) < 64 or data[:4] != b"\x7fELF":
        return fail("file is not a complete ELF")
    if data[4] != 2 or data[5] != 1:
        return fail("expected ELF64 little-endian")

    try:
        (etype, machine, version, entry, phoff, shoff, flags,
         ehsize, phentsize, phnum, shentsize, shnum, shstrndx) = ELF64_EHDR.unpack_from(data, 16)
    except struct.error:
        return fail("truncated ELF header")
    if etype != 2:
        return fail(f"expected ET_EXEC (2), got {etype}")
    if machine != 62:
        return fail(f"expected x86-64 machine id 62, got {machine}")
    if version != 1 or ehsize != 64 or phentsize != ELF64_PHDR.size or shentsize != ELF64_SHDR.size:
        return fail("unsupported or inconsistent ELF header sizes/version")
    if phnum == 0 or phoff + phnum * phentsize > len(data):
        return fail("program header table is missing or truncated")
    if shnum == 0 or shstrndx >= shnum or shoff + shnum * shentsize > len(data):
        return fail("section header table is missing or truncated")

    loads: list[tuple[int, int, int, int, int, int, int]] = []
    for idx in range(phnum):
        fields = ELF64_PHDR.unpack_from(data, phoff + idx * phentsize)
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = fields
        if p_type != PT_LOAD:
            continue
        if p_filesz > p_memsz:
            return fail(f"PT_LOAD #{idx}: file size exceeds memory size")
        if p_offset + p_filesz > len(data):
            return fail(f"PT_LOAD #{idx}: file bytes extend beyond ELF")
        if p_align > 1 and p_vaddr % p_align != p_offset % p_align:
            return fail(f"PT_LOAD #{idx}: p_vaddr/p_offset alignment mismatch")
        loads.append((p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align))
    if not loads:
        return fail("no PT_LOAD segments")

    # Limine refuses an ELF when two PT_LOAD segments with different flags
    # touch the same 4 KiB page. Check page-rounded memory ranges, not only
    # raw byte ranges: the previous linker script placed .limine_requests
    # immediately after .rodata, so RO and RW headers shared one page.
    page_size = 4096
    for i, a in enumerate(loads):
        flags_a, _, vaddr_a, _, _, memsz_a, _ = a
        if memsz_a == 0:
            continue
        a_start = vaddr_a & ~(page_size - 1)
        a_end = (vaddr_a + memsz_a + page_size - 1) & ~(page_size - 1)
        for j in range(i + 1, len(loads)):
            b = loads[j]
            flags_b, _, vaddr_b, _, _, memsz_b, _ = b
            if memsz_b == 0 or flags_a == flags_b:
                continue
            b_start = vaddr_b & ~(page_size - 1)
            b_end = (vaddr_b + memsz_b + page_size - 1) & ~(page_size - 1)
            if max(a_start, b_start) < min(a_end, b_end):
                return fail(
                    f"PT_LOAD #{i} flags=0x{flags_a:x} and PT_LOAD #{j} "
                    f"flags=0x{flags_b:x} share a 4 KiB page "
                    f"(0x{max(a_start, b_start):x}); align differently-permissioned "
                    "sections to separate pages (e.g. .limine_requests)."
                )

    if not any((fl & PF_X) and v <= entry < v + mem for fl, off, v, pa, fs, mem, al in loads):
        return fail(f"entry 0x{entry:x} is not inside an executable PT_LOAD segment")

    # Decode section names to ensure Limine requests are present in file-backed,
    # loaded memory early enough to be scanned by the bootloader.
    try:
        shstr = ELF64_SHDR.unpack_from(data, shoff + shstrndx * shentsize)
        names_off, names_size = shstr[4], shstr[5]
        if names_off + names_size > len(data):
            return fail("section-name string table extends beyond ELF")
        names = data[names_off:names_off + names_size]
        sections = []
        for idx in range(shnum):
            section = ELF64_SHDR.unpack_from(data, shoff + idx * shentsize)
            name_off, sh_type, sh_flags, sh_addr, sh_offset, sh_size = section[:6]
            if name_off >= len(names):
                section_name = ""
            else:
                stop = names.find(b"\0", name_off)
                if stop < 0:
                    return fail("unterminated section name")
                section_name = names[name_off:stop].decode("ascii", errors="replace")
            sections.append((section_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size))
    except struct.error:
        return fail("truncated section header table")

    requests = next((s for s in sections if s[0] == ".limine_requests"), None)
    if not requests:
        return fail(".limine_requests section is missing")
    _, sh_type, sh_flags, sh_addr, sh_offset, sh_size = requests
    if not (sh_flags & 2):  # SHF_ALLOC
        return fail(".limine_requests is not SHF_ALLOC")
    if sh_type != 8 and sh_offset + sh_size > len(data):
        return fail(".limine_requests extends beyond the ELF file")
    if sh_offset >= 2 * 1024 * 1024:
        return fail(f".limine_requests file offset 0x{sh_offset:x} is beyond the first 2 MiB scan window")
    if not any(v <= sh_addr and sh_addr + sh_size <= v + mem for _, _, v, _, _, mem, _ in loads):
        return fail(".limine_requests is not contained in a PT_LOAD segment")

    try:
        nm_out = subprocess.run(["nm", "-n", str(path)], check=True, capture_output=True, text=True).stdout
    except (OSError, subprocess.CalledProcessError) as exc:
        return fail(f"could not inspect _start symbol with nm: {exc}")
    start = None
    for line in nm_out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-1] == "_start":
            try:
                start = int(parts[0], 16)
            except ValueError:
                pass
            break
    if start is None:
        return fail("_start symbol is missing")
    if start != entry:
        return fail(f"ELF entry 0x{entry:x} does not equal _start 0x{start:x}")
    print(f"KERNEL ELF CHECK PASSED: ELF64 x86-64 ET_EXEC; entry/_start=0x{entry:x}; {len(loads)} PT_LOAD segments; .limine_requests at file offset 0x{sh_offset:x}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
