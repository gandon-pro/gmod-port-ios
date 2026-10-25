# GModPort iOS — M3

Личная сборка, приватный репозиторий. Контент игры и ПК-бинарники сюда не входят.
Пины находятся в dependencies.lock.json. Исходная база движка не заменяется.

## Текущий статус

- G1: все Source-модули компилируются и линкуются в один arm64-бинарник, CI выпускает
  `GModPort-unsigned.ipa`. Пользователь сообщил о работающей игре на iPhone;
  новые NPC и редактор управления требуют отдельной проверки на устройстве.
- Renderer bring-up использует разрешённый заданием native OpenGLES fallback.
  **ANGLE/Metal ещё не подключён**, его наличие не имитируется.
- G2: статические библиотеки Windows x64 и iPhone arm64 собраны. На Windows и
  macOS прошли 9 runtime-тестов и компиляция 215 файлов includes/sandbox.
  [Успешный CI G2](https://github.com/gandon-pro/gmod-port-ios/actions/runs/40081079603).
  Все 70 объектов iOS-архива проверены как Mach-O arm64 с платформой iOS;
  выполнение VM на самом телефоне ещё не проверялось.
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

Модули статические; у каждого своё имя CreateInterface через forced prefix header.
Каждый модуль с нужными объектами вспомогательных библиотек объединяется через
`ld -r`, затем `nmedit` делает его внутренние символы локальными. Это сохраняет
отдельное состояние tier1/tier2/tier3 и InterfaceReg у каждого модуля.
Имена внешних API tier0/vstdlib/ToGLES/stub_steam сохраняются; фабрики остаются
раздельными. Это [штатный способ обработки объектных файлов Apple](https://github.com/apple-oss-distributions/cctools/blob/main/man/nmedit.1).
Опция подавления duplicate definitions не используется. Host-тест с настоящим
EXPOSE_INTERFACE_FN и общим helper-архивом проходит; полная линковка базового движка
проходит в CI. tier1/interface.cpp заменён общей реализацией loader и фабриками.

## Контент и проверка на телефоне

После установки и переподписи IPA через Sideloadly/AltStore скопировать в Files → GModPort
три папки из установленного GMod (`steamapps/common/GarrysMod`): `garrysmod`, `sourceengine`,
`platform`. gameinfo.txt, touch_default.cfg и текстуры кнопок приложение при каждом запуске
ставит поверх скопированных (свой gameinfo из GMod на телефоне не работает).
CI-бандл не содержит gm_construct и ресурсов GMod.

Вариант без копирования: встроить контент в IPA локально (только для личной установки,
контент не попадает в CI/GitHub):

    python tools/embed_content.py GModPort-unsigned.ipa "E:/SteamLibrary/steamapps/common/GarrysMod" GModPort-content.ipa

Скрипт кладёт `garrysmod`, `sourceengine` (только hl2_* VPK из gameinfo) и `platform` в
`GModPort.app/content/`. Приложение линкует `Documents/gmcontent` на встроенный контент,
а без него — на сам Documents, поэтому оба варианта используют один gameinfo.txt.
Конфигурация запуска использует `-basedir Documents -game garrysmod` и главное меню.
Основа — hl2mp client/server с нативными расширениями sandbox. Q открывает меню
создания, включая NPC. Полной Lua-совместимости GMod sandbox пока нет.
Шестерёнка включает редактор тач-кнопок: перемещение, размер, OK для сохранения.
[NPC, управление и перечень контента для Claude](docs/npc_touch_port.md).

Критерий G1: реальный iPhone загружает карту и ходит тачем. Пользователь сообщил
о примерно играбельном состоянии; новые возможности проверяются отдельно.

Host-test на Windows: `cmake -S . -B build-host -A x64 -DGMOD_BUILD_ENGINE=OFF`,
затем `cmake --build build-host --config Debug` и `ctest --test-dir build-host -C Debug`.
