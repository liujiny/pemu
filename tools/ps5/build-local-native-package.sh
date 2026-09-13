#!/usr/bin/env bash
set -euo pipefail

pemu_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
dev_root=${PS5_DEV_ROOT:-"$HOME/ps5dev"}
template=${PS5_NATIVE_APP_TEMPLATE:-"$dev_root/ps5-native-app-boilerplate"}
sdk=${PS5_PAYLOAD_SDK:-"$template/.deps/native/ps5-payload-sdk"}
opengl=${PS5_OPENGL_PREFIX:-"$dev_root/ps5-opengl/build/sdk/ps5-opengl-core33"}
sdl=${PS5_SDL_PREFIX:-"$dev_root/ps5-opengl/build/native-sdl2/sdk"}
pemu_build=${PEMU_BUILD_ROOT:-"$dev_root/build/pemu"}
stage=${PS5_NATIVE_STAGE:-"$dev_root/build/native-pfbneo"}
title_id=${PS5_NATIVE_TITLE_ID:-PPSA99998}
homebrew="$sdk/target/user/homebrew"
llvm_ar=${LLVM_AR:-$(command -v llvm-ar-21 || command -v llvm-ar)}
host_tools="$dev_root/host-tools/usr/bin"
[[ ! -d $host_tools ]] || export PATH="$host_tools:$PATH"

test -x "$template/tooling/prospero-clang18"
test -x "$sdk/bin/prospero-lld"
test -f "$opengl/lib/libPS5OpenGLCore33.a"
test -f "$sdl/lib/libSDL2.a"
test -n "$llvm_ar"

mkdir -p "$stage"
for directory in sce_sys tooling runtime tools .deps; do
    test -d "$stage/$directory" || cp -a "$template/$directory" "$stage/"
done
test -f "$stage/Makefile" || cp "$template/Makefile" "$stage/Makefile"
mkdir -p "$stage/.deps/native/zlib"
zlib_source_archive="$dev_root/cache/native-deps/zlib-1.3.2.tar.gz"
test -f "$zlib_source_archive"
cp "$zlib_source_archive" "$stage/.deps/native/zlib/zlib-1.3.2.tar.gz"
mkdir -p "$stage/src" "$stage/.local/lib" "$stage/assets"
printf '%s\n' 'extern "C" void pfbneo_native_link_anchor() {}' > "$stage/src/native_entry_stub.cpp"
cp "$dev_root/ps5-opengl/native-app/app_heap.c" "$stage/src/app_heap.c"
cp "$dev_root/ps5-opengl/native-app/runtime_shims.c" "$stage/src/runtime_shims.c"
cp "$pemu_root/tools/ps5/native-runtime-compat.c" "$stage/src/native_runtime_compat.c"
cp "$template/tooling/native/ps5-pie.ld" "$stage/tooling/native/ps5-pie-base.ld"
cp "$dev_root/ps5-opengl/native-app/ps5-pie.ld" "$stage/tooling/native/ps5-pie.ld"
cp "$dev_root/ps5-opengl/native-app/app-symbols.map" "$stage/tooling/native/app-symbols.map"

copy_archive() {
    local source=$1
    test -f "$source"
    cp "$source" "$stage/.local/lib/"
}

copy_archive "$pemu_build/src/cores/pfbneo/libpfbneo_ps5_native.a"
copy_archive "$pemu_build/src/skeleton/libcross2dui.a"
copy_archive "$pemu_build/external/libcross2d/libcross2d.a"
copy_archive "$pemu_build/external/sscrap/libsscrap.a"
copy_archive "$sdl/lib/libSDL2.a"
for archive in "$opengl"/lib/*.a; do
    copy_archive "$archive"
done
for archive in libc++.a libc++abi.a libunwind.a; do
    copy_archive "$sdk/target/lib/$archive"
done
copy_archive "$(clang-18 --print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a"
for archive in libpng16.a libfreetype.a libcurl.a libtinyxml2.a libssl.a libcrypto.a libz.a; do
    copy_archive "$homebrew/lib/$archive"
done
for shared in libSceAgc.so libSceAgcDriver.so; do
    cp "$opengl/lib/$shared" "$stage/.local/lib/$shared"
    cp "$opengl/lib/$shared" "$stage/.deps/native/ps5-payload-sdk/target/lib/$shared"
done

for data_dir in \
    "$pemu_build/src/cores/pfbneo/data_datadir" \
    "$pemu_build/src/cores/pfbneo/data_romfs"; do
    test ! -d "$data_dir" || cp -a "$data_dir/." "$stage/assets/"
done

archives=(
    .local/lib/libpfbneo_search.a
    .local/lib/libpfbneo_ps5_native.a
    .local/lib/libcross2dui.a
    .local/lib/libcross2d.a
    .local/lib/libsscrap.a
    .local/lib/libSDL2.a
    .local/lib/libPS5OpenGLCore33.a
    .local/lib/libpng16.a
    .local/lib/libfreetype.a
    .local/lib/libcurl.a
    .local/lib/libtinyxml2.a
    .local/lib/libssl.a
    .local/lib/libcrypto.a
    .local/lib/libz.a
    .local/lib/libunwind.a
    .local/lib/libc++abi.a
    .local/lib/libc++.a
    .local/lib/libclang_rt.builtins-x86_64.a
)
printf 'SEARCH_DIR("%s")\nSEARCH_DIR("%s")\n' \
    "$sdk/target/lib" "$stage/.local/lib" \
    > "$stage/.local/lib/libpfbneo_search.a"
printf '%s\n' "${archives[*]}" > "$stage/.local/native-archives.txt"

for archive in "${archives[@]}"; do
    file "$stage/$archive"
    if grep -Eq '^(GROUP \(|SEARCH_DIR\()' "$stage/$archive"; then
        :
    else
        "$llvm_ar" -t "$stage/$archive" >/dev/null
    fi
done

(
    cd "$stage"
    make doctor
    make init TITLE_ID="$title_id" APP_NAME="pFBNeo PS5 GPU Test"
    if ! (cd runtime && sha256sum --check --strict libc.prx.sha256); then
        make libc
    fi
    APP_STATIC_ARCHIVES="${archives[*]}" \
    APP_SHARED_INPUTS='.local/lib/libSceAgc.so .local/lib/libSceAgcDriver.so' \
    APP_FORCE_SYMBOLS=ps5_agc_gate2_run \
    PEMU_SOURCE_ROOT="$pemu_root" \
        "$pemu_root/tools/ps5/native-build-pfbneo.sh" "$stage"
)

tool="$stage/build/host/ps5-native-tool"
app="$stage/dist/$title_id"
test -f "$app/eboot.bin"
test -f "$app/sce_sys/param.json"
test -f "$app/sce_module/libc.prx"
test -d "$app/assets"
"$tool" self --inspect --file "$app/eboot.bin" > "$stage/ps5-native-inspect.txt"
"$tool" self --inspect --file "$app/sce_module/libc.prx" >> "$stage/ps5-native-inspect.txt"
printf '[PS5 LOCAL] native package=%s\n' "$app"
