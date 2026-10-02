#!/usr/bin/env bash
# Restore tested PS5 sources on the pinned submodules without moving gitlinks.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
apply_patch() {
    local checkout=$1 patch_file=$2
    if git -C "$checkout" apply --reverse --check "$patch_file" >/dev/null 2>&1; then
        echo "Already applied: ${patch_file##*/}"
    else
        git -C "$checkout" apply --check --whitespace=nowarn "$patch_file"
        git -C "$checkout" apply --whitespace=nowarn "$patch_file"
    fi
}
apply_patch "$root/external/libcross2d" "$root/patches/ps5/libcross2d-ps5-opengl.patch"
apply_patch "$root/external/cores/FBNeo" "$root/patches/ps5/fbneo-ps5-native-video-owner.patch"
# FBNeo state.cpp keeps its original CRLF line endings.
git -C "$root/external/libcross2d" diff --check
git -C "$root/external/cores/FBNeo" -c core.whitespace=cr-at-eol diff --check
