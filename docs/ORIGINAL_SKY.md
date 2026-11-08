# Original GMod sky: required source inputs

The pinned Facepunch checkout contains the unchanged
`gamemodes/base/entities/entities/env_skypaint.lua` and
`lua/matproxy/sky_paint.lua`. Their networked fields, defaults and material
parameter writes can be inspected directly. That checkout does not include
`g_sky` shader source, compiled `.vcs` shaders or the shader-registration
library. No original sky bytecode/library was found in the available workspace.

The current runtime still uses `g_sky -> Sky_DX9` and the empty native
`SkyPaint` proxy in `tools/patch_engine.py`. **The original sky is not ported.**
Those fallbacks have not been removed: doing so before a validated replacement
exists would break sky rendering. A guessed gradient is not an original port.

## Collect the exact installed build

Run from this repository on the PC containing the original game:

```powershell
py -3 tools/collect_original_sky.py "C:\Program Files (x86)\Steam\steamapps\common\GarrysMod" original-sky.zip
```

Adjust the installation path to the actual game folder. Python 3.10+ is
sufficient; this tool uses the standard library and the repository's existing
`tools/extract_npc_content.py` VPK reader. It does not modify the installation
or replace an existing output archive.

The collector selects actual filenames containing `sky` under
`garrysmod/shaders/fxc`, matching skybox materials/textures, both original Lua
files, and shader libraries under the installation's `bin`/`garrysmod/bin`
folders. Packed game assets are read from `garrysmod/*_dir.vpk`, preserving
loose-file overrides. If a loose `gm_construct.bsp` exists, only its entity
lump is exported, along with a hash of the original map. The manifest records
relative paths, origin, hashes, optional Steam build ID and PE machine type.
No guessed shader filenames, parameter-register layouts or runtime
compatibility claims are inserted. Missing shader inputs are explicitly listed.
The package is for analysis; collection does not install a working sky.

After receiving `original-sky.zip`, identify the actual `g_sky` shader variants
and parameter-register bindings in that build, adapt them to the pinned
Source/ToGLES shader loader, connect the original entity/proxy behavior, and
verify sky direction, gradients, sun, dusk, stars and HDR on the target. The
Windows shader-registration library cannot be loaded directly by iOS.

## Check the collector

```sh
python3 tests/test_collect_original_sky.py
```

Observed: synthetic loose/VPK priority, exact bytes/hashes, PE machine tagging,
BSP entity bounds, build-ID-only metadata, missing input reporting and output
overwrite protection pass. These fixtures are not original GMod shaders and
do not establish sky-renderer compatibility.
