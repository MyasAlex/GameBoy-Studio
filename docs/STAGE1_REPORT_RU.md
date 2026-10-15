# GameBoy Studio — результат первого этапа

14 октября 2026, Windows 10 x64. Подготовлена нативная сборка MSVC с исходниками и достоверными проверками. **Весь список функций ещё не закрыт: остаются 11 Mealybug FAIL и три Boot-проверки.**

## Что исправлено

- Сборка MSVC: отключены конфликтующие макросы Windows для общих заголовков и тестов; камера использует библиотеку `vfw32` из Windows SDK. Для MinGW сохранена `avicap32`.
- Legacy-сохранения: PPU/APU записываются с нулевыми байтами выравнивания. Размер и расположение данных сохранены, старый reader не менялся. Тест намеренно изменяет только padding и проверяет одинаковые состояния, BESS-импорт и продолжение игры в DMG/CGB/compatibility.
- PPU-тест: успешный код завершения и `RESULT: PASS` выдаются только после достижения software breakpoint. Исчерпание бюджета возвращает ошибку. Конфликтующие протоколы отклоняются.
- Extended runner: корректный выбор SGB на Windows, удаление старого снимка перед запуском, обязательный свежий снимок и явное завершение теста. Устранены два ложных Windows FAIL в SameSuite.
- Windows QA: надёжный код завершения, Unicode/сложные аргументы, одновременное чтение stdout/stderr, timeout и освобождение ресурсов; отдельные кадры DMG/CGB с проверкой размера и непустого изображения. Отчёт можно сохранить через `-OutputFolder`.
- Boot runner: добавлен внешний `--boot-rom` для DMG-наборов, проверка размера до запуска и SHA256 firmware в отчёте. Никакие оригинальные прошивки не скачивались.
- В GitHub Actions добавлены новые проверки протоколов и Windows QA. Сам workflow в GitHub здесь не запускался.

## Проверено на финальной сборке

| Проверка | Результат |
|---|---|
| Нативная сборка MSVC 19.51, RelWithDebInfo | PASS |
| Внутренние нативные проверки | 16/16 PASS через ограниченный по времени runner |
| Новые Python-проверки | 15/15 PASS |
| PowerShell QA: exit codes, Unicode, кавычки, большие потоки, timeout, кадры | PASS |
| Windows CPU rendering self-test и два демо | 3/3 PASS; по 600 кадров |
| Кадры демо | 160×144; DMG 4 цвета, CGB 27 цветов |
| SameSuite | 78/78 PASS |
| Mealybug DMG | 23 PASS, 1 FAIL |
| Mealybug CGB C | 21 PASS, 6 FAIL |
| Mealybug CGB D | 16 PASS, 4 FAIL |
| Завершённые Mealybug-снимки | 71/71 |
| Новые расхождения относительно preview | 0; хеши ROM/эталонов и число отличающихся пикселей совпали |

Общий CTest на этом ПК зависал при сборе результата `accuracy`. Те же 16 команд затем успешно выполнены через надёжный process runner; отдельный `accuracy` завершился с кодом 0 за 8,83 с. Поэтому полный CTest-прогон не помечен PASS. Детали всех 16 проверок находятся в `stage1-native-tests.json`.

Окно приложения запускалось и сообщало о работе демо примерно на 60 FPS. Инструмент захвата окна дважды вернул timeout, поэтому визуальная проверка меню, GPU, управления и полного GUI не засчитана. CPU rendering self-test и исходные видеокадры проверены отдельно.

## Оставшиеся Mealybug-расхождения

| Модель | ROM | Отличающихся пикселей |
|---|---|---|
| CGB-C | `m3_lcdc_obj_size_change_scx` | 15 |
| CGB-D | `m3_lcdc_obj_size_change_scx` | 15 |
| CGB-C | `m3_lcdc_tile_sel_change` | 8 |
| CGB-D | `m3_lcdc_tile_sel_change` | 520 |
| CGB-C | `m3_lcdc_tile_sel_change2` | 186 |
| CGB-C | `m3_lcdc_tile_sel_win_change` | 8 |
| CGB-D | `m3_lcdc_tile_sel_win_change` | 260 |
| CGB-C | `m3_lcdc_tile_sel_win_change2` | 179 |
| DMG | `m3_lcdc_win_en_change_multiple_wx` | 43 |
| CGB-C | `m3_wx_4_change_sprites` | 10 |
| CGB-D | `m3_wx_4_change_sprites` | 10 |

Три исходных Boot FAIL (`boot_div`, `boot_hwio`, `boot_sclk_align`) сравнивают состояние после запуска оригинальной прошивки. Текущая replacement firmware имеет другую последовательность старта и фазу DIV/LCD/serial. Это требует отдельной проверки с пользовательской оригинальной DMG-прошивкой; менять общие тайминги ради этих трёх тестов без неё нельзя считать подтверждённым исправлением.

Реальные игры, обмен Pokémon, устройства, LAN/WAN, серверный Hardcore, внешний BESS roundtrip и цифровая подпись остаются открытыми. Windows 11 отдельно не проверена. Предыдущие `1.0-*-results.json` и `validation-1.0.json` сохранены как история исходного preview; актуальные хеши находятся в `stage1-validation.json` и `SHA256SUMS.txt`.

## Следующий небольшой этап

Начать с двух PPU-ROM: `m3_lcdc_obj_size_change_scx` и `m3_wx_4_change_sprites` на CGB C/D. Условие завершения — совпадение с эталонами и отсутствие новых регрессий. Остальные этапы перечислены в `STAGES_RU.md`.

Для запуска распакуйте весь архив и откройте `START_WINDOWS.bat`. Исходники и сборка находятся вместе. Отчёты: `stage1-validation.json`, `stage1-native-tests.json`, `stage1-windows-qa.json`, `stage1-extended-results.json`; логи и raw-кадры — `stage1-qa/`.
