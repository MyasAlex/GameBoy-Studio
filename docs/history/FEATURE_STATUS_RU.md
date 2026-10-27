# GameBoy Studio 1.0 preview — фактический статус

Третий этап от 15 октября 2026: исправлен долг времени после Turbo, ограничены догоняющие кадры и добавлены Ctrl+0 / индикатор темпа. Шесть замеров настоящего Windows-цикла прошли со звуком и без. Постоянное ускорение на доступных ROM не воспроизвелось. Отчёт — [STAGE3_REPORT_RU.md](STAGE3_REPORT_RU.md), план — [STAGES_RU.md](STAGES_RU.md).

Ядро этапа 2 не изменено: после четырёх исправлений остаются **7 Mealybug FAIL и 3 Boot-проверки**. Полные PPU-результаты этого ядра — в [STAGE2_REPORT_RU.md](STAGE2_REPORT_RU.md); на этапе 3 они повторно не запускались. Отчёт первого этапа — [STAGE1_REPORT_RU.md](STAGE1_REPORT_RU.md).

Версия существенно расширена по всем перечисленным направлениям. **Полный список нельзя считать завершённым:** ниже отделены реализованные функции, выполненные проверки и оставшиеся ограничения. История исходного ядра: FEATURE_STATUS_0.8_RU.md и FEATURE_STATUS_0.9_RU.md. Полный исходный запрос: REQUEST_RU.md.

Основное ядро — SameBoy 1.0.4 с патчами Studio, свободной replacement firmware и C ABI v3. Собственное ядро сохранено как legacy. Это архитектурное изменение указано явно; новый код не выдаётся за полностью собственное аппаратное ядро.

| Направление | Реализовано | Что остаётся / предел проверки |
|---|---|---|
| Темп игры Windows | Сброс долга после Turbo/смены скорости/паузы; максимум два догоняющих кадра на 1×; Ctrl+0; выбранная скорость и измеренный процент; диагностический Win32-режим | 6/6 замеров DMG/CGB/CGB double speed со звуком и без прошли. Это скрытое окно с собственными ROM; конкретная проблемная игра, GPU-показ и слышимое качество не проверены. Постоянное ускорение не воспроизведено |
| Точность ядра | Native FIFO/OAM/APU; DMG-B, MGB/Pocket/Light, CGB 0/A/B/C/D/E, AGB; цветные BG/OBJ в DMG compatibility; MMM01/HuC1/HuC3/MBC7/Camera; отдельный MBC6; уточнены CGB OBJ low-address latch и повторное включение WX | Mealybug: 7 FAIL по DMG/CGB. Boot handoff: 3 FAIL с replacement firmware. Оригинальные Boot ROM, реальные игры и все unit-specific OAM-паттерны не проверены. Pocket/Light используют общий MGB electrical model. TAMA5 не добавлен. MBC6 busy timing/modem не моделируются точно |
| Link / сеть | Два ядра, serial bits/IRQ, игрок 2; точные парные состояния с расписанием кабеля; TCP protocol 10, rollback, heartbeat, повторное соединение и host resync | Проверены actual-core loopback, поздние вводы и disconnect/reconnect. Pokémon trade и реальные LAN/WAN/NAT не прогонялись. IPv4/доступный порт, без relay/TLS/auth. Camera/наклон в сетевой сессии запрещены |
| Аксессуары | SGB/SGB2/PAL HLE, рамки и команды/палитры; Printer/BMP; Camera/BMP/VfW; IR peer/HuC; MBC5/MBC7 rumble; клавиши/стик наклона | SGB не имеет полного SNES-звукового ядра/LLE. Camera sensor упрощает реальную аналоговую экспозицию. Веб-камера, реальные геймпады, IR/Printer и rumble-устройства не проверены. Восстановление IR/rumble сигналов из состояния проверено |
| Графика | Direct3D 11/WARP; CRT warp/glow/scanlines/vignette; GPU Scale2x/3x/HQ2x, матрица/gamma и ghosting; CPU fallback; xBRZ 3x в отдельном GPL-процессе; автопресеты каждой игры | DXC проверил HLSL syntax/semantics для SM6.0; Windows D3DCompile SM4.0 и устройство не выполнялись. Shader effects остаются встроенными; нет редактора произвольных shader chains. SGB border использует GDI. Физическая калибровка CRT/LCD не выполнена |
| Звук | 44.1/48/96 кГц, EQ, поканальные уровни, scope, WASAPI/waveOut, WAV/FLAC; исправлена смена native sample rate | FLAC verbatim с CRC, почти без сжатия и без MD5. PCM roundtrip независимым декодером проверен. Слышимое качество и реальная задержка Windows-устройства не измерены; host resampler history перезапускается при загрузке |
| Сохранения | Стандартные BESS-блоки + GSTU/GBSX; ROM/model/hash guards; точное продолжение 13 моделей; framebuffer/рамка, ввод, Camera, flash и achievement progress; quick/named/auto/rewind; точки/BMP/комментарии/ветки/graph; paired Link | Exact private section предназначена для этого ядра/ABI; стандартный BESS остаётся переносимой частью. Foreign BESS проверяется/импортируется с потерей некоторых внутренних фаз. Реальный roundtrip с другой программой не выполнен. Старые GBSTATE3–6 мигрируют best effort. Это дерево контрольных точек, не видеозапись каждого прошлого кадра |
| Читы / достижения | Рабочие GG 6/9; GS/AR 01, 80–8F SRAM, 90–97 WRAM; менеджер, наборы, поиск/freeze; настоящий rc_client, ROM identity, RAM mapping, события/очереди unlock, сохранение прогресса и ограничения Hardcore | Другие GS/AR типы отклоняются. rc_client протестирован с офлайн mock HTTP, не реальным аккаунтом. Официальное approval и серверный Hardcore не подтверждены. Ограничения редакторов/состояний/перемотки/замедления/Link и pause cooldown реализованы локально |
| Studio | Память/дизассемблер/регистры, CPU и bank breakpoints, CPU/DMA bus events, optional PPU VRAM/OAM watches, native FIFO/fetcher/APU данные и scope; tile/OAM/map/palette editors; CPU profiler; Lua | Остановка и просмотр на границах инструкции, без отдельного GUI-шагания каждого dot/M-cycle. Bus callbacks отражают также промежуточные I/O операции native conflict logic. Профилировщик собирает инструкции в Studio execution, не системный sampling profiler. DLL plugin ABI не добавлен |
| Библиотека / Windows | Обложки BMP/PNG/JPEG, поиск/избранное/заметки/эмулированное время; ZIP/GZIP+CRC/лимиты; WIC MotionJPEG + PCM AVI; два XInput-профиля; 15 hotkey remaps; installer/shortcuts/Open With/uninstall; MSVC/MinGW build paths | На этапе 1 выполнены нативная сборка MSVC, CPU rendering self-test и два демо с кадрами 160×144. Полная проверка меню/GPU/input не завершена. AVI — raw LCD, без GPU-эффектов/SGB-рамки, до 1.9 ГБ. Вторые клавиши клавиатуры фиксированы. Installer PowerShell/bat, не MSI; установка и ассоциации требуют проверки |
| Интеграции | Discord local IPC + ping/pong и Application ID; GitHub release check; до 8 изолированных Lua plugins; Issue JSON; signing всех EXE/DLL собственным сертификатом | Discord/updates/реальный RA transport на Windows не выполнялись. Пользователь задаёт Discord Application ID и RA token. EXE/DLL **не подписаны**: доверенный сертификат не предоставлен. Публикация релиза в GitHub не выполнялась |
| Особые режимы | TAS input grid/range edit/preview/trim/replay с motion/Camera capture; четыре software hardware-model panes; граф веток; интерактивные кнопки корпуса; homebrew hub; searchable compatibility TSV | TAS не имеет произвольного монтажа input/camera timeline и общих многоконсольных movies. Сравнение моделей программное, не измерение настоящих консолей. Корпус один с темами. Homebrew hub содержит два собственных демо, импорт и внешний каталог авторов. Compatibility DB хранит тесты, не недоказанные рейтинги коммерческих игр |
| Дополнительные проверки | Native regression/integration, 800 legacy state + 512 accuracy mutations, 250 ROM/archive mutations; настоящий GCC trace-pc feedback fuzzer с ASan/UBSan; core benchmark; длительный CGB demo run; Windows QA script | Конечный fuzz run не гарантирует отсутствие всех ошибок. LeakSanitizer здесь блокируется окружением; ASan/UBSan выполнялись с detect_leaks=0. Слабый физический ПК, длительные реальные прохождения, слышимое качество и устройство управления не проверены |

