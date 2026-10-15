#!/usr/bin/env bash
set -euo pipefail
ISO="${1:-build/Fluxmax-x86_64.iso}"
if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "qemu-system-x86_64 is not installed" >&2; exit 127
fi
if [[ ! -s "$ISO" ]]; then
    echo "ISO not found: $ISO. Build it with make iso LIMINE_DIR=/path/to/limine-binary" >&2; exit 2
fi
qemu-system-x86_64 -m 512M -machine q35 -cdrom "$ISO" -boot d -serial stdio -no-reboot -no-shutdown
