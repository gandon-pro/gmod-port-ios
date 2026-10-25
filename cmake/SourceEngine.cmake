# CMake is used as an iOS packaging/link driver; source lists remain upstream waf.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SOURCE_ENGINE "${CMAKE_SOURCE_DIR}/deps/engine" CACHE PATH "Pinned engine checkout")
set(SDL_SOURCE "${CMAKE_SOURCE_DIR}/deps/sdl" CACHE PATH "Pinned SDL2 checkout")
set(GMOD_DEPS_PREFIX "${CMAKE_SOURCE_DIR}/build-deps/install" CACHE PATH "iPhoneOS static dependencies")
if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  message(FATAL_ERROR "This target requires arm64 iphoneos")
endif()
execute_process(COMMAND git -C "${SOURCE_ENGINE}" rev-parse HEAD OUTPUT_VARIABLE source_revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT source_revision STREQUAL "ed8209cc35c61fbd8ddff8480962a01c981eef2f")
  message(FATAL_ERROR "Unexpected engine revision: ${source_revision}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
find_package(SDL2 REQUIRED CONFIG PATHS "${GMOD_DEPS_PREFIX}/lib/cmake/SDL2" NO_DEFAULT_PATH)
foreach(pair "freetype;freetype" "fontconfig;fontconfig" "jpeg;jpeg" "png;png16;png" "curl;curl" "openal;openal;OpenAL")
  set(names ${pair})
  list(POP_FRONT names target)
  # These are already cross-compiled archives at an explicit workspace path.
  # Do not re-root that path under iPhoneOS.sdk as for SDK system libraries.
  find_library(dep_${target} NAMES ${names} PATHS "${GMOD_DEPS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
  add_library(ios_${target} STATIC IMPORTED GLOBAL)
  set_target_properties(ios_${target} PROPERTIES IMPORTED_LOCATION "${dep_${target}}" INTERFACE_INCLUDE_DIRECTORIES "${GMOD_DEPS_PREFIX}/include")
endforeach()
set(GMOD_BUILD_NUMBER "$ENV{GITHUB_RUN_NUMBER}")
if(NOT GMOD_BUILD_NUMBER)
  set(GMOD_BUILD_NUMBER 1)
endif()
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generated}")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/export_engine.py"
  --engine "${SOURCE_ENGINE}" --sdl-include "${SDL_SOURCE}/include" --output "${generated}/source-manifest.json" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/generate_targets.py"
  --manifest "${generated}/source-manifest.json" --output "${generated}" --platform "${CMAKE_SOURCE_DIR}/platform"
  --isolate-modules COMMAND_ERROR_IS_FATAL ANY)
include("${generated}/targets.cmake")
add_library(port_source_loader STATIC platform/source_loader.cpp)
target_include_directories(port_source_loader PRIVATE "${SOURCE_ENGINE}/public" "${SOURCE_ENGINE}/public/tier0" "${SOURCE_ENGINE}/public/tier1")
target_compile_definitions(port_source_loader PRIVATE POSIX=1 OSX=1 GNUC=1 SOURCE_IOS=1 NO_MEMOVERRIDE_NEW_DELETE=1)
target_link_libraries(port_source_loader PUBLIC port_module_registry)
file(GLOB GMOD_TOUCH_TEXTURES "${CMAKE_SOURCE_DIR}/resources/touch/*.vtf")
add_executable(GModPort MACOSX_BUNDLE app/ios_main.mm "${SOURCE_MODULE_TABLE}" resources/gameinfo.txt resources/touch_default.cfg resources/gmp_bindings.cfg resources/gmp_gamepad.cfg resources/weapon_physgun.txt resources/gmod_tool.txt resources/ui/GameMenu.res resources/ui/GameLogo.res ${GMOD_TOUCH_TEXTURES})
set_source_files_properties(app/ios_main.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
set_source_files_properties(resources/gameinfo.txt resources/touch_default.cfg resources/gmp_bindings.cfg resources/gmp_gamepad.cfg resources/weapon_physgun.txt resources/gmod_tool.txt resources/ui/GameMenu.res resources/ui/GameLogo.res PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties(${GMOD_TOUCH_TEXTURES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/touch)
target_link_libraries(GModPort PRIVATE port_source_loader port_module_registry SDL2::SDL2main SDL2::SDL2-static ${SOURCE_EXTERNAL_TARGETS})
set_source_files_properties(${SOURCE_MODULE_OBJECTS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_sources(GModPort PRIVATE ${SOURCE_MODULE_OBJECTS})
add_dependencies(GModPort ${SOURCE_MODULE_OBJECT_TARGETS})
# First bring-up uses the task's explicitly permitted native GLES fallback.
# ANGLE requires a separately built iOS EGL/GLES library and SDL EGL integration;
# no desktop ANGLE binary is silently linked here.
message(WARNING "Renderer bring-up: native Apple OpenGLES fallback. ANGLE/Metal is not integrated yet.")
target_link_libraries(GModPort PRIVATE "-framework UIKit" "-framework Foundation" "-framework QuartzCore"
  "-framework OpenGLES" "-framework AVFoundation" "-framework AudioToolbox" "-framework CoreAudio"
  "-framework CoreMotion" "-framework GameController" "-framework Metal" "-framework CoreHaptics"
  "-framework CoreGraphics" "-framework CoreText" "-framework Security" "-framework SystemConfiguration"
  "-framework UniformTypeIdentifiers" iconv z bz2 expat)
set_target_properties(GModPort PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/app/Info.plist.in"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.kindmops.gmodport
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
