# ANGLE / Metal integration status

The new platform/angle/angle_metal.cpp adapter implements EGL initialization
with the explicit Metal backend, an SDL Metal layer, GLES3 contexts, isolated
pbuffer surfaces for shared worker contexts, swap interval, presentation,
proc-address lookup and drawable-size queries. Destroy all contexts before
Shutdown; initialize/shutdown and present on the renderer owning thread.

Validation: MSVC C++17 syntax compilation against SDL2/Khronos headers and
ANGLE's official eglext_angle.h succeeded. This is not an iOS link or runtime test.

Source integration now redirects SDL context creation, sharing, activation,
destruction, swap interval and presentation via gmp_angle_bridge.h. The app
routes GL lookup and drawable dimensions to EGL and bypasses the EAGL FBO shim.
Worker contexts use private pbuffers. The SDL progress UI selects Metal before
content unpacking to avoid calling native GLES with ANGLE GL symbols linked.

The iOS workflow builds pinned ANGLE db5eb421c4350138e291220646de7764987a07e3,
collects angle_static object files into an arm64 archive and links it into the
app. GMOD_USE_ANGLE defaults to ON; missing libraries fail configuration.
GMOD_USE_ANGLE=OFF retains the earlier Apple GLES route for comparison.

Pending verification: actual iOS build/link, device lifecycle, backgrounding,
shader fidelity, water/shadows/HDR, GPU timing and frame presentation. No claim
of successful Metal gameplay or exact PC visuals is made before those tests.

Official reference:
https://chromium.googlesource.com/angle/angle.git/+/HEAD/doc/DevSetup.md

The pending gmp_graphics.cfg/fps_max 999 changes are independent of this adapter.
The original 1080-line resolution is preserved as explicitly requested.
