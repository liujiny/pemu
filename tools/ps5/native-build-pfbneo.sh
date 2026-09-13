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
"$native_build" Folder
