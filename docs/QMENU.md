# Q-menu implementation and verification

The existing native VGUI Q-menu now has separate creation/tool panes, content
search, a tool filter, collapsible categories, all 29 existing native tool modes,
editable tool parameters, Reset defaults and Utilities actions. Search preserves
matching item groups. Empty results are explicit. Keyboard focus starts/stops SDL
text input on iOS; closing releases mouse capture. Resizing updates the layout.

**This is not the complete original Lua/Derma Q-menu.** Its original
`gamemodes/sandbox/gamemode/spawnmenu/spawnmenu.lua` controller, general VGUI
bindings, addon-created panels and original stool CPanel builders are not running.
Native C++ controllers continue to own the UI and spawning behavior. Do not present
this patch as a finished, pixel-identical port of the original Q-menu.

## Original code that runs

`resources/lua/port/qskin.lua` loads the unmodified pinned Facepunch
`lua/derma/derma_gwen.lua` and `lua/skins/default.lua`. The real GWEN functions
compute nine-slice quads; native bindings sample/upload the decoded skin atlas and
submit the resulting UV rectangles to Source VGUI. No guessed atlas coordinates
are supplied by the port. The skin uses a separate client-realm Lua state and
does not bypass the incomplete general GLua bootstrap.

Required installed assets:

- `materials/gwenskin/GModDefault.png`
- `lua/derma/derma_gwen.lua`
- `lua/skins/default.lua`
- Existing models, spawnlists and other game content described in the main README.

The app installs the port's own `lua/port/*.lua` from its bundle. Original game
assets are read through Source's GAME filesystem, including mounted VPKs. If the
atlas or skin scripts are missing, diagnostics name the failure and the native
controls retain a basic fallback appearance. The original skin cannot be visually
verified without the actual installed atlas.

Pins remain Source `ed8209cc35c61fbd8ddff8480962a01c981eef2f`, Facepunch
`ad8023410600199ad9a12444bc496b9d65226a82`, LuaJIT
`a4c064539ea931274d407e1249b4bb575424a0f4`.

## Controls and commands

- Existing `+menu` bind toggles Q-menu; the corner X/Escape closes it.
- Creation tabs keep the existing props/entities/weapons/NPCs/vehicles/dupes/saves
  backends. Native command semantics and ownership checks are preserved.
- Selecting a tool writes `gmod_toolmode`, equips `gmod_tool`, and opens settings.
- Parameter widgets write the existing archived `<mode>_<setting>` ConVars;
  `GMP_ToolSync` retains responsibility for synchronizing settings with the server.
- Numeric values reject invalid/non-finite input and clamp to the existing spec.
  Boolean fields use checkboxes, choice fields use the existing choice parser,
  numpad settings currently use numeric Source button codes.
- Utilities use actual existing `gmod_undo`, `gmod_cleanup`, `gm_dupe_save`, `gm_save`
  commands. Saving a dupe still requires a copied contraption.
- `gmp_qmenu_status` prints the skin state; skin failures are also logged.

## Checks

[Preflight 42373719941](https://github.com/gandon-pro/gmod-port-ios/actions/runs/42373719941)
passed with the first complete Q-menu implementation:

- iPhone arm64 interpreter archive validation and WebKit SDK compilation.
- Actual patched GameUI, client and server `weapon_physcannon.cpp` units checked
  with exported Waf flags and the iPhone SDK.
- Eight host CTest tests, including execution of real original GWEN/default skin,
  nine-slice UV bounds, small-panel clamping, tool metadata, filtering and layouts.
- Apple ImageIO/CoreGraphics decoder test for RGBA order, top/bottom orientation,
  straight alpha and invalid data.
- Existing original-main-menu browser smoke remains passing.

Reproduce host tests with the native GLua commands in `NATIVE_GLUA_HTML.md`.
Run `python3 tools/check_menu_engine.py` after bootstrapping/patching the engine
and building `gluavm/build_ios.sh` on macOS/Xcode. The G1 workflow now also builds
this feature branch and packages an unsigned IPA only after a full successful
compile/link.

No Q-menu screenshot from Source or an iPhone, full app link result, or live
spawn/tool action has been observed for this patch. Next device check: install
its IPA, open `+menu`, inspect `gmp_qmenu_status`, spawn a prop, select Weld, change
its limit, use the tool, switch tabs/search, then close/reopen and rotate. Test
settings persistence after restarting the app.
