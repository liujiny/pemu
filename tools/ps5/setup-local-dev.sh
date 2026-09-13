#!/usr/bin/env bash
set -euo pipefail

dev_root=${PS5DEV_ROOT:-"$HOME/ps5dev"}
template="$dev_root/ps5-native-app-boilerplate"
opengl="$dev_root/ps5-opengl"
python_root="$dev_root/python-tools"
cache="$dev_root/cache"
template_commit=722f2227a8bb6fa2229120546995b6562552c752
opengl_commit=7f9bfabdddb187a11e4401058eba8c9e55194d0a
payload_url=https://github.com/ps5-payload-dev/sdk/releases/download/v0.42/ps5-payload-sdk.zip
payload_sha=8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da

mkdir -p "$dev_root" "$cache"

if [[ ! -d "$template/.git" ]]; then
    git clone https://github.com/blackbearreloaded/ps5-native-app-boilerplate.git "$template"
fi
git -C "$template" fetch --depth=1 origin "$template_commit"
git -C "$template" checkout "$template_commit"
test "$(git -C "$template" rev-parse HEAD)" = "$template_commit"

payload_zip="$cache/ps5-payload-sdk-v0.42.zip"
if [[ ! -f "$payload_zip" ]]; then
    curl --fail --location --retry 3 -o "$payload_zip" "$payload_url"
fi
echo "$payload_sha  $payload_zip" | sha256sum -c -
mkdir -p "$template/.deps/native"
sdk="$template/.deps/native/ps5-payload-sdk"
if [[ ! -x "$sdk/bin/prospero-lld" || ! -d "$sdk/target/include" ]]; then
    unzip -q "$payload_zip" -d "$template/.deps/native"
fi
test -x "$sdk/bin/prospero-lld"
test -d "$sdk/target/include"

if [[ ! -d "$opengl/.git" ]]; then
    git clone https://github.com/blackbearreloaded/ps5-opengl.git "$opengl"
fi
git -C "$opengl" fetch --depth=1 origin "$opengl_commit"
git -C "$opengl" checkout "$opengl_commit"
test "$(git -C "$opengl" rev-parse HEAD)" = "$opengl_commit"

python3 -m venv "$python_root"
"$python_root/bin/pip" install meson==1.10.1 Mako==1.3.10 PyYAML==6.0.2 packaging==25.0

cat <<EOF
PS5_NATIVE_APP_TEMPLATE=$template
PS5_PAYLOAD_SDK=$sdk
PS5_OPENGL_ROOT=$opengl
PS5_PYTHON_TOOLS=$python_root
EOF
