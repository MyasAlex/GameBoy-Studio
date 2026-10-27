# GameBoy Studio 0.9 beta — фактический статус

Это большая функциональная бета. **Весь целевой список ещё не завершён.**
Полный запрос: REQUEST_RU.md. История: FEATURE_STATUS_0.8_RU.md.

| Блок | Добавлено и доступно | Что не закрыто / границы проверки |
|---|---|---|
| Ядро | CGB compatibility для DMG с BG/OBJ палитрами; MBC7 EEPROM/наклон; HuC1/IR; Camera FC | FIFO/OAM и тонкие APU эффекты ещё неточны. Pocket/Light/ревизий нет; MBC6/MMM01/HuC3/TAMA5 нет. Штатная таблица палитр CGB по играм без boot ROM не встроена; оригинальные boot ROM и реальные игры не проверялись |
| Link | Два локальных ядра, serial bits/IRQ, окно и ввод игрока 2, IR peer | Чередование по инструкциям; все быстрые serial краевые эффекты не доказаны. Реальный обмен Pokémon не проверен |
| Сеть | TCP host/join по IPv4; две ROM проверяются; обе консоли синхронизируются BESS; input prediction и двухконсольный rollback; ручное переподключение через новую сессию | Две сессии проверены на loopback. Реальные LAN/интернет/NAT не проверены. Нет matchmaking, шифрования, аутентификации, автоматического reconnect, чата. Живые Camera/tilt входы не передаются; такие картриджи отклоняются |
| Аксессуары | Printer packets/checksum/RLE/тайлы/BMP; Camera регистры/пороги/2bpp/busy/pause/resume; BMP и VfW webcam в GUI; MBC5 rumble через XInput; MBC7 наклон через Num4/6/8/2 | SGB/SGB2 нет. Camera аналоговые фильтры/усиление и частичное заполнение RAM упрощены; полный снимок записывается в конце таймера. MBC7 EEPROM программируется сразу, без аналогового шума/settle time. MBC7 rumble не моделируется. Физические устройства и Camera ROM не проверены |
| Графика | Direct3D 11, WARP fallback, CRT искривление/свечение/scanlines/vignette; Scale2x/3x; цветовая матрица/gamma; пресеты игры | GUI/GPU не прогонялись на Windows. Пользовательские шейдеры, xBRZ/HQ отсутствуют. Scale2x/3x, ghosting и цветокоррекция остаются на CPU. Пресеты загружаются вручную; LCD профили не калиброваны |
| Звук | 44.1/48/96 кГц, 3-band EQ, поканальные уровни и осциллограммы; WAV/FLAC | FLAC verbatim, с CRC, почти без сжатия и без MD5. Независимый декодер восстановил побайтово совпадающий PCM. Слышимое качество и задержка устройства не проверены |
| Состояния | BESS 1.1 CORE/MBC/RTC и GBSP v2 с явным LE-кодированием всех полей; quick/named/auto save; точки с BMP/комментариями/ветками | Внешний BESS импортируется best effort; состояния другого эмулятора ещё не проверены. Native GBSTATE6 внутри rewind/movies зависит от ABI. Это дерево точек, не шкала каждого кадра. Обычные слоты/rewind одной консоли при Link запрещены |
| Читы | Game Genie 6/9 цифр заменяет ROM-чтения; GameShark/AR тип 01 пишет RAM; менеджер/сохранение; поиск/заморозка RAM | Другие типы AR/GS отклоняются. RetroAchievements/Hardcore нет. Читы/редактирование отключены при сети |
| Studio | Реальные CPU read/write watchpoints, включая прежнее значение и заблокированный доступ; bank breakpoints API; память/тайлы редактируются; VRAM/OAM/BG/APU графика; CPU profiler; ограниченная Lua 5.4.8 и Lua-плагины | Нет DMA/PPU bus watchpoints, полного FIFO/регистрового инспектора и графического редактора объектов/карт/палитр. Один активный скрипт, без DLL ABI. Инструкции профилируются в Studio execution |
| Библиотека / Windows | Поиск, избранное, BMP обложки, заметки, эмулированное время; ZIP с одной ROM, одиночный GZIP, CRC/лимиты; raw AVI; переназначение 15 команд; установщик/ярлыки/Open With | PNG/JPEG и автозагрузка обложек не поддержаны. AVI без звука/шейдеров, лимит около 1.9 ГБ. Вторые клавиши/XInput раскладки фиксированы. Установщик PowerShell/bat, не MSI; выполнение на Windows не проверено |
| Интеграции | GitHub release check по запросу; Issue JSON с host diagnostics; Lua plugins; скрипт подписи своим сертификатом | Discord/RetroAchievements нет. Update check не проверен на Windows. EXE не подписан: сертификат не предоставлен |
| Особые режимы | Диапазонный TAS input editor и replay; декоративная рамка | Camera/tilt ввод в movies не записывается. Нет полноценной TAS timeline, сравнения моделей, интерактивного корпуса, homebrew каталога и общей базы совместимости |
| Проверки | Regression/integration, ROM/archive/state mutations, ASan/UBSan и core benchmark | Это не непрерывный coverage-guided fuzzing. LeakSanitizer недоступен. Слабый физический ПК, устройства и длительные игровые прохождения не проверены |

## Полученные результаты

| Проверка | Результат |
|---|---|
| Unit/integration CTest | 12/12 PASS |
| Исходный DMG: Blargg/acid2/Mooneye | 130 PASS, 0 FAIL |
| CGB: Blargg/acid2/demo | 15 PASS, 0 FAIL |
| SameSuite, смешанные CGB ревизии | 20 PASS, 58 FAIL |
| Mealybug, DMG blob изображения | 1 PASS, 23 FAIL |
| ASan/UBSan | См. validation-0.9.json; leak detection отдельно не подтверждён |
| FLAC independent decode | PCM совпал с оригиналом |
| AVI independent probe | Rawvideo, 160×144, 10 кадров, 59.7275 fps |
| Windows x64 | Компиляция/линковка успешны; только системные DLL; GUI/HW не прогонялись |

JSON: 0.9-dmg-results.json, 0.9-cgb-results.json, 0.9-extended-results.json,
0.9-benchmark.json и validation-0.9.json. 130/15 PASS не гарантируют идеальную
работу Mario/Tetris и каждой другой игры. Коммерческие игры и оригинальные
Nintendo boot ROM для проверки не предоставлены и не включены.

Следующий необходимый этап — исправление конкретных провалов SameSuite/
Mealybug с регрессионными проверками, затем реальные игры, Pokémon Link,
внешние BESS и Windows устройства. Незавершённые пункты не скрыты как готовые.
