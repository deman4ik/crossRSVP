# Можно ли ускорить refresh Xteink X3 для RSVP

Дата исследования: 11 сентября 2026 года. Исходники Free-Ink проверены на
revision `fde240faaeae6c340dacd435a4f77d2ef2f82dfd` (локальный submodule
`freeink-sdk`). Отчёт рассматривает только исследование и план проверки: прошивка
не менялась, сборка и аппаратный эксперимент для этого отчёта не выполнялись.

В тексте явно разделены три вида свидетельств: **исходник SDK/репозитория**,
**измерение пользователя** и **инференция**. Ссылки на generic datasheet не
подменяют квалификацию конкретного X3-модуля.

## Ответ

В текущих источниках partial window даёт две проверяемые гипотезы: уменьшить
передачу RAM и изменить затворный участок waveform через `PT_SCAN`. В B/W пути
RSVP ни одна из них ещё не реализована или не измерена. Для UC8253 драйвер
каждый FAST-кадр отправляет полный DTM2, запускает DRF, ждёт BUSY и затем отправляет
полный DTM1 для следующего differential-кадра. Это видно в pinned
[Uc8253X3Driver.cpp:164–218](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L164-L218)
и [Uc8253X3Driver.cpp:221–279](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L221-L279).

Тот же источник показывает, что `displayWindow()` у `Uc8253X3Driver` не
переопределён. Виртуальный fallback в
[PanelDriver.h:52–57](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/PanelDriver.h#L52-L57)
вызывает обычный полный FAST-refresh; facade делает такой же полный refresh, если
инверсия включена или её состояние изменилось, в
[FreeInkDisplay.cpp:785–804](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/FreeInkDisplay.cpp#L785-L804).
Следовательно, сам вызов `displayWindow()` сейчас не означает передачу только
окна.

Partial window всё же может сократить объём SPI-данных, если для конкретного
контроллера будут одновременно доказаны корректная геометрия окна, old/new RAM
семантика и подходящая waveform. Но окно само по себе не означает «короткую
волну»: команда DRF по-прежнему выбирает загруженную LUT и завершает внутреннюю
операцию по BUSY. Общий [даташит UltraChip UC8253c, pp. 19 и 36](https://files.waveshare.com/wiki/3.52inch%20e-Paper%20HAT/UC8253c.pdf)
описывает DTM1/DTM2, DSP/DRF, BUSY_N и PTL/PTIN/PTOUT, но это generic TFT-module
даташит, а не квалификация стекла Xteink X3. Поэтому безопасный вывод —
«partial RAM upload стоит исследовать», а не «одна строка уже ускорит X3».

Измерение пользователя даёт для этого экземпляра около 475 мс среднего FAST-вызова
и плато около 95 кадров/мин при 125–200 слов/мин. Эти данные находятся в
[rsvp-display-limits.md:62–85](rsvp-display-limits.md#L62-L85). Интервал плато
около 632 мс включает больше, чем один `display()`; разность около 157 мс нельзя
приписать только waveform, SPI или CPU без отдельного замера. Новый потолок в
WPM из арифметики `60000 / 475` не следует.

## Что именно занимает время

### 1. Передача данных в RAM контроллера

Generic UC8253 datasheet говорит, что DTM1 и DTM2 передают host data в SRAM, DSP
заканчивает ввод одного кадра, а DRF запускает вывод по SRAM и LUT; эти команды
различают загрузку RAM и сам refresh ([UC8253c datasheet, pp. 19–20](https://files.waveshare.com/wiki/3.52inch%20e-Paper%20HAT/UC8253c.pdf)).
Это аппаратное описание, применимость которого к X3 следует подтверждать кодом
драйвера.

В профиле X3 SDK заданы 792×528 пикселей и 10 MHz SPI в
[BoardConfig.h:880–893](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/hardware/BoardConfig/include/BoardConfig.h#L880-L893).
Значит, один полный 1-bpp plane и активный X3 framebuffer — `792 / 8 × 528 =
52 272` байта. Два полных
payload (DTM2 и последующий DTM1) дают идеальный wire-time
`2 × 52 272 × 8 / 10 000 000 = 83,6 мс`; один plane — 41,8 мс. Это **нижняя
оценка времени передачи payload**, а не обещанный выигрыш: команды, настройка
окна, возможный per-row overhead, DRF и BUSY в неё не входят. Сам SDK посылает
полный plane одной CS-low транзакцией и разворачивает строки в
[EpdBus.cpp:396–403](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/bus/EpdBus.cpp#L396-L403).

Для обычного прогретого FAST-кадра UC8253 путь отправляет DTM2 в
[Uc8253X3Driver.cpp:191–195](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L191-L195),
а после завершения waveform отправляет DTM1 в
[Uc8253X3Driver.cpp:261–269](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L261-L269).
DTM1 нужен как baseline для следующего differential-перехода; простое удаление
этой передачи может сделать последующий FAST неправильным.

**Инференция.** Если контроллер допускает корректную windowed запись и его old
plane можно синхронизировать без полного DTM1, можно выиграть часть примерно
41,8 мс передачи. Это не означает выигрыш 41,8 мс на экране: baseline может
потребовать другой передачи, а waveform и host preparation останутся.

### 2. Waveform и partial waveform

В даташите PTL задаёт координаты частичного окна, `PT_SCAN=0` оставляет сканирование
затворов внутри окна, а `PT_SCAN=1` сканирует внутри и снаружи; в таблице это
значение указано как default. Следом PTIN входит в partial mode, а PTOUT выводит
из него ([UC8253c datasheet, p. 36, строки PTL/PTIN/PTOUT](https://files.waveshare.com/wiki/3.52inch%20e-Paper%20HAT/UC8253c.pdf)).
Это доказывает наличие window control и оставляет возможное влияние на gate scan,
но не доказывает безопасность произвольной строки на X3: важны порядок команд,
byte alignment, ориентация и таблица переходов.

SDK уже применяет PTIN/PTL/PTOUT для X3 grayscale preconditioning и записи
полосы, включая обратный GATE-Y и выравнивание X по байту, в
[Uc8253X3Driver.cpp:328–365](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L328-L365)
и [Uc8253X3Driver.cpp:383–414](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L383-L414).
В этих вызовах порядок — PTIN → PTL (последний байт окна `0x01`) → запись/DRF
→ PTOUT; `0x01` соответствует описанному в даташите `PT_SCAN=1`, то есть scan
внутри и снаружи окна. Это наблюдение относится к существующему grayscale-пути,
а не к готовому B/W partial API.
Это доказательство существования рабочего window-кода в grayscale-пути, но не
готового B/W API: в B/W class объявляет `display`, `displayStart` и
`displayFinish`, без override `displayWindow`, что видно в
[Uc8253X3Driver.h:44–79](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.h#L44-L79).

Partial window и partial waveform — разные изменения. PTL/PT_SCAN могут изменить
участок gate scan, но сами по себе не выбирают более короткую LUT. Good Display прямо
описывает режим, где обновляется **весь экран** с partial refresh, что является
хорошим первичным примером раздельности «объём/область данных» и «тип waveform»:
[официальное объявление Good Display](https://www.good-display.com/news/167.html).
Этот материал относится к дисплеям Good Display, не к X3, поэтому используется
только для терминологического различения.

### 3. DRF и BUSY

Для UC8253 драйвер после `CMD_DISPLAY_REFRESH` сначала проверяет начало LOW-фазы
до 50 мс, а `displayFinish()` затем ждёт завершения BUSY в
[Uc8253X3Driver.cpp:204–231](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L204-L231).
Для режима `X3TwoPhase` bus ждёт переход HIGH→LOW, затем LOW→HIGH с ограниченными
таймаутами; если доступна ISR-семафорная ветка, задача спит до точного edge в
[EpdBus.cpp:285–394](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/bus/EpdBus.cpp#L285-L394).

BUSY здесь — сигнал занятости внутренней операции контроллера и обязательный
барьер перед следующей записью RAM. Он не является фотодатчиком и не даёт
отдельного измерения времени оптического оседания пигмента: даташит связывает
DRF с LUT и внутренним BUSY_N, но не предоставляет для X3 независимый optical
settling timestamp ([UC8253c datasheet, pp. 19–20](https://files.waveshare.com/wiki/3.52inch%20e-Paper%20HAT/UC8253c.pdf)).
Поэтому BUSY нельзя «срезать» ожиданием или ранним DTM1. ISR вместо polling
сокращает занятость CPU и может изменить энергопотребление/планирование, но не
доказывает более короткую работу панели.

### 4. Явные дополнительные settle-проходы

В текущем UC8253 `displayFinish()` добавляет 200 мс только для не-FAST режимов,
может сделать partial-window conditioning, а затем выполняет полный DTM1 sync;
после `doFullSync` есть ещё один no-op FAST refresh и повторный DTM1 sync.
Точные ветки находятся в
[Uc8253X3Driver.cpp:232–279](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L232-L279).
Для обычного прогретого FAST отдельного `delay(200)` нет, однако DTM1 sync всё
равно входит в блокирующий `display()`.

В RSVP время измеряется вокруг `renderer.displayBuffer(mode)` в
[RsvpReaderActivity.cpp:766–806](../src/activities/reader/RsvpReaderActivity.cpp#L766-L806),
поэтому это время включает передачу, DRF/BUSY и обязательную работу драйвера
после BUSY. Session после `FramePresented` вычитает `refreshDurationMs` из паузы
в [RsvpSession.cpp:488–510](../lib/Rsvp/RsvpSession.cpp#L488-L510); подготовка
следующего кадра живёт в оставшемся времени.

## Какой контроллер установлен

У X3 есть две ветви, которые нельзя смешивать. Профиль
`XTEINK_X3` объявляет UC8253, 792×528 и 10 MHz, а соседний
`XTEINK_X3_UC8279` — UC8279d с тем же стеклом/геометрией и тем же активным
профилем 10 MHz: [BoardConfig.h:880–940](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/hardware/BoardConfig/include/BoardConfig.h#L880-L940).
В текущем CrossRSVP `platformio.ini` подключает локальный SDK через
`symlink://freeink-sdk` в [platformio.ini:107–116](../platformio.ini#L107-L116),
а submodule зафиксирован на `fde240f…`.

Facade маршрутизирует `PanelSel::X3` на `Uc8279Driver`, только когда
`ACTIVE.displayController == UC8279`; иначе выбирается `Uc8253X3Driver` в
[FreeInkDisplay.cpp:130–145](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/FreeInkDisplay.cpp#L130-L145).
Это текущая развилка драйверов, а не предположение по названию модели.

SDK описывает более новые X3 как UC8279d и читает три байта VER: `0x66` означает
подтверждённый UC8279, `0xFF` — предположение UC8253, другие значения оставляют
результат неубедительным в
[XteinkDetect.h:44–59](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/hardware/XteinkDetect/include/XteinkDetect.h#L44-L59).
При подтверждённом X3 выбирается профиль UC8279d, иначе сохраняется UC8253 в
[XteinkDetect.cpp:620–653](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/hardware/XteinkDetect/src/XteinkDetect.cpp#L620-L653).
Без Serial в данном исследовании нельзя считать конкретный экземпляр
идентифицированным; в A/B это надо показать на самом экране или сохранить после
паузы на SD.

Ветка UC8279d также не даёт готовой B/W-строки. Её `grayWindowIn()` входит в
PTIN с **полным окном 792×528**, затем `displayStart()` отправляет полный DTM2,
а `displayFinish()` отправляет полный DTM1 до PTOUT:
[Uc8279Driver.cpp:72–79](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8279Driver.cpp#L72-L79),
[Uc8279Driver.cpp:122–170](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8279Driver.cpp#L122-L170),
[Uc8279Driver.cpp:173–203](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8279Driver.cpp#L173-L203).
Комментарии этого драйвера ссылаются на UC8279d_B datasheet и reverse engineering
stock firmware; публичной официальной копии этого X3-specific datasheet в
проверенных материалах нет. Поэтому для UC8279d это подтверждённое поведение
pinned SDK, но не самостоятельная официальная гарантия произвольного PTL.

## Что может дать строка и что может сломаться

**Подтверждено источниками:** контроллерное семейство знает PTL/PTIN/PTOUT,
а SDK умеет windowed запись для grayscale; текущий B/W fallback передаёт полный
кадр. Это ссылки выше на UC8253 datasheet, `Uc8253X3Driver.cpp` и `PanelDriver.h`.

**Инженерный вывод:** строка может уменьшить payload примерно пропорционально
площади, если горизонтальные границы расширены до байтов, строки отправлены в
нужном порядке, а DTM1 и DTM2 для области остаются согласованными. Это не обещает
пропорционального сокращения BUSY: waveform может иметь фиксированные setup-фазы,
сканировать затворы за пределами данных или всё равно занимать большую часть
кадра. Для `PT_SCAN` и координат нужно следовать конкретной таблице контроллера,
а не переносить grayscale window-код в B/W без проверки ([UC8253c datasheet, p. 36](https://files.waveshare.com/wiki/3.52inch%20e-Paper%20HAT/UC8253c.pdf)).

Есть ещё два состояния, которые особенно важны для RSVP:

- Differential FAST сравнивает NEW с old frame в DTM1. Частичный NEW без
  корректного old baseline может дать не тот transition, даже если данные строки
  записались. Это следует из назначения DTM1/DTM2 в драйвере
  ([Uc8253X3Driver.h:8–15](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.h#L8-L15)).
- В single-buffer facade после async `displayStart()` framebuffer нельзя менять
  до `waitRefreshComplete()`. Это явно закреплено в
  [HalDisplay.h:52–63](../lib/hal/HalDisplay.h#L52-L63) и
  [FreeInkDisplay.cpp:630–665](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/FreeInkDisplay.cpp#L630-L665).
  Поэтому параллелить можно подготовку следующего токена/метаданных, но нельзя
  рисовать следующий RSVP-кадр в том же buffer до окончания waveform и post-sync.

## Реалистичные пределы

| Вариант | Что реально сокращается | Ожидаемый эффект | Цена и риск |
| --- | --- | --- | --- |
| Текущий полный FAST | Ничего нового; известный и проверенный путь | Базовая точка: около 475 мс FAST и около 95 кадров/мин на данном прогоне ([rsvp-display-limits.md:64–85](rsvp-display-limits.md#L64-L85)) | Сохраняет baseline и качество; это единственная текущая точка сравнения |
| B/W partial RAM window | Часть 52 272-байтовых plane-записей | **Инференция:** идеальный wire-time двух полных plane — 83,6 мс; фактический выигрыш может быть меньше, нулевым или потребовать другой baseline | Необходимо доказать PTL, old/new RAM, порядок строк, BUSY и качество на нужном контроллере |
| Короткая/частичная LUT | Время DRF/BUSY, если waveform действительно короче | **Инференция:** потенциально больше, чем экономия SPI | LUT и напряжения зависят от стекла, температуры и контроллера; [generic datasheet](https://files.waveshare.com/wiki/3.52inch%20e-Paper%20HAT/UC8253c.pdf) не даёт безопасной X3 LUT |
| Async + prefetch | Подготовку CPU между `displayStart` и BUSY | **Инференция:** может уменьшить host gap, но не длительность панели | Single-buffer contract запрещает запись в активный framebuffer; текущий RSVP измеряет блокирующий путь |
| Поднять SPI | Только передачу payload | **Инференция:** теоретически уменьшает wire-time; active X3 profile всё ещё 10 MHz | Нужна квалификация сигнала и платы; не является доказанным выигрышем refresh |

83,6 мс нельзя вычесть из 475 мс и объявить новый темп. Пользовательский прогон
показывает плато полного интервала около 632 мс, а не только `refreshDuration`; в
этом остатке могут быть отрисовка, планирование, логирование и cleanup
([rsvp-display-limits.md:79–85](rsvp-display-limits.md#L79-L85)). С практической
точки зрения partial RAM стоит проверять как оптимизацию десятков миллисекунд,
тогда как заметное изменение BUSY требует отдельной panel-qualified waveform.
Ни один из этих источников не подтверждает новое безопасное значение WPM.

Поднимать SPI или менять PLL/CDI/LUT «на глаз» не следует. BoardConfig намеренно
выбирает 10 MHz для обеих X3 ветвей ([BoardConfig.h:837–838](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/hardware/BoardConfig/include/BoardConfig.h#L837-L838));
generic UC8253 datasheet описывает 4-wire host interface и регистры, но не
квалифицирует конкретную Xteink плату на повышенной частоте ([UC8253c datasheet, pp. 10 и 31](https://files.waveshare.com/wiki/3.52inch%20e-Paper%20HAT/UC8253c.pdf)).
Нельзя также закреплять постоянный rails-on эксперимент: текущий FAST уже вызывает
PON только при `_isScreenOn == false`, чтобы не получить лишний BUSY ceiling, в
[Uc8253X3Driver.cpp:197–203](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp#L197-L203).

## Безопасный A/B-план без Serial

Это план для отдельной будущей диагностической задачи; здесь он не реализуется.
Он не требует просить у владельца UART-лог.

1. **Показать идентичность и среду на экране.** В начале прогона вывести один
   статичный заголовок: `UC8253 assumed`, `UC8279 confirmed` или `inconclusive`,
   raw VER, CPU MHz, revision SDK/образа, ориентацию и заданный WPM. API для
   получения probe snapshot уже есть в
   [XteinkDetect.h:75–99](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/hardware/XteinkDetect/include/XteinkDetect.h#L75-L99).
   Если UI ещё не отображает его, статус можно отрисовать до теста; во время
   каждого кадра заголовок не менять, чтобы не смешивать его отрисовку с тестом.

2. **Сохранить bounded-метрики.** Без heap и без записи на SD в hot path
   показывать после паузы/конца `n`, actual frames/min, полный `frame_ms`,
   `refresh_ms`, а для нового split — `start/upload`, `BUSY wait` и post-sync.
   Малые счётчики сами по себе не дают точные median/p95: для них нужен либо
   bounded histogram с явно обозначенными approximate `p50/p95`, либо ограниченный
   фиксированный массив samples с заранее указанным числом элементов и RAM-бюджетом.
   Текущий speedtest уже фиксирует `refresh_ms`, `frame_ms`, `interval_ms` и heap
   до Serial-вывода в
   [RsvpReaderActivity.cpp:811–821](../src/activities/reader/RsvpReaderActivity.cpp#L811-L821),
   но без Serial агрегаты надо вывести на экран или записать одним блоком после
   паузы. Если нужен CSV, писать его через HalStorage только после паузы или
   завершения.

3. **Первый A/B — host gap, с текущим полным FAST.** Вариант A — текущая
   блокирующая последовательность. Вариант H — только одна заранее выбранная
   оптимизация подготовки следующего слова/метаданных (prefetch или scheduling),
   без изменения драйвера, LUT, SPI, rails и BUSY wait. Выполнить A и H
   автономно по 60 секунд на одинаковом fixture из 720 коротких слов, с выключенной
   группировкой, одинаковыми ориентацией, температурой и шрифтом. Сравнить
   `refresh_ms`, полный `frame_ms`, `interval_ms` и actual frames/min. Если
   `refresh_ms` остаётся тем же, а actual pace меняется, это отделяет host gap от
   panel wait до эксперимента с partial window. Протокол fixture и длительность
   описаны в [rsvp-x3-speedtest.md:38–59](rsvp-x3-speedtest.md#L38-L59).

   При интерпретации учитывать два явных свойства текущего кода. Render task
   держит `HalPowerManager::Lock` во время `render()` в
   [ActivityManager.cpp:53–68](../src/activities/ActivityManager.cpp#L53-L68),
   поэтому сам render не должен попасть под понижение частоты CPU. Но RSVP не
   переопределяет `preventAutoSleep()`/`skipLoopDelay()` (базовые значения false в
   [Activity.h:43–50](../src/activities/Activity.h#L43-L50)); после 3000 мс
   бездействия main включает power saving и может спать короткими 50-мс срезами в
   [HalPowerManager.h:30–35](../lib/hal/HalPowerManager.h#L30-L35) и
   [main.cpp:764–785](../src/main.cpp#L764-L785). Это кандидат для измерения
   host gap, а не доказательство того, что весь 95-кадровый потолок вызван CPU.

4. **Второй A/B — partial, только после host baseline.** Вариант A снова текущий
   полный FAST, вариант B меняет только оконный механизм. На идентифицированном
   контроллере сначала проверить полное 792×528 PTL с теми же DTM2/DTM1 и LUT,
   чтобы отделить ошибку режима от ошибки малой геометрии; затем, только если этот
   шаг чистый, тестировать byte-aligned stripe/line. Нельзя менять одновременно
   окно, LUT, SPI и power-on. До следующего RAM write всегда дожидаться BUSY HIGH
   и завершать baseline sync; при текущем single-buffer контракте не трогать
   framebuffer до `waitRefreshComplete()`
   ([FreeInkDisplay.cpp:647–657](https://github.com/Free-Ink/freeink-sdk/blob/fde240faaeae6c340dacd435a4f77d2ef2f82dfd/libs/display/FreeInkDisplay/src/FreeInkDisplay.cpp#L647-L657)).
   Для line-кандидата область должна объединять old/new bbox слова и направляющие
   (либо иметь фиксированный максимальный размер), сначала очищать old перед
   рисованием new, затем переводиться logical→physical и byte-align во всех
   четырёх ориентациях; меню, смена шрифта, инверсия или cleanup требуют полного
   resync и обновления baseline.

5. **Критерий годности.** Сравнивать A/H и A/B по actual frames/min и
   approximate p95 либо по заранее ограниченному sample-set, а не по одному
   среднему. Проверять отсутствие пропущенных строк/краёв, ghosting, контраст и
   стабильность после 5/20/60 минут; после теста сделать обычный HALF или FULL
   cleanup. Повторить в четырёх ориентациях. Остановить B при BUSY timeout,
   видимых следах или рассинхроне старого/нового кадра. Если B экономит только
   upload, а actual pace не растёт, оставить его только как неиспользованный
   экспериментальный путь.

## Итоговое решение

- Для **UC8253** текущий B/W путь — полный DTM2, DRF/BUSY, полный DTM1; `displayWindow`
  сейчас фактически полный FAST. Partial RAM upload — разумный кандидат на
  отдельный A/B, но не готовая функция и не доказанное сокращение waveform.
- Для **подтверждённого UC8279d** есть отдельный SDK-путь; он тоже пишет полные
  792×528 planes в полном PTL. Его специфическое поведение нельзя смешивать с
  UC8253 и нельзя расширять до малой строки без квалификации.
- `BUSY` — обязательный барьер завершения внутренней refresh-операции. Уменьшать
  ожидание нельзя; partial window не даёт права пропускать BUSY. Optical settling
  и качество оцениваются по LUT/панели и визуальному A/B, а не по одной команде.
- Реалистичный первый A/B — проверить host preparation/prefetch при неизменном
  полном FAST и отделить host gap от panel wait; только затем измерять, даёт ли
  partial window экономию payload без изменения качества.
  Постоянный rails-on, произвольная LUT/PLL/CDI правка и повышение SPI не имеют
  доказанного безопасного выигрыша для данного X3.
- До такого A/B безопасная производственная база остаётся текущим полным FAST и
  наблюдаемым результатом пользователя: около 475 мс FAST и около 95 кадров/мин
  на указанном экземпляре, без переноса этого числа на другие X3.

## Первичные материалы

- [UltraChip UC8253c generic datasheet (PDF)](https://files.waveshare.com/wiki/3.52inch%20e-Paper%20HAT/UC8253c.pdf) — DTM1/DTM2, DSP/DRF, BUSY_N, PTL/PTIN/PTOUT, host interface; источник размещён на Waveshare, применимость к стеклу X3 не заявлена.
- [Страница Good Display для IC Driver UC8253](https://www.good-display.com/companyfile/794.html) — файл помечен категорией UltraChip; прямую manufacturer-hosted копию PDF UltraChip в доступных материалах найти не удалось, поэтому выше явно указано, что использован зеркальный generic-документ.
- [Free-Ink/freeink-sdk pinned revision](https://github.com/Free-Ink/freeink-sdk/tree/fde240faaeae6c340dacd435a4f77d2ef2f82dfd) — текущие X3 драйверы, BoardConfig, detection и EpdBus.
- [Good Display: Enhanced Full-Screen Partial Refresh](https://www.good-display.com/news/167.html) — первичный пример, что partial refresh может обновлять full-screen; это не X3 qualification.
- [Официальная страница Xteink X3](https://www.xteink.com/products/xteink-x3) — идентичность продукта; она не используется как доказательство его контроллера или времени refresh.
