#!/usr/bin/env bash
set -euo pipefail

stage=${1:?usage: native-build-pfbneo.sh <staging-root>}
root=$(cd -- "$stage" && pwd)
template_build="$root/tools/build.sh"
native_build="$root/tools/build-pfbneo.sh"
patch_file=${PEMU_SOURCE_ROOT:?}/patches/ps5/boilerplate-build-pfbneo.patch

test -f "$template_build"
test -f "$patch_file"
cp "$template_build" "$native_build"
patch --batch --forward -d "$root" -p1 < "$patch_file"
chmod +x "$native_build"
for patch_name in boilerplate-native-loader.patch boilerplate-native-plt.patch; do
    loader_patch="$PEMU_SOURCE_ROOT/patches/ps5/$patch_name"
    if patch --dry-run --batch --forward -d "$root" -p1 < "$loader_patch" >/dev/null 2>&1; then
        patch --batch --forward -d "$root" -p1 < "$loader_patch"
    elif ! patch --dry-run --batch --reverse -d "$root" -p1 < "$loader_patch" >/dev/null 2>&1; then
        echo "native converter does not match $patch_name" >&2
        exit 1
    fi
done
cp "$PEMU_SOURCE_ROOT/tools/ps5/native-app-crt.cpp" "$root/tooling/native/app_crt.cpp"
cp "$PEMU_SOURCE_ROOT/tools/ps5/native-crash-report.c" "$root/src/native_crash_report.c"
cp "$PEMU_SOURCE_ROOT/tools/ps5/native-directory.c" "$root/src/native_directory.c"
cp "$PEMU_SOURCE_ROOT/tools/ps5/native-launch-compat.c" "$root/src/native_launch_compat.c"
cp "$PEMU_SOURCE_ROOT/tools/ps5/native-app-heap.c" "$root/src/app_heap.c"
cp "$PEMU_SOURCE_ROOT/tools/ps5/native-input-probe.c" "$root/src/native_input_probe.c"
test -f "$root/data_romfs/skins/default/config.cfg"
test -f "$root/data_romfs/skins/default/default.ttf"
(cd "$root" && "$native_build" Folder)
python3 "$PEMU_SOURCE_ROOT/tools/ps5/verify-native-elf.py" \
    "$root/build/eboot.elf" --compare-llvm "$root/build/llvm-pie.elf"
