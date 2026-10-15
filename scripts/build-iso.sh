#!/usr/bin/env bash
set -euo pipefail
: "${LIMINE_DIR:?Set LIMINE_DIR to the extracted official Limine binary bundle}"
ISO_OUT="${ISO_OUT:-build/Fluxmax-x86_64.iso}"
ROOT="build/iso-root"

fail() { echo "ISO BUILD ERROR: $*" >&2; exit "${2:-3}"; }

# Do not mix files found independently across a directory tree. All Limine
# boot images/firmware applications must come from the same release directory.
BUNDLE_DIR=""
while IFS= read -r sys_file; do
  candidate="$(dirname "$sys_file")"
  if [[ -s "$candidate/limine-bios-cd.bin" && \
        -s "$candidate/limine-uefi-cd.bin" && \
        -s "$candidate/BOOTX64.EFI" ]]; then
    BUNDLE_DIR="$candidate"
    break
  fi
done < <(find "$LIMINE_DIR" -type f -name limine-bios.sys -print | sort)

[[ -n "$BUNDLE_DIR" ]] || fail "No complete, consistent Limine bundle found under $LIMINE_DIR. Expected limine-bios.sys, limine-bios-cd.bin, limine-uefi-cd.bin and BOOTX64.EFI in the same directory."
BIOS_SYS="$BUNDLE_DIR/limine-bios.sys"
BIOS_CD="$BUNDLE_DIR/limine-bios-cd.bin"
UEFI_CD="$BUNDLE_DIR/limine-uefi-cd.bin"
EFI_APP="$BUNDLE_DIR/BOOTX64.EFI"

find_limine_cli() {
  find "$LIMINE_DIR" -type f -name limine -perm /111 -print -quit
}
LIMINE_CLI="$(find_limine_cli)"
if [[ -z "$LIMINE_CLI" ]]; then
  # Release archives usually put the host utility Makefile alongside the
  # boot files. If it is one directory above, use that instead.
  BUILD_DIR="$BUNDLE_DIR"
  if [[ ! -f "$BUILD_DIR/Makefile" && ! -f "$BUILD_DIR/GNUmakefile" && ! -f "$BUILD_DIR/makefile" ]]; then
    for parent in "$(dirname "$BUNDLE_DIR")" "$LIMINE_DIR"; do
      if [[ -f "$parent/Makefile" || -f "$parent/GNUmakefile" || -f "$parent/makefile" ]]; then
        BUILD_DIR="$parent"
        break
      fi
    done
  fi
  [[ -f "$BUILD_DIR/Makefile" || -f "$BUILD_DIR/GNUmakefile" || -f "$BUILD_DIR/makefile" ]] || \
    fail "Limine host utility is missing and no Makefile was found. Bundle directory: $BUNDLE_DIR"
  echo "Building Limine host utility in: $BUILD_DIR"
  make -C "$BUILD_DIR"
  LIMINE_CLI="$(find_limine_cli)"
fi
[[ -n "$LIMINE_CLI" && -x "$LIMINE_CLI" ]] || fail "Limine host executable was not produced. Searched under $LIMINE_DIR"

[[ -s build/kernel.elf ]] || fail 'build/kernel.elf is missing; run make first.' 4
[[ -s limine.conf ]] || fail 'limine.conf is missing from repository root.' 5
bash scripts/check-limine-config.sh limine.conf
python3 scripts/check-kernel-elf.py build/kernel.elf

# Ensure entrypoint and Limine request section are actually present in this ELF.
entry="$(readelf -h build/kernel.elf | awk '/Entry point address:/ {print $4}')"
start="$(nm -n build/kernel.elf | awk '$3 == "_start" {print $1}')"
[[ -n "$entry" && -n "$start" ]] || fail 'Kernel ELF entry or _start symbol is missing.' 8
[[ "$entry" == "0x$start" ]] || fail "ELF entry $entry does not equal _start 0x$start." 8
readelf -S build/kernel.elf | grep -q '\.limine_requests' || fail 'Kernel ELF has no .limine_requests section.' 8

