(() => {
  const russian = {
    "skip": "К содержимому",
    "navigation": "Основная навигация",
    "home": "Главная CrossRSVP",
    "languageLabel": "Switch to English",
    "navWhy": "Возможности",
    "navDemo": "Пример",
    "navDownloads": "Скачать",
    "navInstall": "Установка",
    "eyebrow": "Прошивка для чтения на Xteink",
    "hero": "Читайте книги<br><em>в своём темпе.</em>",
    "lede": "CrossRSVP — прошивка для Xteink X3, X4 и X4 Pro на основе CrossPoint Reader. Читайте книги постранично или включайте RSVP: слова из книги будут появляться по очереди в одном месте экрана.",
    "choose": "Скачать прошивку ↓",
    "tryDemo": "Попробовать на сайте ↓",
    "builtOn": "На основе",
    "models": "Для Xteink X3 · X4 · X4 Pro",
    "illustration": "Ридер показывает одно слово с выделенной буквой",
    "rsvpMode": "РЕЖИМ RSVP",
    "illustrationPace": "100 слов/мин",
    "illustrationWord": "<span>чи</span><strong>т</strong><span>ать</span>",
    "illustrationText": "Пауза в любой момент.<br>Продолжайте с того же места.",
    "whyEyebrow": "Возможности для чтения",
    "why": "Что умеет<br><em>CrossRSVP</em>",
    "featurePages": "Обычное чтение по страницам",
    "pagesText": "Открывайте книги с SD-карты, листайте страницы, выбирайте шрифт и размер текста. Библиотека и привычные инструменты CrossPoint остаются доступны.",
    "featureFocus": "Слова из книги в одной строке",
    "focusText": "В режиме RSVP слова из книги появляются по одному вокруг неподвижной опорной буквы. Во время чтения ридер обновляет область строки со словом. На протестированном X3 это ускорило смену слов.",
    "featurePace": "Настройка скорости и пауз",
    "paceText": "Выберите количество слов в минуту. Ставьте чтение на паузу, двигайтесь по одному слову или возвращайтесь назад. Настройте длительность пауз на длинных словах и знаках препинания.",
    "featurePosition": "Возврат к странице",
    "positionText": "Переключайтесь из RSVP обратно к книге, чтобы перечитать абзац, рассмотреть иллюстрацию или таблицу. При выходе из книги место, на котором вы остановились, сохраняется.",
    "featureGroups": "Короткие слова вместе",
    "groupsText": "Включите группировку, чтобы рядом с основным словом появлялись до двух коротких служебных слов. Они показываются уменьшенным шрифтом. С выключенной группировкой слова идут по одному.",
    "demoEyebrow": "Пример чтения в RSVP",
    "demo": "Попробуйте читать<br><em>слово за словом</em>",
    "demoText": "Нажмите «Начать» и следите за выделенной буквой, пока слова сменяют друг друга. Ползунком подберите удобную скорость. В любой момент можно сделать паузу.",
    "demoNote": "Это пример в браузере. На ридере темп также зависит от скорости обновления экрана E-Ink.",
    "pace": "Скорость",
    "wpm": "слов/мин",
    "pick": "X3 · X4 · X4 Pro",
    "downloads": "Скачать для<br><em>вашего ридера</em>",
    "downloadText": "Для каждой модели устройства нужен свой файл прошивки. Предварительные релизы на GitHub отмечены как beta.",
    "unavailable": "Для этой модели пока нет файла прошивки.",
    "view": "Открыть релизы ↗",
    "download": "Скачать .bin ↓",
    "notes": "Что нового ↗",
    "allReleases": "Все версии на GitHub ↗",
    "installEyebrow": "С чего начать",
    "install": "Как установить<br><em>CrossRSVP</em>",
    "installText": "Если на ридере уже установлен CrossPoint Reader или CrossRSVP, обновите прошивку с SD-карты.",
    "stepDownload": "Скачайте прошивку",
    "stepDownloadText": "Выберите свой ридер выше и скачайте файл .bin. Сохраните копию книг и настроек с SD-карты, а также файл предыдущей прошивки.",
    "stepCopy": "Скопируйте файл на SD-карту",
    "stepCopyText": "Поместите файл .bin на карту. Если копируете через USB на X4 Pro, сначала безопасно извлеките накопитель на компьютере, а затем выйдите из режима передачи USB на ридере.",
    "stepUpdate": "Запустите обновление",
    "stepUpdateText": "Откройте «Настройки → Система → Обновление системы с SD-карты». Выберите файл, подтвердите обновление и дождитесь перезапуска. Не выключайте ридер во время установки.",
    "stepRead": "Откройте книгу и включите RSVP",
    "stepReadText": "Откройте EPUB и вызовите меню чтения. В панели выберите «Прочее → Режим чтения RSVP», в меню-списке — «Режим чтения RSVP». Режим откроется на паузе.",
    "firstInstall": "На устройстве заводская прошивка? Сначала установите CrossPoint Reader по <a href=\"https://github.com/crosspoint-reader/crosspoint-reader#install-firmware\">инструкции проекта ↗</a>, затем выполните шаги выше.",
    "installGuide": "Подробная инструкция и возврат к прежней версии ↗",
    "footer": "Бесплатная прошивка с открытым исходным кодом для ридеров Xteink.",
    "forkSource": "Исходный код ↗",
    "attribution": "Дизайн вдохновлён CrossPoint Tools ↗",
    "independent": "CrossRSVP — самостоятельный проект на основе CrossPoint Reader."
};
  const states = {
    en: { start: 'Start', resume: 'Continue', pause: 'Pause', paused: 'Paused', playing: 'Playing', ended: 'Finished', restart: 'Restart' },
    ru: { start: 'Начать', resume: 'Продолжить', pause: 'Пауза', paused: 'Пауза', playing: 'Чтение', ended: 'Завершено', restart: 'Заново' }
  };
  const passages = { en: 'You can read one word at a time. Choose a comfortable pace and pause whenever you need.', ru: 'Вы можете читать слово за словом. Выберите удобную скорость и делайте паузы, когда нужно.' };
  const translated = [...document.querySelectorAll('[data-i18n]')].map(node => ({ node, key: node.dataset.i18n, english: node.innerHTML }));
  const named = [...document.querySelectorAll('[data-i18n-aria]')].map(node => ({ node, key: node.dataset.i18nAria, english: node.getAttribute('aria-label') }));
  const languageButton = document.getElementById('language');
  const toggle = document.getElementById('demo-toggle');
  const reset = document.getElementById('demo-reset');
  const word = document.getElementById('demo-word');
  const status = document.getElementById('demo-state');
  const speed = document.getElementById('speed');
  const output = document.getElementById('speed-value');
  let language = (navigator.language || '').toLowerCase().startsWith('ru') ? 'ru' : 'en';
  try { const saved = localStorage.getItem('crossrsvp-language'); if (saved === 'ru' || saved === 'en') language = saved; } catch {}
  let words = [], index = 0, timer = null, playing = false, started = false, ended = false;
  const before = document.createElement('span'); before.className = 'word-before';
  const pivot = document.createElement('strong'); pivot.className = 'word-pivot';
  const after = document.createElement('span'); after.className = 'word-after';
  word.replaceChildren(before, pivot, after);
  function renderWord() {
    const parts = [...new Intl.Segmenter(language, { granularity: 'grapheme' }).segment(words[index])].map(part => part.segment);
    const letters = parts.map((part, i) => /\p{L}/u.test(part) ? i : -1).filter(i => i >= 0);
    const position = letters[Math.min(4, Math.floor(letters.length * 0.35))] ?? 0;
    before.textContent = parts.slice(0, position).join('');
    pivot.textContent = parts[position];
    after.textContent = parts.slice(position + 1).join('');
  }
  function renderState() {
    const copy = states[language];
    toggle.textContent = playing ? copy.pause : ended ? copy.restart : started ? copy.resume : copy.start;
    status.textContent = playing ? copy.playing : ended ? copy.ended : copy.paused;
    reset.textContent = copy.restart;
  }
  function pause() { playing = false; clearTimeout(timer); renderState(); }
  function schedule() {
    timer = setTimeout(() => {
      if (!playing) return;
      if (index + 1 === words.length) { ended = true; pause(); return; }
      ++index; renderWord(); schedule();
    }, 60000 / Number(speed.value));
  }
  function restart() { pause(); index = 0; started = false; ended = false; renderWord(); renderState(); }
  function applyLanguage() {
    pause(); words = passages[language].split(/\s+/u); index = 0; started = false; ended = false;
    document.documentElement.lang = language;
    document.title = language === 'ru' ? 'CrossRSVP — прошивка для чтения на Xteink' : 'CrossRSVP · Reading firmware for Xteink';
    document.querySelector('meta[name="description"]').content = language === 'ru'
      ? 'CrossRSVP для Xteink X3, X4 и X4 Pro: чтение по страницам и слово за словом. Скачать прошивку и узнать, как её установить.'
      : 'CrossRSVP firmware for Xteink X3, X4 and X4 Pro: read pages or follow words one at a time. Download and installation guide.';
    for (const entry of translated) entry.node.innerHTML = language === 'ru' ? russian[entry.key] : entry.english;
    for (const entry of named) entry.node.setAttribute('aria-label', language === 'ru' ? russian[entry.key] : entry.english);
    languageButton.textContent = language === 'ru' ? 'EN ↗' : 'RU ↗';
    languageButton.setAttribute('aria-label', language === 'ru' ? 'Switch to English' : 'Переключить на русский');
    if (language === 'en') languageButton.setAttribute('aria-label', 'Переключить на русский');
    renderWord(); renderState();
  }
  languageButton.addEventListener('click', () => {
    language = language === 'en' ? 'ru' : 'en';
    try { localStorage.setItem('crossrsvp-language', language); } catch {}
    applyLanguage();
  });
  toggle.addEventListener('click', () => {
    if (playing) { pause(); return; }
    if (ended) restart();
    playing = true; started = true; renderState(); schedule();
  });
  reset.addEventListener('click', restart);
  speed.addEventListener('input', () => { output.value = speed.value; if (playing) { clearTimeout(timer); schedule(); } });
  document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });
  applyLanguage(); document.documentElement.classList.add('js');
})();
