#!/usr/bin/env bash
set -euo pipefail

dev_root=${PS5_DEV_ROOT:-"$HOME/ps5dev"}
template=${PS5_NATIVE_APP_TEMPLATE:-"$dev_root/ps5-native-app-boilerplate"}
export PS5_PAYLOAD_SDK=${PS5_PAYLOAD_SDK:-"$template/.deps/native/ps5-payload-sdk"}
cache="$dev_root/cache/native-deps"
work="$dev_root/build/native-deps"
prefix="$PS5_PAYLOAD_SDK/target/user/homebrew"
jobs=${PS5_DEPS_JOBS:-$(nproc)}

test -x "$PS5_PAYLOAD_SDK/bin/prospero-clang"
mkdir -p "$cache" "$work" "$prefix/include" "$prefix/lib/pkgconfig"
source "$PS5_PAYLOAD_SDK/toolchain/prospero.sh"
export CPPFLAGS="-I$prefix/include ${CPPFLAGS:-}"
export LDFLAGS="-L$prefix/lib ${LDFLAGS:-}"
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig:$prefix/share/pkgconfig"
export PKG_CONFIG_PATH=

fetch() {
    local name=$1 url=$2 hash=$3
    local archive="$cache/$name"
    if [[ ! -f $archive ]] || ! printf '%s  %s\n' "$hash" "$archive" | sha256sum -c --status; then
        curl -L --fail --retry 3 -o "$archive.download" "$url" || return
        printf '%s  %s\n' "$hash" "$archive.download" | sha256sum -c - >&2
        mv "$archive.download" "$archive"
    fi
    printf '%s\n' "$archive"
}

extract() {
    local archive=$1 directory=$2
    if [[ ! -d $directory ]]; then
        mkdir -p "$directory.tmp"
        tar -xf "$archive" -C "$directory.tmp" --strip-components=1
        mv "$directory.tmp" "$directory"
    fi
}

build_zlib() {
    local stamp="$work/zlib-1.3.2.installed" src="$work/zlib-1.3.2"
    [[ -f $stamp ]] && return
    extract "$(fetch zlib-1.3.2.tar.gz https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16)" "$src"
    (cd "$src"; CHOST=x86_64-pc-freebsd ./configure --prefix=/user/homebrew --static; make -j"$jobs"; make DESTDIR="$PS5_PAYLOAD_SDK/target" install)
    touch "$stamp"
}

build_png() {
    local stamp="$work/libpng-1.6.43.installed" src="$work/libpng-1.6.43"
    [[ -f $stamp ]] && return
    extract "$(fetch libpng-1.6.43.tar.xz https://download.sourceforge.net/libpng/libpng-1.6.43.tar.xz 6a5ca0652392a2d7c9db2ae5b40210843c0bbc081cbd410825ab00cc59f14a6c)" "$src"
    (cd "$src"; ./configure --prefix=/user/homebrew --host=x86_64-pc-freebsd --enable-static --disable-shared --disable-tests; make -j"$jobs"; make DESTDIR="$PS5_PAYLOAD_SDK/target" install)
    touch "$stamp"
}

build_freetype() {
    local stamp="$work/freetype-2.13.2.installed" src="$work/freetype-2.13.2"
    [[ -f $stamp ]] && return
    extract "$(fetch freetype-2.13.2.tar.gz https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.gz 1ac27e16c134a7f2ccea177faba19801131116fd682efc1f5737037c5db224b5)" "$src"
    (cd "$src"; ./configure --prefix=/user/homebrew --host=x86_64-pc-freebsd --enable-static --disable-shared --with-zlib=yes --with-bzip2=no --with-png=yes --with-harfbuzz=no; make -j"$jobs"; make DESTDIR="$PS5_PAYLOAD_SDK/target" install)
    touch "$stamp"
}

build_tinyxml2() {
    local stamp="$work/tinyxml2-10.0.0.installed" src="$work/tinyxml2-10.0.0"
    [[ -f $stamp ]] && return
    extract "$(fetch tinyxml2-10.0.0.tar.gz https://github.com/leethomason/tinyxml2/archive/10.0.0.tar.gz 3bdf15128ba16686e69bce256cc468e76c7b94ff2c7f391cc5ec09e40bff3839)" "$src"
    "$CMAKE" -S "$src" -B "$src/build-ps5" -DBUILD_SHARED_LIBS=OFF -Dtinyxml2_BUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/user/homebrew
    cmake --build "$src/build-ps5" -j"$jobs"
    DESTDIR="$PS5_PAYLOAD_SDK/target" cmake --install "$src/build-ps5"
    touch "$stamp"
}

build_openssl() {
    local stamp="$work/openssl-3.5.2.installed" src="$work/openssl-3.5.2"
    [[ -f $stamp ]] && return
    extract "$(fetch openssl-3.5.2.tar.gz https://github.com/openssl/openssl/releases/download/openssl-3.5.2/openssl-3.5.2.tar.gz c53a47e5e441c930c3928cf7bf6fb00e5d129b630e0aa873b08258656e7345ec)" "$src"
    (cd "$src"; ./Configure BSD-x86_64 no-tests no-apps no-shared no-dgram no-dso no-async --prefix=/user/homebrew; make -j"$jobs" build_sw; make DESTDIR="$PS5_PAYLOAD_SDK/target" install_sw)
    touch "$stamp"
}

build_curl() {
    local stamp="$work/curl-8.18.0.installed" src="$work/curl-8.18.0"
    [[ -f $stamp ]] && return
    extract "$(fetch curl-8.18.0.tar.xz https://curl.se/download/curl-8.18.0.tar.xz 40df79166e74aa20149365e11ee4c798a46ad57c34e4f68fd13100e2c9a91946)" "$src"
    (cd "$src"; ./configure --prefix=/user/homebrew --host=x86_64-pc-freebsd --enable-static --disable-shared --with-openssl="$prefix" --with-zlib="$prefix" --without-libpsl --without-zstd --disable-docs --disable-ipv6 --disable-socketpair --disable-netrc; make -j"$jobs"; make DESTDIR="$PS5_PAYLOAD_SDK/target" install)
    touch "$stamp"
}

build_zlib
build_png
build_freetype
build_tinyxml2
build_openssl
build_curl

# glm is header-only, but the PS5 cross compiler cannot consume the host
# /usr/include tree directly. Mirror the distro package into this SDK prefix.
glm_source=$(dpkg -L libglm-dev | awk '/\/glm\/glm\.hpp$/ {sub(/\/glm\/glm\.hpp$/, ""); print; exit}')
test -n "$glm_source"
test -f "$glm_source/glm/glm.hpp"
if [[ ! -f $prefix/include/glm/glm.hpp ]]; then
    cp -a "$glm_source/glm" "$prefix/include/"
fi
test -f "$prefix/include/glm/glm.hpp"

for module in zlib libpng freetype2 tinyxml2 libcurl; do
    PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig:$prefix/share/pkgconfig" "$PKG_CONFIG" --exists "$module"
done
printf '[PS5 LOCAL] native dependency prefix=%s\n' "$prefix"
