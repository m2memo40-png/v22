#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Check user-visible branding and CI artifact contract.
grep -q '^/Fluxmax ' limine.conf
grep -q 'Fluxmax window manager started\.' gui/desktop.c
grep -q 'fluxmax> ' scripts/qemu-smoke-driver.py
grep -q 'Fluxmax-x86_64.iso' Makefile
# Reject the former brand in text source/docs. This checker is excluded itself.
if grep -RInE 'Tuyryu|TUYRYU|tuyryu' \
  --exclude='check-branding.sh' --exclude='*.elf' --exclude='*.zip' \
  --exclude='*.iso' --exclude='*.img' --exclude='*.bin' \
  --exclude-dir=build --exclude-dir=.git .; then
  echo 'Branding check failed: old product name remains in project text.' >&2
  exit 1
fi
echo 'Fluxmax branding check: PASS'
