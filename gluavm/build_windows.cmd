@echo off
setlocal
if "%~1"=="" exit /b 2
set "VM_SOURCE=%~f1"
set "VM_OUT=%~dp0..\build-gluavm\windows-x64"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
pushd "%VM_SOURCE%\src"
call msvcbuild.bat static
if errorlevel 1 exit /b 1
if not exist lua51.lib exit /b 1
if not exist "%VM_OUT%" mkdir "%VM_OUT%"
copy /y lua51.lib "%VM_OUT%\gluavm.lib" >nul
copy /y lua.h "%VM_OUT%\lua.h" >nul
copy /y lua.hpp "%VM_OUT%\lua.hpp" >nul
copy /y luaconf.h "%VM_OUT%\luaconf.h" >nul
copy /y lualib.h "%VM_OUT%\lualib.h" >nul
copy /y lauxlib.h "%VM_OUT%\lauxlib.h" >nul
copy /y luajit.h "%VM_OUT%\luajit.h" >nul
popd
cl /nologo /std:c++17 /MD /EHsc /I "%VM_OUT%" "%~dp0verify_vm.cpp" "%VM_OUT%\gluavm.lib" /Fe:"%VM_OUT%\gluavm_verify.exe" /Fo:"%VM_OUT%\verify_vm.obj"
exit /b %errorlevel%
