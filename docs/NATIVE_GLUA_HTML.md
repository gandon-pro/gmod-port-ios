# Native GLua / HTML integration status

This change connects the pinned Lua interpreter to client, server and menu
lifecycles. It is an integration foundation, not a complete Garry's Mod API port.
The original Q-menu, Tool Gun Lua rendering and HTML main menu are **not working
yet**. The existing C++ sandbox remains active when original Lua bootstrap fails.

## Revisions

- Source: `ed8209cc35c61fbd8ddff8480962a01c981eef2f`
- LuaJIT: `a4c064539ea931274d407e1249b4bb575424a0f4`
- Facepunch Lua: `ad8023410600199ad9a12444bc496b9d65226a82`

Pins are recorded in `dependencies.lock.json`. Source files are copied into the
disposable engine checkout by `tools/patch_engine.py`. GLUAVM is linked once into
the app; runtime and registration state remain private to each isolated module.

## Implemented

- Native interpreter, JIT and FFI disabled; GLua lexer extensions use the existing
  pinned VM patch, without rewriting game scripts.
- Separate client/server/menu states, native registration, Source filesystem
  includes, include return values, error reporting and balanced Lua stacks.
- Think/InitPostEntity/ShutDown lifecycle hooks when bootstrap succeeds.
- Native WKWebView transport: local `asset://garrysmod/` loading, bounded JSON
  message queue, JavaScript function registration, visibility, bounds and cleanup.
  UIKit work is dispatched to its main queue. Lua callbacks must be dispatched on
  the state-owning game thread by the future DHTML binding, never from WebKit.
- iPhone/iPad icon files, plist entries, packaging validation.

`platform/html/html_panel.h` is only a C transport API. It does not implement
`vgui.Create("DHTML")` or register original menu callbacks. No original HTML menu
is shown until those bindings exist. Remote page navigation is not implemented.

## Reproduce host checks (Linux)

```sh
python3 tools/bootstrap.py --only luajit --only garrysmod
python3 gluavm/patch_luajit.py deps/luajit
make -C deps/luajit -j2 BUILDMODE=static XCFLAGS='-DLUAJIT_DISABLE_JIT -DLUAJIT_DISABLE_FFI'
cmake -S . -B build-native-host -DGMOD_BUILD_ENGINE=OFF -DGMOD_BUILD_GLUA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native-host --parallel 2
ctest --test-dir build-native-host --output-on-failure
python3 tests/glua/test_icon_package.py
build-native-host/glua_bootstrap_probe deps/garrysmod/garrysmod
```

Observed locally: four CTest tests pass; icon validation passes. The final probe
exits **1**, reporting the actual error below in client (1), server (2), menu (4):

```text
includes/util/color.lua:8: attempt to call global 'RegisterMetaTable' (a nil value)
```

This probe executes the original `includes/init.lua`; it is not a syntax check.
The CI preflight uploads its JSONL report separately from the passing runtime
unit tests. A green preflight does not mean original bootstrap succeeded.

## iOS checks

The `Native GLua and HTML preflight` workflow builds and validates the iPhone
archive, checks Objective-C++ with the iPhone SDK, and runs the host tests. The
G1 workflow builds the full engine/app and validates icons while packaging IPA.
Neither check demonstrates behavior on an iPhone. Full engine linking and device
runtime must be checked separately. No Xcode/iOS SDK is available in the Linux
workspace used for these edits.

Runtime diagnostics: `gmp_glua_client_status`, `gmp_glua_server_status`,
`gmp_glua_menu_status`.

## Missing integration

`docs/GPT_TASK_GLUA.md` names `glua_runtime`, `glua_entity`, `glua_render`,
`glua_vgui`, `glua_types` and an API specification. Only the `glua_api.h` contract
was present before this change. The old engine bindings and specification are
not present in this checkout; this change supplies runtime ownership, not those
bindings. Remaining work includes metatable/value types, entity lifetime,
engine libraries, VGUI panel ownership/input/rendering, DHTML Lua callback
registration/dispatch, and bootstrapping the original gamemode/menu.

Do not replace missing APIs with silent stubs: continue the original bootstrap
probe after implementing each compatible binding, then validate actual menu,
spawn and Tool Gun behavior on the pinned engine build.
