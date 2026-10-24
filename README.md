# GModPort iOS — M3

Личная сборка, приватный репозиторий. Контент игры и ПК-бинарники сюда не входят.
Пины находятся в dependencies.lock.json. Исходная база движка не заменяется.

## Текущий статус

- G1 **не готов**: сборочная цель и статические фабрики добавлены; полная компиляция,
  конфликты остальных символов и запуск gm_construct должны быть проверены.
- Renderer bring-up использует разрешённый заданием native OpenGLES fallback.
  **ANGLE/Metal ещё не подключён**, его наличие не имитируется.
- G2 пока не начат: сначала bring-up G1. Непатченный LuaJIT не называется GLua VM.
- Код Aero в game/client/gmod, game/server/gmod и game/shared/gmod включается в
  source lists при наличии, но не изменяется средствами этого проекта.

## Сборка G1

Workflow `G1 Source iOS`: macos-14, ручной запуск или push main. Сначала host-test
реестра, затем реальные iPhoneOS зависимости и все Source-модули. IPA публикуется
только после успешной компиляции/линковки приложения и проверки Mach-O arm64.
Зелёный тест реестра сам по себе не означает готовую игру.

CMake выбран для iOS SDK и app bundle, списки C++-файлов берутся из существующих
wscript; это не ручная копия тысяч записей. Compiler/dependency probes выполняет
реальная сборка, экспортёр читает только метаданные Waf.

Модули статические; у каждого своё имя CreateInterface/InterfaceReg/
CreateInterfaceInternal/Sys_GetFactoryThis через forced prefix header. Известные
per-module globals также префиксуются. Другие duplicate symbols не подавляются
опцией линкера: их нужно разбирать по compile/link log и дополнять карту.
tier1/interface.cpp заменён общей реализацией loader и фабриками модулей.

## Контент и проверка на телефоне

После установки и переподписи IPA через Sideloadly/AltStore импортировать собственный
контент в Files → GModPort → garrysmod и sourceengine. Путь игры:
Documents/garrysmod. Бандл не содержит gm_construct и ресурсов GMod.
Конфигурация запуска задаёт `-basedir Documents -game garrysmod +map gm_construct`.
Для G1 используются stock hl2mp client/server. Это ещё не GMod sandbox.
Q/C кнопки отправляют будущие GMod menu-команды; stock hl2mp spawnmenu не реализует.

Критерий G1: реальный iPhone загружает карту и ходит тачем. До этого статус остаётся
неподтверждённым независимо от наличия artifact.

Host-test на Windows: `cmake -S . -B build-host -A x64 -DGMOD_BUILD_ENGINE=OFF`,
затем `cmake --build build-host --config Debug` и `ctest --test-dir build-host -C Debug`.
