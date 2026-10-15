#!/usr/bin/env bash
# Fail early with a clear path if the checked-out repository is missing files
# referenced by the Makefile. In particular, HTTP/TCP builds require
# include/net/tcp.h to be committed at the repository root.
set -euo pipefail

required_files=(
    include/kernel/log.h
    arch/x86_64/entry.S
    scripts/check-kernel-elf.py
  "include/net/tcp.h"
  "include/drivers/ps2_mouse.h"
  "include/drivers/ps2_mouse_protocol.h"
  "include/drivers/ps2_keymap.h"
  "drivers/keyboard/ps2_keymap.c"
  "tests/host_ps2_keymap_tests.c"
  "tests/host_console_frame_tests.c"
  "drivers/input/ps2_mouse.c"
  "drivers/input/ps2_mouse_protocol.c"
  "tests/host_ps2_mouse_tests.c"
  "include/net/net.h"
  "include/net/ipv4.h"
  "net/tcp.c"
  "net/net.c"
  "drivers/net/e1000.c"
  "include/drivers/e1000.h"
  "browser/http_client.c"
  "browser/browser.c"
  "tests/host_http_client_tests.c"
  "tests/host_tcp_tests.c"
  "scripts/qemu-smoke-driver.py"
  "scripts/qemu-smoke-test.sh"
  "Makefile"
)
missing=0
for file in "${required_files[@]}"; do
  if [[ ! -s "$file" ]]; then
    printf 'ERROR: required project file is missing or empty: %s\n' "$file" >&2
    missing=1
  fi
done
if (( missing )); then
  cat >&2 <<'MSG'
This build cannot compile the current checkout. Restore the missing file(s) in
this exact case-sensitive path at the repository root, commit them to main/master,
and push all source files. Do not upload only changed .c files or only the Makefile.
MSG
  exit 2
fi
printf 'Source-tree preflight: %u required files found.\n' "${#required_files[@]}"
