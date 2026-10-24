#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
source="${1:-$root/deps/luajit}"
output="$root/build-gluavm/ios-arm64"
python3 "$root/gluavm/patch_luajit.py" "$source"
export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.0}"
sdk="$(xcrun --sdk iphoneos --show-sdk-path)"
cc="$(xcrun --sdk iphoneos --find clang)"
make -C "$source" clean TARGET_SYS=iOS
make -C "$source" -j3 DEFAULT_CC=clang HOST_CC="clang -m64" CROSS="$(dirname "$cc")/" \
  TARGET_SYS=iOS BUILDMODE=static TARGET_FLAGS="-arch arm64 -isysroot $sdk -miphoneos-version-min=15.0" \
  XCFLAGS="-DLUAJIT_DISABLE_JIT -DLUAJIT_DISABLE_FFI"
mkdir -p "$output/include"
cp "$source/src/libluajit.a" "$output/libgluavm.a"
for header in lua.h lua.hpp luaconf.h lualib.h lauxlib.h luajit.h; do cp "$source/src/$header" "$output/include/"; done
xcrun lipo -info "$output/libgluavm.a"
xcrun lipo -verify_arch arm64 "$output/libgluavm.a"
