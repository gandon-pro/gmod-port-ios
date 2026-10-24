#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
engine="${1:?engine checkout required}"
out="$root/build-isolation-test"
mkdir -p "$out"
arch="$(uname -m)"
flags=(-std=c++17 -arch "$arch" -mmacosx-version-min=11.0 -DPOSIX=1 -DOSX=1 -DGNUC=1
       -I"$engine/public" -I"$engine/public/tier0")
cat > "$out/helper.cpp" <<'CPP'
extern int moduleValue;
int SharedHelper() { return moduleValue; }
CPP
cat > "$out/registration.cpp" <<'CPP'
#include "tier1/interface.h"
int moduleValue = FACTORY_VALUE;
int SharedHelper();
static void* InstantiateTest() {
    static int result;
    result = SharedHelper();
    return &result;
}
EXPOSE_INTERFACE_FN(InstantiateTest, SharedTestInterface, "SharedTestInterface001")
CPP
xcrun clang++ "${flags[@]}" -c "$out/helper.cpp" -o "$out/helper.o"
xcrun ar rcs "$out/helper.a" "$out/helper.o"
for module in engine client; do
    value=11
    [[ "$module" == client ]] && value=22
    define="-DCreateInterface=gp_test_${module}_CreateInterface"
    xcrun clang++ "${flags[@]}" "$define" -DFACTORY_VALUE="$value" -c "$out/registration.cpp" -o "$out/${module}_registration.o"
    xcrun clang++ "${flags[@]}" "$define" -x c++ -c "$root/platform/module_factory.cpp.in" -o "$out/${module}_factory.o"
    xcrun ar rcs "$out/$module.a" "$out/${module}_registration.o" "$out/${module}_factory.o"
    python3 "$root/tools/isolate_module.py" --archive "$out/$module.a" --helper "$out/helper.a" \
        --export "gp_test_${module}_CreateInterface" --output "$out/${module}_isolated.o" \
        --sdk macosx --arch "$arch" --minimum 11.0
done
# Reuse the real Source loader/registry assertions, including aliasing and
# unload/refcount checks. Both modules contain identically named globals,
# InterfaceReg lists and SharedHelper; the final link must reject no symbols.
xcrun clang++ "${flags[@]}" -I"$root/platform" "$root/tests/source_loader_test.cpp" \
    "$root/platform/source_loader.cpp" "$root/platform/static_modules.cpp" \
    "$out/engine_isolated.o" "$out/client_isolated.o" -o "$out/verify"
"$out/verify"
echo 'Source factories, shared helper state and module isolation: passed'
