# Captions, local creations and graphics quality

Target: native-glua-html branch, iOS arm64; Source ed8209cc35c61fbd8ddff8480962a01c981eef2f, Facepunch Lua ad8023410600199ad9a12444bc496b9d65226a82.

## Spawn captions

The old pre-child caption strip and the new post-child original ContentIcon frame both painted a caption. Only the final caption remains. A glyph-advance budget truncates long names with an ellipsis within the card. Model/PNG images now fill the original image region instead of reserving a second caption strip. Missing overlay images retain a dark caption on the native light background.

## Main menu

The original workshop_files.lua, menu_dupe.lua, menu_demo.lua and menu_save.lua are bundled unchanged and installed at launch, as are the new bounded menu bindings. Original HTML pages and their local pagination remain in use.

* Dupes > Local enumerates `garrysmod/dupes/*.dupe`, newest first. `Load into Duplicator` requires a loaded map and forwards the selected path to the existing `gm_dupe_open` handler. Paste with the Tool Gun's primary action. `Save copied dupe` calls `gm_dupe_save`; copy first with the Duplicator's secondary action. Supported entity/constraint formats remain those implemented by the existing native duplicator, not arbitrary addon GLua.
* Demos > Local enumerates `garrysmod/demos/*.dem`, newest first. Record starts `record demos/recording_DATE_TIME`, closes pause/menu, and refuses to overwrite an existing file. Stop ends recording; Play validates the actual Source demo header and both demo and network protocol before `playdemo`. These are engine demo recordings, not video exports. PC GMod files with a different protocol are rejected explicitly. Required maps/content must be installed.
* Local delete is restricted to direct children of dupes/saves/demos and their supported file/preview extensions. Deleting a creation also removes its same-name metadata and preview. Demo deletion is blocked during recording. No generic filesystem deletion is exposed.
* Saves are listed by the unchanged local controller and forwarded to the existing native `gm_load` path. Save names and formats retain the current native loader's limits.
* Steam Workshop fetching/publishing and video export are not implemented. Online categories return an empty result and an explicit notice instead of leaving a loading spinner.

The existing dupe/save server commands consume `ArgS`, so paths with spaces are validated and forwarded as the whole unquoted command remainder. Demo record/play commands use a quoted filename argument. Recording and playback remain game-engine operations; host tests do not prove device compatibility.

## Graphics

`gmp_graphics.cfg` now requests mat_picmip=-1, texture/lightmap filtering, explicit water reflection and refraction, and up to 64 rendered shadows. It retains native anisotropic filtering, bump/specular/Phong, HDR and render-to-texture shadows. The engine still decides supported formats, material/shader behaviour, map lighting and content availability; no assets or offsets are fabricated.

`gmp_render_status` and the automatic post-map log now include texture, water and shadow variables in addition to gamma/sRGB/HDR. Requested settings do not prove that a visible water material uses the expensive path or that a shadow was submitted. IMG_0896 shows the Q-menu, not a water/shadow comparison; a device capture with visible water and shadow-casting props plus the new log is required to diagnose a specific remaining renderer defect. There is no new claim of PC pixel parity or a new water/shadow shader implementation.

## Validation

```
python3 tests/test_render_diagnostics.py
python3 tests/test_local_content.py --engine deps/engine
cmake -S . -B build-native-host -DGMOD_BUILD_ENGINE=OFF -DGMOD_BUILD_GLUA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native-host --parallel 3
ctest --test-dir build-native-host --output-on-failure
node tests/menu/browser.mjs deps/garrysmod/garrysmod build-native-host/glua_menu_test
```

The local action fixture compiles the production dispatch branches with the actual pinned demo-header declaration against a deterministic file/engine host. It verifies dupe/save command paths with spaces, record/stop/play, invalid/partial demo rejection, protocol mismatch, overwrite prevention and bounded deletion/cleanup. Lua tests exercise the unchanged local controllers, pagination, callbacks and commands. Caption/path tests cover long names and prohibited paths. Browser checks cover the original Angular pages and added controls; they require an installed Chromium and are wired into CI. Native iPhone compilation and on-device behaviour are separate gates, not implied by these host tests.

## Angular production-mode regression

Build106's IPA compilation succeeded, but the original-HTML browser test timed out waiting for `Load into Duplicator`. The unchanged `html/js/menu/common.js` disables `$compileProvider.debugInfoEnabled`, so `angular.element(card).scope()` cannot expose the row data. The extension now uses `dupe.Scope.Files`, which is the original WorkshopFiles controller's public model, and adds a visible button directly to each local card rather than the hover-only action tray.

`node tests/menu/local_controls.mjs` executes the production extension with Angular DOM scope access deliberately forbidden. It covers local/online cards, refreshed rows, repeated mutation callbacks and recording controls. The original browser test retains disabled debug metadata and now checks that condition explicitly. For the existing original demo Play control, the browser test hovers the card to expose Source's hover-only action tray before clicking it.
