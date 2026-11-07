# Gamma, sRGB and HDR diagnosis

This instrumentation is read-only. It does not raise gamma, force HDR, replace
textures or alter any rendering state to make a screenshot appear brighter.

## Source/build scope

- Source engine: `ed8209cc35c61fbd8ddff8480962a01c981eef2f`.
- SDL: `c98c4fbff6d8f3016a3ce6685bf8f43433c3efcc`.
- ANGLE Metal: `db5eb421c4350138e291220646de7764987a07e3`.
- Existing device logs are from app build 90. They report Apple A15,
  ANGLE Metal / OpenGL ES 3.0, and no `GL_EXT_texture_sRGB_decode`.
- No device execution of the new diagnostics has been observed yet.

## Confirmed source findings

1. `CSDLMgr::SetGammaRamp` forwards to `SDL_SetWindowGammaRamp`. The pinned SDL
   UIKit driver never installs `SetWindowGammaRamp`; SDL returns unsupported.
   `mat_monitorgamma` or a hardware-capability flag alone cannot prove that the
   requested hardware gamma curve reached the display. Other shader-based
   color transformations are separate from this unsupported path.
2. `GLMContext::FlushDrawStates` has an sRGB texture-format mismatch fallback
   guarded by `OSX`, for both sampler-object and texture-parameter paths.
   `tools/patch_engine.py` excludes those desktop OSX branches from SOURCE_IOS.
   `SetSamplerSRGBTexture` still records the requested sampler bit, while the
   `HandleSRGBMismatch` repair is absent. The old log proves the decode
   extension is absent; it does not quantify the visual effect on a material.
   Shader-based fake sRGB writes also exist and must not be confused with
   this texture-read repair.
3. `CHardwareConfig::GetHDRType()` requires `mat_hdr_level >= 2`, internal
   `GetHDREnabled()`, DX support >= 90 and a non-NONE hardware HDR type.
   `mat_hdr_level 2` is a request, not proof of an active HDR pipeline.
   Map HDR lighting must be checked separately. Source scene HDR is not
   equivalent to HDR display output.

## Device capture

Install a new IPA containing these changes, launch a map and wait at least two
seconds after loading. `CGMPRenderDiagnostics` prints one `GMP render:` snapshot
per level even when the client GLua bootstrap fails. It waits through loading
and cancels pending snapshots on level shutdown. For a later snapshot, use:

```text
gmp_render_status
```

Capture `Documents/gmodport_log.txt` and `engine.log`. Required lines:

- `GMP gamma:`: actual SDL gamma-call result, SDL error, requested midpoint RGB.
  No line means the gamma-call path was not observed; it does not mean success.
- `GMP sRGB:`: actual decode extension, sampler-object selection, CPU texture
  backing and the compiled-out texture-read fallback.
- `GMP EGL:`: first primary context's queried window-surface RGBA bit counts
  and colorspace. Unsupported optional queries are explicitly marked unknown.
  These describe the presentation surface, not intermediate HDR render targets.
- `GMP render:`: current material config HDR flag/gamma, map HDR lighting,
  engine-selected HDR type, hardware HDR type, DX level, sRGB capabilities and
  current relevant ConVars. Missing interfaces and ConVars remain unavailable.

Interpret `hdr_type=none` as the engine reporting no active HDR type at the
snapshot. If config HDR is enabled and map HDR lighting is present, a non-NONE
HDR type is evidence that Source selected HDR. This still does not validate
all render-target formats, tone mapping, or a PC-matched image. Record a second
snapshot if settings change after the automatic capture.

## Validation

```sh
python3 tests/test_render_diagnostics.py
python3 tools/bootstrap.py --only engine --only sdl
python3 tools/patch_engine.py deps/engine
python3 tools/check_menu_engine.py
```

The first check compiles the actual diagnostic code in a Source-interface
fixture, testing requested-vs-selected HDR, unavailable values, unchanged CVars,
load delay and cleanup. The SDK check compiles the real shared client/server
translation units. Full G1 builds compile the changed SDL/ToGLES/EGL code.
Neither a fixture nor a successful build supplies actual device measurements.
