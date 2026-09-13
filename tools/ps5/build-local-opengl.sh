#!/usr/bin/env bash
set -euo pipefail

dev_root=${PS5DEV_ROOT:-"$HOME/ps5dev"}
template="$dev_root/ps5-native-app-boilerplate"
opengl="$dev_root/ps5-opengl"
python_root="$dev_root/python-tools"
export PS5_NATIVE_APP_TEMPLATE="$template"
export PS5_PAYLOAD_SDK="$template/.deps/native/ps5-payload-sdk"
export LLVM_CONFIG=/usr/bin/llvm-config-21
export PSBC_JOBS=2
export PATH="$python_root/bin:$PATH"

test -x "$PS5_PAYLOAD_SDK/bin/prospero-clang"
test -x "$PS5_PAYLOAD_SDK/bin/prospero-lld"
test -d "$PS5_PAYLOAD_SDK/target/include"
python3 -c "import mako, yaml, packaging; print('python deps OK')"
meson --version
llvm-config-21 --version
clang-18 --version
clang-21 --version
glslangValidator --version
spirv-as --version
bison --version
flex --version
"$PS5_PAYLOAD_SDK/bin/prospero-clang" --version
"$PS5_PAYLOAD_SDK/bin/prospero-lld" --version

cd "$opengl"
test "$(git rev-parse HEAD)" = 7f9bfabdddb187a11e4401058eba8c9e55194d0a
make source-fetch
make sdk
