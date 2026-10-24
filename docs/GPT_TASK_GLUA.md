# Задача GLua для GPT: среда выполнения GMod Lua — типы и автономные библиотеки

Только личная сборка, без распространения. Контент GMod/Valve/Facepunch в репозиторий и CI не класть.

## Где мы сейчас (проверено)

- Репозиторий `gandon-pro/gmod-port-ios` (локально `C:\aero\gmod_port\ios_m3`). iOS-сборка движка nillerusr
  работает на iPhone: карта, тач, физган, спавн-меню, HUD. Игровой код пока стоковый hl2mp + C++-заплатки Aero.
- `gluavm` (LuaJIT v2.1 `a4c0645`, интерпретатор, синтаксис GLua) готов: 215/215 файлов `lua/includes` + `gamemodes/sandbox`
  из Facepunch/garrysmod `ad80234` компилируются (Windows и macOS). Библиотеки: `out/gluavm/windows-x64/gluavm.lib`, `out/gluavm/ios-arm64/libgluavm.a`.
- Спецификация API: `out/glua_port_spec.jsonl` (1536 записей: имя, kind, class, realm, сигнатуры, wiki, `native`, `subsystem`).
- Решено: настоящий GMod делаем через GLua — реализуем нативный API и запускаем **родной Lua Facepunch** (sandbox, тулган,
  Q/C-меню, Derma). Не переписывать Lua-логику GMod на C++.

## Контракт (не менять без Aero)

`ios_m3/engine_src/game/shared/gmod/glua_api.h` — регистрация нативных функций (`glua::Reg`, `GLUA_REGISTER`),
realm, push/check для Vector/Angle/VMatrix, `PushEntity` для объектов движка. Прочитай его первым.
Весь твой код — в `ios_m3/engine_src/game/shared/gmod/` в файлах с префиксом `glua_` из списка ниже.
Файлы Aero (`glua_api.cpp`, `glua_runtime*`, `glua_entity*`, `glua_vgui*`, `glua_render*`, всё в `game/client|server`) не трогай.
Работай в ветке `gpt/glua`, присылай PR после каждого этапа с логом сборки и тестов.

## Часть GPT

### L1. Типы значений — `glua_types.cpp`
Vector, Angle, VMatrix на `Vector`/`QAngle`/`VMatrix` из `public/mathlib`:
- конструкторы-глобалы (`Vector()`, `Vector(x,y,z)`, `Vector(vec)`, `Vector("1 2 3")`, `Angle(...)`, `Matrix()`/`Matrix(tbl)`);
- все нативные методы этих классов из спецификации (`kind == "method"`, `class` содержит Vector/Angle/VMatrix, `native == true`);
- метаметоды: `__index` (x/y/z, p/y/r, а также 1/2/3), `__newindex`, `__add`, `__sub`, `__mul` (на число и покомпонентно), `__div`, `__unm`, `__eq`, `__tostring`
  (формат как в GMod: `string.format("%f %f %f")`);
- глобальные функции этих подсистем (`subsystem == "math_types"`, напр. `LerpVector`, `LerpAngle`, `OrderVectors`);
- `glua::Push*/Check*/To*` из контракта.
Поведение сверяй с wiki (ссылка в каждой записи спецификации), спорное помечай `// GMod: unverified` и списком в PR.

### L2. Автономные библиотеки (не зависят от сущностей/рендера)
- `glua_util.cpp`: `util.CRC`, `Base64Encode/Decode`, `MD5`, `SHA1`, `SHA256`, `TableToJSON`/`JSONToTable` (с поддержкой Vector/Angle, как в GMod),
  `Compress`/`Decompress` (LZMA, формат GMod: 5 байт свойств + 8 байт длины + поток; LZMA SDK уже есть в `deps/engine/utils/lzma` — проверь, иначе закрепи версию),
  `KeyValuesToTable`/`TableToKeyValues`. Функции `util`, работающие с моделями, трассировкой и сущностями, — Aero.
- `glua_file.cpp`: библиотека `file` и объект `File` поверх `g_pFullFileSystem` (`IFileSystem`): пути `GAME`, `LUA`/`lsv`/`lcl`, `DATA`, `MOD`, `BASE_PATH`, `DOWNLOAD`;
  `file.Find` (сортировки `nameasc`/`namedesc`/`dateasc`/`datedesc`), `Exists`, `IsDir`, `Read`, `Write`, `Append`, `Delete`, `CreateDir`, `Rename`, `Size`, `Time`, `Open`;
  методы `File` (Read/Write/ReadLine/ReadBool/ReadByte/ReadShort/ReadLong/ReadULong/ReadFloat/ReadDouble/Write*, Seek, Skip, Tell, Size, EndOfFile, Flush, Close).
  Запись — только в `DATA` и только внутрь `data/`, как в GMod; белый список расширений GMod для записи.
- `glua_sql.cpp`: `sql.Query`, `QueryRow`, `QueryValue`, `SQLStr`, `LastError`, `TableExists`, `IndexExists`, `Begin`/`Commit` на SQLite
  (amalgamation, закрепи версию и хеш; база `sv.db` на сервере / `cl.db` на клиенте в корне игры).
- `glua_misc.cpp`: `system.*`, `SysTime` и прочие функции из спецификации с `native == true`, которые не трогают сущности,
  рендер, сеть, vgui и состояние движка. На iOS `system.IsOSX()` возвращает true (аддоны ветвятся по ОС, ближе всего macOS);
  `IsWindows`/`IsLinux` — false. Список выбранных функций — в PR.

### L3. Тестовый стенд на Windows — `ios_m3/tests/glua/`
- CMake-проект: exe `glua_test`, линкует `gluavm.lib`, твои `glua_*.cpp`, нужные исходники `mathlib`/`tier1` из `deps/engine`
  (заглушку файловой системы сделай на `IBaseFileSystem` поверх временной папки).
- `tests/glua/*.lua` — тесты на `assert`, по одному на библиотеку, значения из примеров wiki.
- Отчёт покрытия: стенд загружает `lua/includes/init.lua` Facepunch с автозаглушками для неизвестных глобалов/методов и пишет JSON со списком
  вызванных, но не реализованных функций (`out/glua_missing.json`) — по нему Aero выбирает следующий объём работ.

**Готово, когда:** `glua_test` проходит все тесты, `include("includes/init.lua")` в стенде доходит до конца, есть `glua_missing.json`,
PR в `gpt/glua` с логами. На iOS это пока не подключается — интеграцию в client/server делает Aero.

## Часть Aero (для сведения, не делай)

Среда выполнения: Lua-состояния client/server/menu, `include`/`AddCSLuaFile`/`CompileString`, поиск Lua-файлов (gamemodes, addons),
вызовы хуков GM из движка, метатаблицы Entity/Player/Weapon/NPC/PhysObj, SENT/SWEP (scripted_ents/weapons), NW/DT-переменные, `net`,
загрузка gamemode base → sandbox, `surface`/`draw`/`render`/`cam`, vgui Panel + Derma (Q/C-меню, HUD), ConVar/concommand, input, sound,
тулган/физган через родной Lua. Отладка — на Windows-сборке, потом iOS без изменений.

## Порядок

L1 → L3 (минимальный стенд с тестами типов) → L2. Присылай по этапам, не пачкой.
