# GameBoy Studio 1.0 preview — Windows x64

[Скачать Windows ZIP и EXE — v1.0.0-preview.4](https://github.com/MyasAlex/GameBoy-Studio/releases/tag/v1.0.0-preview.4). Для запуска распакуйте Windows ZIP целиком: отдельному EXE требуется DLL из того же релиза.

Этап 4 исправляет оставшиеся Mealybug и Boot-проверки, ошибки потери фокуса Windows и добавляет воспроизводимые прогоны опубликованных игр. Точные результаты и ограничения — в **docs/STAGE4_REPORT_RU.md**. Полная ручная приёмка на настоящей Windows, слышимый звук и физические устройства пока не проверены; приложение остаётся preview.

Версия внутренних снимков — **18**: сохраняется отдельное значение видеошины. Снимки версии 17 импортируются через BESS без точного продолжения прежних фаз. Для старых TAS и детерминированных повторов сохраните прежнюю сборку. История предыдущих исправлений — в **docs/STAGE2_REPORT_RU.md** и **docs/STAGE3_REPORT_RU.md**.

В этой версии основным стало ядро **SameBoy 1.0.4 с доработками Studio**. Собственное ядро 0.9 сохранено для старых состояний и запуска `gb_headless --core original`. Добавлены модели Pocket/Light/CGB/SGB, сеть с восстановлением, аксессуары, RetroAchievements, новые редакторы и фильтры.

Полный запрос пока нельзя считать закрытым: остаются проверки на настоящей Windows, физических устройствах и коммерческой библиотеке. Точный статус каждого направления и все провалы сохранены в **docs/FEATURE_STATUS_RU.md**; результаты сборки — в **docs/stage4-validation.json**.

## Запуск на Windows 10/11

1. Распакуйте **весь архив**.
2. Откройте **START_WINDOWS.bat** или **dist/GBStudio.exe**. Visual Studio для запуска не нужен.
3. Ctrl+O или перетаскивание открывает свою `.gb`, `.gbc`, `.zip`, `.gz`.

Рядом с программой нужны `gb_accuracy.dll` и `gb_pixel_filters.exe`. Они включены в dist. Lua, miniz и rcheevos собраны статически; установка SDL/MinGW не требуется. В архиве два собственных демо, исходники и результаты проверок; коммерческие игры и оригинальные Nintendo Boot ROM не включены.

**INSTALL_WINDOWS.bat** создаёт копию в `%LOCALAPPDATA%\GameBoy-Studio`, ярлыки и регистрацию «Открыть с помощью». Приложение по умолчанию выбирается средствами Windows. **UNINSTALL_WINDOWS.ps1** удаляет ярлыки/регистрацию, сохраняя файлы и сохранения. Программа пока **без цифровой подписи**: SIGN_WINDOWS.ps1 подписывает все EXE/DLL только вашим настоящим сертификатом.

## Управление

| Действие | По умолчанию |
|---|---|
| Игрок 1 | Стрелки; Z/X; Shift/Enter |
| Игрок 2 при Link | WASD; G/H; T/Y; фокус главного окна |
| MBC7 наклон | Num4/6, Num8/2; правый стик XInput |
| Пауза / быстрое сохранение / загрузка / кадр | P; F5; F9; F6 |
| Полный экран / BMP | F11; F12 |
| Перемотка / turbo | Удерживать Backspace / Tab |
| Обычная скорость 1× / отмена turbo | Ctrl+0 или меню времени → скорость → 1× |
| CPU Studio | Ctrl+D |

Controls меняет игровые клавиши. Remap emulator hotkeys меняет 15 команд с Ctrl/Shift/Alt. 1–9 выбирают слот, Esc закрывает полный экран. Два профиля XInput настраивают устройство, dead zone и кнопки; профили можно импортировать и экспортировать. Виртуальные кнопки корпуса управляются мышью.

В нижней строке на обычной скорости ожидаются 1×, около 100% и 59,73 FPS. Процент появляется после первого интервала измерения и может немного колебаться. Пользовательское назначение Ctrl+0 имеет приоритет; в этом случае выберите 1× в меню. Воспроизводимый замер реального Windows-цикла описан в **docs/STAGE3_REPORT_RU.md**.

## Меню и функции

- **Hardware**: DMG-B, Pocket/Light, CGB 0/A/B/C/D/E, AGB, SGB/SGB2/PAL. DMG-игры на CGB получают цветные BG/OBJ-палитры через свободную replacement boot firmware SameBoy. Свои оригинальные boot-файлы можно выбрать отдельно.
- **Display**: Direct3D 11, CRT с искривлением/свечением/scanlines/vignette, GPU Scale2x/3x/HQ2x, цветокоррекция и ghosting. CPU fallback сохранён. xBRZ 3x работает отдельной программой-фильтром. Настройки экрана автоматически сохраняются по хешу игры; есть ручные пресеты.
- **Audio**: 44.1/48/96 кГц, EQ, уровни четырёх каналов, осциллограммы, WAV/FLAC. FLAC lossless с почти несжатыми блоками. Запись AVI содержит сырой LCD 160×144, MotionJPEG через WIC и PCM stereo; максимум около 1.9 ГБ. GPU-эффекты и SGB-рамка в видео не включаются.
- **Cheats / RAM**: действующие Game Genie 6/9, GameShark/Action Replay 01, 80–8F и 90–97, сохранение наборов, поиск/заморозка RAM. Неподдержанные типы кодов отклоняются.
- **Studio**: CPU/память/дизассемблер, bank breakpoints, read/write watches CPU/DMA и отдельное включение чтений PPU; реальные FIFO/состояния fetcher/APU; редактирование тайлов, OAM, BG map и RGB15-палитр; профилировщик. Lua API — docs/LUA_API_RU.md. До восьми Lua-плагинов запускаются вручную в отдельных ограниченных VM.
- **Time**: BESS, слоты, rewind, миниатюры точек, комментарии и ветки; граф временной шкалы. TAS имеет таблицу покадровых кнопок, просмотр до выбранного кадра, обрезку и replay; movies сохраняют также наклон и снимки Camera.
- **Library**: поиск, избранное, заметки, эмулированное время и BMP/PNG/JPEG-обложки. База совместимости показывает записанные тесты. Homebrew-окно содержит собственные демо, импорт и ссылку на каталог авторов gbdev.
- **Integration**: RetroAchievements через официальный rc_client; Discord Rich Presence через локальный IPC и ваш Application ID; проверка GitHub releases по запросу; диагностический JSON для Issues. Токен RetroAchievements хранится только в памяти.

**Hardcore** ограничивает состояния/перемотку/читы/редакторы/замедление/Lua/TAS/Link и перезапускает игру при включении. Официальное одобрение клиента и проверка с настоящим аккаунтом ещё не получены; сервер может отключить Hardcore. Локальные тесты проверяют настоящий runtime rc_client с подменённым HTTP-сервером.

## Link и аксессуары

Local Link загружает вторую ROM и соединяет две консоли на уровне serial bits; звучит первая. Игрок 2 использует отдельные `.sav`, `.rtc`, `.sbbattery`. Одинаковые ROM не делят батарею. **Save/Load linked pair** сохраняет обе консоли и расписание кабеля, включая середину передачи. Обычные слоты одной консоли при Link запрещены.

Для сети на обоих ПК сначала загрузите две одинаковые ROM в одинаковом порядке через Local Link. На первом выберите host (TCP 8765), на втором join с IPv4/портом. Host управляет первой консолью, guest второй. Передаются состояния/ввод, ROM не передаются. Протокол 10 требует одинаковой версии. Есть rollback, heartbeat и автоматические повторы соединения с полным повторным состоянием host. Проверены loopback, запоздалый ввод и переподключение; реальная WAN/NAT ещё не проверена. Прямое соединение требует доступного TCP-порта; transport без шифрования/аутентификации. Camera/наклон через сеть пока запрещены.

SGB/SGB2 используют HLE команд, палитр и рамок; отдельное SNES-звуковое ядро не включено. Printer экспортирует BMP-страницы. Camera получает BMP/веб-камеру (VfW/RGB24, включается вручную). IR связывает две локальные консоли, включая HuC; rumble использует XInput. Поддержаны MMM01, HuC1/HuC3, MBC7 и MBC6; flash MBC6 не моделирует точное время busy/modem.

## Данные и сохранения

Основные файлы состояния — **BESS со стандартными блоками и расширениями GSTU/GBSX**. Они сохраняют точное продолжение этого ядра, ввод, изображение/рамку, Camera, MBC6 и прогресс достижений. Другой BESS импортируется с проверкой ROM/модели, с возможной потерей внутренних фаз. Старые GBSTATE3–6 мигрируют через BESS с потерей части точных фаз. Программы другого эмулятора для внешнего roundtrip ещё не прогонялись.

Native RTC/flash сохраняются в `.sbbattery` рядом с ROM; обычная RAM также экспортируется в `.sav`. Для переноса батареи копируйте оба файла. Настройки — рядом с EXE, при отсутствии прав в LocalAppData; архивные ROM помещаются в ArchiveCache, читы/пресеты/точки — в Games по хешу, игрок 2 — в Player2. Личные ROM, токены и сохранения в этот архив не включены.

## Сборка и проверка

Visual Studio 2022/2026: **Desktop development with C++** и **CMake tools**. Откройте GBStudio.sln либо запустите **BUILD_WINDOWS.bat --no-launch**. Оба пути используют CMake, тесты и установку в dist. MSVC собирает frontend/инструменты и использует включённую C ABI DLL ядра (GNU C). MinGW может пересобрать DLL из всех включённых исходников. Для Ninja задайте доступный компилятор в выбранной среде.

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python tests/run_rom_tests.py --emulator build/gb_headless --download --mooneye
python tests/run_cgb_tests.py --emulator build/gb_headless --download
python tests/run_extended_tests.py --emulator build/gb_headless --suite PATH_TO_C_SP_V7
```

Windows использует `.exe` и конфигурацию `--config Release` / `ctest -C Release`. Последние ROM-наборы содержат настоящие FAIL, они не скрываются. tests/benchmark_core.py измеряет только ядро; RUN_WINDOWS_QA.ps1 сохраняет отдельный отчёт реального Windows-прогона. GUI, GPU, звук, геймпады, webcam и подписанный выпуск требуют проверки на настоящей Windows. Полные лицензии и исходники зависимостей — docs/THIRD_PARTY_NOTICES.md и third_party.


Coverage-guided проверка состояния/BESS на Linux GCC:

```text
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGB_BUILD_COVERAGE_FUZZER=ON -DBUILD_TESTING=OFF
cmake --build build-coverage --target gb_coverage_fuzzer --parallel 2
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 build-coverage/gb_coverage_fuzzer demo.gb build-coverage/corpus 30
```

Фаззер использует настоящий trace-pc feedback, сохраняет новые входы корпуса и последний reproducer. Хешированные coverage slots не являются процентом покрытия. Выполненный конечный прогон записан в docs/1.0-coverage-fuzz-results.json.
