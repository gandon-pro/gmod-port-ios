# Gamma, sRGB and HDR diagnosis

The diagnostic commands remain read-only. The presentation path now applies
Source's requested per-channel gamma ramp on the GPU, and the iOS texture path
now restores the existing Source sRGB format-switch fallback. Neither fix forces HDR.

## Source/build scope

- Source engine: `ed8209cc35c61fbd8ddff8480962a01c981eef2f`.
- SDL: `c98c4fbff6d8f3016a3ce6685bf8f43433c3efcc`.
- ANGLE Metal: `db5eb421c4350138e291220646de7764987a07e3`.
- Existing device logs are from app build 90. They report Apple A15,
  ANGLE Metal / OpenGL ES 3.0, and no `GL_EXT_texture_sRGB_decode`.
- No device execution of the new diagnostics has been observed yet.

## Findings before the gamma/sRGB fix

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

## Implemented correction

`platform/angle/gamma_pass.cpp` applies the exact 256-entry unsigned-16-bit RGB
ramp submitted by Source. The ANGLE build routes `CSDLMgr::SetGammaRamp` to this
GPU pass instead of SDL's unsupported UIKit hardware-gamma entry point. Ramp
updates are mutex-protected and may precede GL context creation. The pass runs
on the primary context just before swap, after Source rendering and tone
mapping. It uses a high-precision float LUT, preserves alpha and touched GL
state, resolves the final backbuffer to a private texture, resizes with the
actual drawable and releases objects while the primary context is current.
An exact identity ramp bypasses the pass. Shader/allocation failures are
reported and correction is suspended until another ramp update; unsupported
nonlinear presentation surfaces are explicitly rejected to prevent double
sRGB encoding. The Apple GLES fallback without ANGLE retains SDL behavior.

The pinned Source `HandleSRGBMismatch`/`ResetSRGB` texture-reupload path is now
compiled on iOS and invoked from both sampler-object and texture-parameter
paths. The iOS handler skips render targets and unsupported formats, requires
retained CPU texels, and reuploads only when the texture encoding differs.
It uses actual sRGB texture storage, not a gamma multiplier applied to every
material. Desktop CGL/client-storage APIs remain excluded. As with the upstream
fallback, simultaneously sampling the same texture through conflicting sRGB
modes is not solved by storage switching; this would require dual texture
storage or a shader-level decoding design. Missing CPU backing reports a
warning instead of attempting an unsafe reupload.

Observed local checks on Mesa llvmpipe GLES3:

- Production gamma pass: per-channel nonlinear/inverted LUTs agree with the
  reference pixels within one 8-bit output step; alpha is preserved.
- Identity ramp bypass, resize, release/recreation and touched GL-state
  restoration pass, including sampler bindings and pixel-unpack state.
- The actual patched Source texture methods run against real GL uploads in a
  small context/layout fixture: a stored value of 128 samples as 55 through
  sRGB decoding and returns to 128 after switching to raw reads. Repeated
  requests avoid redundant uploads; render targets/missing backing are skipped.

These checks are not an iPhone/ANGLE Metal capture or proof of PC parity.
After installing the new build, look for `GMP gamma: presentation LUT applied`,
`read_state_fallback=texture_reupload` and the existing actual HDR snapshot.

## Build 96 vertex-lock report

The supplied `gmodport_log(1).txt` identifies build 96. The companion engine
log reports `CMeshDX8::LockVertexBuffer` failing for four vertices at 30.0898 s,
after `maxplayers set to 1`. Neither log contains a submitted/applied gamma
ramp for that launch. The low-level buffer-map result and GL error were absent;
the exact device failure is not established by this fatal message alone.

The iOS buffer path now records rejected mappings and uses Source's existing
bounded CPU-staging/`Unlock`+`glBufferSubData` path when all of these hold:
real storage covers the requested range, its binding matches the expected
buffer name, it is not already mapped, the failure is not GL_OUT_OF_MEMORY,
and this is a vertex/index write fitting the existing staging allocation.
Existing static-staging disable controls are respected. Failed maps no longer
leave `m_bMapped` set, and every lock initially clears its output pointer.
Missing vertex-buffer storage is logged separately. Unrecoverable conditions
still fail rather than pretending to have GPU storage.

`tests/test_buffer_fallback.py` compiles the exact injected fallback code and
runs it against Mesa GLES3. A deliberately rejected map recovers through
staging; uploaded bytes at a nonzero offset are verified on the GPU. Bounds,
already-mapped buffers, a simulated OOM status, disabled staging and incorrect
bindings are rejected. This verifies the fallback, not the cause of the iPhone
crash. A new device run is required to determine whether it resolves that case.
