# Include after the platform library has been built. Exports the ordinary Lua C
# API only; no GMod native functions are provided by this target.
if(TARGET gluavm)
  return()
endif()
if(NOT GLUAVM_ROOT)
  if(IOS)
    set(GLUAVM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../build-gluavm/ios-arm64")
  elseif(WIN32)
    set(GLUAVM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../build-gluavm/windows-x64")
  else()
    message(FATAL_ERROR "Set GLUAVM_ROOT for this platform")
  endif()
endif()
add_library(gluavm STATIC IMPORTED GLOBAL)
if(WIN32)
  set_target_properties(gluavm PROPERTIES IMPORTED_LOCATION "${GLUAVM_ROOT}/gluavm.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${GLUAVM_ROOT}")
else()
  set_target_properties(gluavm PROPERTIES IMPORTED_LOCATION "${GLUAVM_ROOT}/libgluavm.a"
    INTERFACE_INCLUDE_DIRECTORIES "${GLUAVM_ROOT}/include")
endif()
