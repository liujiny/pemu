#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
pemu_root=$(cd "$script_dir/../.." && pwd)
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
audit_patch="$pemu_root/patches/ps5/ps5-opengl-capability-audit.patch"
echo "PS5_OPENGL_UPSTREAM_COMMIT=$(git rev-parse HEAD)"
echo "PFBNEO_PS5_OPENGL_AUDIT_PATCH=YES"
if git apply --reverse --check "$audit_patch" >/dev/null 2>&1; then
    echo "PS5 OpenGL capability audit patch already applied"
else
    git apply --check "$audit_patch"
    git apply "$audit_patch"
fi
git diff --check
make source-fetch
make sdk
