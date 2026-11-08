# Tool screen, HUD, weapon selection and spawn icons

Reference: Facepunch/garrysmod `ad8023410600199ad9a12444bc496b9d65226a82` and Source engine `ed8209cc35c61fbd8ddff8480962a01c981eef2f`.

* Tool screen executes the shipped original `gamemodes/sandbox/entities/weapons/gmod_tool/cl_viewscreen.lua` unchanged in an isolated client Lua state. The exact pinned Lua file and English tool.properties are bundled and installed on launch. The host supplies surface calls, the actual 256x256 `GModToolgunScreen` material target, time and tool mode. PreRender draws it before the viewmodel, outside the normal HUD/VGUI paint traversal. It retains the original background material, Helvetica 60/900, 250 pixel/sec scroll, 3px shadow and y=104 text centre. The screen material's previous texture is restored and owned resources released on level exit/shutdown. Native stools do not supply custom Lua DrawToolScreen callbacks.
* Tool help is a native adaptation of `gmod_tool/cl_init.lua`: Roboto Bk 80/24/17, x=50/y=40, gradient 600px, title, translated description and stage-specific instructions. These dimensions scale from a 1080px viewport; missing Roboto falls back to Helvetica with a warning. `gmod_drawhelp 0/1` controls help. English tool.properties is loaded, followed by installed gmod_language tool.properties. Original gui/lmb.png, gui/rmb.png and gui/r.png button icons are loaded when installed, otherwise text labels remain. Instruction strings still come from native tool definitions; full stool Information/op dispatch is not implemented.
* Health, suit armour and ammo remain the native Source HUD using the mounted game's ClientScheme/HudLayout and weapon data. No second numeric HUD is drawn. Touch gmp_weapnext/prev now invoke invnext/invprev so the existing slot selector, its icons and confirmation path are used. FIRE confirms the selection when hud_fastswitch=0; hud_fastswitch=1 keeps immediate selection.
* Spawn icons use explicit original materials/entities/*.png metadata for weapons, entities and NPCs, and installed materials/spawnicons/models/... cache hits for props. The cache lookup supports default skin and `_skinN` files; a hit is used only after successful decode. Without an installed image, the existing native model preview remains. This is not a port of GMod's closed-source ModelImage PNG generator. Installed original gui/ContentIcon-normal.png, ContentIcon-hovered.png and sm_hover.png supply the content frame and prop hover border. SpawnIcon cells use 64x64, ContentIcon cells 128x128; pooling bounds image allocations to visible cells. Scrolling/replacing items and destroying the grid release/reuse image resources.

## Checks

Host test runs the unchanged original tool screen Lua and verifies background dimensions, font parameters, scrolling over several timestamps, text shadow positions, balanced camera/RT and error handling.

```
cmake -S . -B build-native-host -DGMOD_BUILD_ENGINE=OFF -DGMOD_BUILD_GLUA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native-host --parallel 3
ctest --test-dir build-native-host --output-on-failure
```

macOS CI type-checks the actual client/server weapon translation units plus native HUD units against the pinned Source headers and iPhone SDK, then builds the IPA.

Device acceptance: enter gm_construct, equip Tool Gun, select weld then rope (screen name scrolls/changes), toggle gmod_drawhelp, cycle with both touch arrow buttons and confirm using FIRE, spawn prop/weapon/NPC from their icons, scroll long lists, rotate/resize, change map and reconnect. Verify health/armour/ammo reflect damage, suit battery, reload and weapon changes. Original PNG/materials, model textures and fonts must be installed in GAME. No device runtime result is inferred from host checks or the compiler.