mkdir -p "$ROOT/boot/limine" "$ROOT/EFI/BOOT" "$ROOT/boot" "$ROOT/limine" "$(dirname "$ISO_OUT")"
rm -f "$ROOT/boot/kernel.elf" \
  "$ROOT/boot/limine/limine-bios.sys" "$ROOT/boot/limine/limine-bios-cd.bin" \
  "$ROOT/boot/limine/limine-uefi-cd.bin" "$ROOT/EFI/BOOT/BOOTX64.EFI" \
  "$ROOT/limine.conf" "$ROOT/boot/limine/limine.conf" "$ROOT/boot/limine.conf" \
  "$ROOT/limine/limine.conf" "$ROOT/EFI/BOOT/limine.conf"
cp build/kernel.elf "$ROOT/boot/kernel.elf"
# Provide all official Limine config search locations so BIOS/UEFI don't depend
# on one specific filesystem path. These are copies of the same checked file.
for config_dest in \
  "$ROOT/boot/limine/limine.conf" \
  "$ROOT/boot/limine.conf" \
  "$ROOT/limine/limine.conf" \
  "$ROOT/limine.conf" \
  "$ROOT/EFI/BOOT/limine.conf"; do
  cp limine.conf "$config_dest"
done
cp "$BIOS_SYS" "$ROOT/boot/limine/limine-bios.sys"
cp "$BIOS_CD" "$ROOT/boot/limine/limine-bios-cd.bin"
cp "$UEFI_CD" "$ROOT/boot/limine/limine-uefi-cd.bin"
cp "$EFI_APP" "$ROOT/EFI/BOOT/BOOTX64.EFI"

# Fail loudly if file copies or paths don't match the config's kernel target.
test -s "$ROOT/boot/kernel.elf" || fail 'Kernel was not copied into ISO tree.' 9
for cfg in "$ROOT/boot/limine/limine.conf" "$ROOT/boot/limine.conf" "$ROOT/limine/limine.conf" "$ROOT/limine.conf" "$ROOT/EFI/BOOT/limine.conf"; do
  cmp -s limine.conf "$cfg" || fail "Configuration copy differs from repository config: $cfg" 9
done
chmod +x "$LIMINE_CLI"

# Use the exact, same-directory boot images identified above.
xorriso -as mkisofs -R -r -J \
  -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
  -hfsplus -apm-block-size 2048 \
  --efi-boot boot/limine/limine-uefi-cd.bin --efi-boot-part --efi-boot-image --protective-msdos-label \
  "$ROOT" -o "$ISO_OUT"
"$LIMINE_CLI" bios-install "$ISO_OUT"

# Validate the resulting ISO, not just the input directory.
tmp_cfg="$(mktemp)"
trap 'rm -f "$tmp_cfg"' EXIT
cfg_found=0
for iso_path in /boot/limine/limine.conf /limine.conf /EFI/BOOT/limine.conf; do
  if xorriso -osirrox on -indev "$ISO_OUT" -extract "$iso_path" "$tmp_cfg" >/dev/null 2>&1; then
    cmp -s limine.conf "$tmp_cfg" || fail "ISO configuration at $iso_path is missing/stale." 6
    cfg_found=1
    break
  fi
done
(( cfg_found == 1 )) || fail 'Could not extract any Limine config from the generated ISO.' 6
kernel_tmp="$(mktemp)"
if ! xorriso -osirrox on -indev "$ISO_OUT" -extract /boot/kernel.elf "$kernel_tmp" >/dev/null 2>&1; then
  rm -f "$kernel_tmp"
  fail 'Could not extract /boot/kernel.elf from the generated ISO.' 6
fi
cmp -s build/kernel.elf "$kernel_tmp" || { rm -f "$kernel_tmp"; fail 'Embedded kernel differs from build/kernel.elf.' 6; }
rm -f "$kernel_tmp"
xorriso -indev "$ISO_OUT" -report_el_torito plain 2>&1 | tee "${ISO_OUT}.eltorito.txt"
grep -qi 'El Torito boot img' "${ISO_OUT}.eltorito.txt" || fail 'No bootable El Torito entry reported in ISO.' 7
printf 'Created ISO: %s\n' "$ISO_OUT"
printf 'Limine bundle: %s\n' "$BUNDLE_DIR"
printf 'Limine host tool: %s\n' "$LIMINE_CLI"
printf 'Kernel entry: %s\n' "$entry"