## Исходные результаты preview до первого этапа

| Проверка | Результат |
|---|---|
| Native CTest | 16/16 PASS |
| ASan + UBSan | 16/16 PASS, затем новые PPU/IR/rumble проверки 2/2 PASS |
| DMG Blargg / acid2 / Mooneye | 127 PASS, 3 FAIL (boot_div, boot_hwio, boot_sclk_align) |
| CGB Blargg / acid2 / demo | 15/15 PASS |
| SameSuite с правильными моделями | 78/78 PASS |
| Mealybug DMG blob | 23 PASS, 1 FAIL |
| Mealybug CPU CGB C | 21 PASS, 6 FAIL |
| Mealybug CPU CGB D | 16 PASS, 4 FAIL |
| Exact states / BESS / continuation | 13 моделей, corruption reject; MBC6 flash и paired mid-serial restore PASS |
| Actual-core TCP | State sync, late input convergence, automatic host reconnect/resync PASS |
| rc_client офлайн | Login, hash, RAM trigger, award request, progress restore, logout-independent core state и Hardcore reset PASS |
| Coverage-guided loader fuzz | 1810 входов / 30 с, ASan/UBSan; 6 новых входов корпуса, 1756 hashed slots; не процент покрытия |
| AVI independent decode | 12 raw frames + 9600 stereo samples / 48 кГц; PCM совпал побайтово |
| HLSL | Microsoft DXC vertex/pixel SM6.0 PASS; SM4/device отдельно не проверены |
| xBRZ pipe | Solid-color roundtrip, dimensions и rejection больших входов PASS |
| Windows x64 | EXE/DLL/helper/headless cross-compiled; native GUI/device run не выполнен |

Числа относятся к этому контейнерному прогону, а не к пользовательскому ПК. Точные входные SHA256 и провалы находятся в 1.0-*-results.json. validation-1.0.json содержит хеши финальных файлов, логи, benchmark и длительный demo run. CI сохраняет известные FAIL открытыми и отдельно проверяет, что не появились новые регрессии.

Чтобы закрыть оставшиеся проверки, нужны предоставленные пользователем реальные ROM/Boot ROM, физические устройства, тестовая LAN/WAN, настоящий RetroAchievements аккаунт/approval и code-signing сертификат. Обновлённый RUN_WINDOWS_QA.ps1 записывает фактические результаты и снимки кадров; успешное выполнение ROM отмечается как проверка запуска, а не доказательство совместимости игры. Полный список остаётся незавершённым.
