# gluavm — G2

LuaJIT `a4c064539ea931274d407e1249b4bb575424a0f4`, standard Lua C API через lua.hpp.
Не реализует C++-функции GMod и не выполняет sandbox во время syntax check.

В данном pinned upstream уже есть `!=`, `!`, `&&`, `||` и scoped `continue`.
Собственная правка добавляет `//` и `/* */` в lexer, включая подсчёт строк,
EOF-диагностику и чтение на границах reader buffers. Исходные Lua-файлы не меняются.
JIT и FFI отключены как при генерации VM assembly, так и при компиляции C-кода.

## Windows x64

`python gluavm/patch_luajit.py deps/luajit`, затем
`gluavm\build_windows.cmd deps\luajit` из корня репозитория.
Нужен VS 2022 Community с C++ x64. Выход: build-gluavm/windows-x64/gluavm.lib
и заголовки. Runtime — release `/MD`; при интеграции согласовать CRT игрового модуля.
Динамической lua51.dll для этой библиотеки не требуется.

Проверки: `python gluavm/prepare_checks.py --garrysmod PATH --out build-gluavm/checks`,
`gluavm_verify --run-manifest .../tests.txt` и
`gluavm_verify --compile-manifest .../syntax.txt`.
Второй режим вызывает luaL_loadfile для каждого файла, без выполнения chunk.
Лог — отдельная JSONL-строка на файл и итоговый счётчик.

## iPhone arm64

`bash gluavm/build_ios.sh` на macOS/Xcode. Выход:
build-gluavm/ios-arm64/libgluavm.a + include/. Сборка проверяет arm64 через lipo
и платформу iOS в каждом Mach-O объекте через verify_archive.py. Это исключает
случайную подстановку macOS arm64-библиотеки с той же архитектурой.
Workflow G2 дополнительно собирает native host VM для запуска тестов и компиляции
корпуса. Это не означает выполнение VM на iPhone — только сборку и host validation.

Подключение из CMake: `include(path/to/gluavm-target.cmake)`,
`target_link_libraries(your_game_module PRIVATE gluavm)`.
GLUAVM_ROOT можно переопределить путём к скачанному/локальному artifact.
