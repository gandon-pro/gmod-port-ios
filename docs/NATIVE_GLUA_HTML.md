# Native GLua and original main menu

The port now executes Facepunch's unchanged `menu/mainmenu.lua`, `menu/getmaps.lua`
and their helper scripts through a **menu-only** binding layer. The original
`html/menu.html`, Angular controllers, templates and CSS are loaded from installed
game content. No HTML/CSS imitation or upstream Lua rewrite is used.

This is not a complete game-realm GLua/VGUI implementation. The original Q-menu
and Tool Gun Lua renderer remain separate unfinished work. Steam server queries,
Workshop, saves, dupes, demos, other gamemodes and auxiliary Lua dialogs are not
implemented by this engine build; attempting an unavailable API reports an error.
A visible original menu does not establish compatibility for those pages.

## Revisions

- Source: `ed8209cc35c61fbd8ddff8480962a01c981eef2f`
- LuaJIT: `a4c064539ea931274d407e1249b4bb575424a0f4`
- Facepunch Lua/HTML: `ad8023410600199ad9a12444bc496b9d65226a82`
- Browser smoke API: Playwright `1.62.1` (development dependency only).

Dependencies are pinned in `dependencies.lock.json` and `tests/menu/package.json`.
The browser test records its actual Chrome version. Game assets are not bundled;
copy the game folders as described in the main README. Missing `html/menu.html`
is detected at startup and leaves the existing native menu available.

## Connected behavior

- Main menu, original map categories/search, map selection and favourites.
- Original New Game code sends validated console arguments to the Source engine;
  map commands require an installed BSP. Only the native sandbox is supported.
- Options route to the existing Source options dialog; HTML hides while it is
  visible. Resume/hide follows GameUI visibility; loading hides the WebKit view.
- Localized labels read original `main_menu.properties` with English fallback.
  Language and favourites/last map persist through real engine/filesystem writes.
- Menu panel ownership, resize, queued JavaScript, callback result values and
  teardown. Lua calls stay on the engine thread; UIKit runs on its main queue.
- Original background.lua animation feeds an image/gradient layer in WebKit.
  Dimensions come from ImageIO, thumbnails resolve through `maps/thumb/`.
- Original Angular controllers must initialize before initialization scripts are
  flushed. A startup timeout restores the existing native fallback.
- Missing image assets log diagnostics; failed scripts/styles and unavailable
  Lua APIs report visible errors. No browser TestData is enabled: the WebKit
  user agent explicitly identifies a Valve Source Client.

## Source layout

- `resources/lua/port/menu_bootstrap.lua`: menu-specific GLua, panel and DHTML
  bindings; includes the original files unchanged.
- `resources/lua/port/json.lua`: strict JSON codec for callback arguments/results.
- `engine_src/gameui/gmp_lua_menu.inl`: actual Source filesystem, console, ConVar,
  localization, GameUI and HTML transport operations.
- `platform/html/html_panel.mm`: WKWebView, local scheme handler and message queue.
- `platform/html/bridge_script.h`: shared JavaScript used verbatim in WebKit and
  the browser integration test.
- `engine_src/game/shared/gmod/glua_runtime.*`: interpreter ownership and includes.
- `tests/glua/menu_test.cpp`: unchanged original Lua against an explicit test engine.
- `tests/menu/browser.mjs`: original HTML/JS against that real Lua interpreter.

## Reproduce host checks (Linux)

```sh
python3 tools/bootstrap.py --only luajit --only garrysmod
python3 gluavm/patch_luajit.py deps/luajit
make -C deps/luajit -j2 BUILDMODE=static XCFLAGS='-DLUAJIT_DISABLE_JIT -DLUAJIT_DISABLE_FFI'
cmake -S . -B build-native-host -DGMOD_BUILD_ENGINE=OFF -DGMOD_BUILD_GLUA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native-host --parallel 2
ctest --test-dir build-native-host --output-on-failure
python3 tests/glua/test_icon_package.py
npm install --prefix tests/menu --ignore-scripts --omit=optional
tests/menu/node_modules/.bin/playwright install chromium
node tests/menu/browser.mjs deps/garrysmod/garrysmod build-native-host/glua_menu_test
```

Observed locally and in [preflight 42371703372](https://github.com/gandon-pro/gmod-port-ios/actions/runs/42371703372): five CTest tests pass, including unchanged original main menu
creation, map classification, DHTML callbacks/results, strict JSON, favourites,
persistence and cleanup. The test engine supplies two explicitly synthetic map
inventory entries; it does not launch Source or establish device behavior.

The browser check saves `artifacts/menu/main-menu.png`, `new-game.png` and
`result.json`. Missing PC content in its fixtures is recorded separately. It
checks `IN_ENGINE`, localization callbacks, navigation and the `map gm_construct`
command produced by real original Lua. The observed Chrome 157.0.8103.39 run
completed with no Lua/JavaScript errors. The fixture omits flags, PC material
icons and real map thumbnails; screenshots are not an iPhone capture. Browser rendering does not verify WebKit
custom-scheme behavior or Source execution on an iPhone.

## iOS checks and next device check

The preflight builds/validates the arm64 iPhone VM, type-checks the WKWebView code,
and checks the actual patched `BasePanel.cpp` and `GameUI_Interface.cpp` using
exported Waf include paths/defines with the iPhone SDK. G1 is the full application
compile/link/IPA workflow; preflight alone is not a complete app build.

Install a G1 IPA built from this branch, copy the original game content, then:

1. Check `gmp_glua_menu_status`: it must report original mainmenu.lua and HTML ready.
2. Verify translated main menu, orientation changes and native Options opening/closing.
3. Choose an installed map in Start New Game; confirm actual loading and gameplay.
4. Reopen GameUI, Resume Game, and check input returns to the game.
5. Toggle a map favourite, restart the app, and confirm persistence.

Full application linking and these on-device steps have not yet been observed
for this patch. No Xcode/iOS SDK is installed in the Linux editing workspace;
iOS compiler checks run in GitHub Actions.

## General GLua bootstrap remains incomplete

The independent `glua_bootstrap_probe deps/garrysmod/garrysmod` deliberately still
executes full original `includes/init.lua`, not the menu-specific bootstrap.
It exits **1** in all three realms at:

```text
includes/util/color.lua:8: attempt to call global 'RegisterMetaTable' (a nil value)
```

The menu-specific entry point avoids claiming that this general API port is done.
Metatable/value types, entities, game libraries, general VGUI and gamemode startup
remain unimplemented. See `docs/GPT_TASK_GLUA.md` for the earlier interface contract.
