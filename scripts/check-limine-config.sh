#!/usr/bin/env bash
set -euo pipefail
cfg="${1:-limine.conf}"
test -s "$cfg" || { echo "CONFIG ERROR: Missing Limine config: $cfg" >&2; exit 1; }
# A small finite delay leaves the menu visible during smoke tests but still
# autoboots. timeout=0 hid useful evidence when the kernel never took control.
timeout_value="$(awk -F: '/^[[:space:]]*timeout:/ {gsub(/[[:space:]]/, "", $2); print $2; exit}' "$cfg")"
[[ "$timeout_value" =~ ^[0-9]+$ ]] && (( timeout_value >= 1 && timeout_value <= 10 )) || {
  echo "CONFIG ERROR: timeout must be an integer from 1 to 10 seconds for observable autoboot (found '${timeout_value:-missing}')." >&2; exit 1;
}
grep -Eiq '^default_entry:[[:space:]]*1([[:space:]]|$)' "$cfg" || { echo 'CONFIG ERROR: default_entry must select entry 1.' >&2; exit 1; }
grep -Eq '^/[^[:space:]].*$' "$cfg" || { echo 'CONFIG ERROR: Limine menu entry is missing (expected /Entry title syntax).' >&2; exit 1; }
grep -Eiq '^[[:space:]]+protocol:[[:space:]]*limine[[:space:]]*$' "$cfg" || { echo 'CONFIG ERROR: Limine protocol entry is missing.' >&2; exit 1; }
grep -Eiq '^[[:space:]]+path:[[:space:]]*boot\(\):/boot/kernel\.elf[[:space:]]*$' "$cfg" || { echo 'CONFIG ERROR: Kernel path is missing or unexpected.' >&2; exit 1; }
grep -Eiq '^serial:[[:space:]]*yes[[:space:]]*$' "$cfg" || { echo 'CONFIG ERROR: serial: yes is required so Limine diagnostics reach COM1.' >&2; exit 1; }
grep -Eiq '^verbose:[[:space:]]*yes[[:space:]]*$' "$cfg" || { echo 'CONFIG ERROR: verbose: yes is required during development boot.' >&2; exit 1; }
grep -Eiq '^graphics:[[:space:]]*yes[[:space:]]*$' "$cfg" || { echo 'CONFIG ERROR: graphics: yes is required for the framebuffer desktop.' >&2; exit 1; }
grep -Eiq '^[[:space:]]+resolution:[[:space:]]*[0-9]+x[0-9]+x32[[:space:]]*$' "$cfg" || { echo 'CONFIG ERROR: the Limine kernel entry must request a 32-bit graphical resolution.' >&2; exit 1; }
echo "Limine configuration checks passed (timeout=${timeout_value}s, default entry 1, serial+verbose+graphics enabled)."
