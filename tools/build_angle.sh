#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
revision=db5eb421c4350138e291220646de7764987a07e3
work="$root/build-angle"
mkdir -p "$work" "$root/build-deps/angle/lib" "$root/build-deps/angle/include"
if [ ! -d "$work/depot_tools" ]; then
  git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git "$work/depot_tools"
fi
export PATH="$work/depot_tools:$PATH"
export DEPOT_TOOLS_UPDATE=0
cd "$work"
cat > .gclient <<EOF
solutions = [{"name": "angle", "url": "https://chromium.googlesource.com/angle/angle.git@$revision", "managed": False, "custom_deps": {}, "custom_vars": {}}]
target_os = ["ios"]
EOF
gclient sync --no-history --revision "angle@$revision"
cd angle
mkdir -p out/ios
cat > out/ios/args.gn <<'EOF'
target_os = "ios"
target_cpu = "arm64"
target_environment = "device"
is_debug = false
is_component_build = false
symbol_level = 0
use_system_xcode = true
use_custom_libcxx = false
ios_enable_code_signing = false
angle_enable_metal = true
angle_enable_gl = false
angle_enable_vulkan = false
angle_build_tests = false
EOF
gn gen out/ios
ninja -C out/ios -j 3 angle_static
# Flatten all object files in this dedicated angle_static build, including GN
# source_set objects which are not included in the intermediate archives.
find "$PWD/out/ios/obj" -name '*.o' -type f | sort > "$work/objects.txt"
test -s "$work/objects.txt"
xcrun libtool -static -filelist "$work/objects.txt" -o "$root/build-deps/angle/lib/libANGLEComplete.a"
cp -R include/. "$root/build-deps/angle/include/"
printf '%s\n' "$revision" > "$root/build-deps/angle/revision.txt"
