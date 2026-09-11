(() => {
  const russian = {
    skip: 'К содержимому', navigation: 'Основная навигация', home: 'Главная CrossRSVP',
    languageLabel: 'Switch to English', navWhy: 'Зачем RSVP', navDemo: 'Демо', navDownloads: 'Загрузки',
    eyebrow: 'Форк CrossPoint Reader · EPUB', hero: 'Читайте в своём темпе,<br><em>по одному слову.</em>',
    lede: 'CrossRSVP добавляет в CrossPoint Reader режим RSVP. Библиотека, настройки и постраничный режим сохраняются. Вы выбираете способ чтения.',
    choose: 'Выбрать устройство ↓', source: 'Исходный код ↗', builtOn: 'На основе', models: 'Для Xteink X3 · X4 · X4 Pro',
    illustration: 'Иллюстрация ридера со словом и выделенной опорной буквой', rsvpMode: 'РЕЖИМ RSVP', illustrationPace: '100 слов/мин',
    illustrationText: 'Спокойная страница. Ровный ритм.<br>Сохраняйте фокус и позицию чтения.',
    whyEyebrow: 'Небольшое изменение ритма', why: 'Знакомый ридер,<br><em>ещё один способ чтения.</em>',
    featurePosition: 'Сохраняйте позицию', positionText: 'RSVP и постраничный режим используют общую позицию чтения. Поставьте на паузу, вернитесь к странице и продолжите с того же места.',
    featureFocus: 'Найдите фокус', focusText: 'Опорная буква ORP остаётся на месте, пока слова меняются вокруг неё. Вы можете оценить удобство фиксации без обещаний ускорения чтения.',
    featurePace: 'Выбирайте темп', paceText: 'Запустите пример, измените темп и продолжите с текущего слова. RSVP предназначен для EPUB; неподдерживаемое содержимое доступно в постраничном режиме.',
    demoEyebrow: 'Попробуйте принцип', demo: 'Одно слово.<br><em>Одна опора.</em>',
    demoText: 'Этот пример в браузере показывает принцип RSVP. Это не эмулятор E-Ink и не измерение скорости прошивки.',
    companions: 'В примере слова показываются отдельно. Дополнительная группировка в прошивке поддерживает русский и английский согласно выбранному языку книги.',
    quiet: 'Focus Reading — отдельная функция постраничного режима. RSVP меняет способ показа слов EPUB.',
    pace: 'Темп', wpm: 'слов/мин', pick: 'Выберите точную модель', downloads: 'Стабильные сборки,<br><em>когда они готовы.</em>',
    downloadText: 'Предварительные релизы не показываются как стабильные. Сверьте имя файла с обозначением на ридере.',
    stable: 'Стабильный релиз пока не опубликован.', view: 'Смотреть релизы ↗', download: 'Скачать .bin ↓', notes: 'Что нового ↗',
    next: 'Далее', install: 'Установка и откат',
    installText: 'Сохраните копию книг и настроек, проверьте SHA-256 и используйте меню обновления прошивки с SD-карты. Оставьте предыдущий образ для отката.',
    installGuide: 'Инструкция по установке и откату ↗', footer: 'Открытая прошивка для ридеров Xteink.',
    forkSource: 'Исходники форка ↗', attribution: 'Визуальный стиль и авторство ↗',
    independent: 'CrossRSVP — независимый форк. CrossPoint Reader и связанные названия принадлежат соответствующим авторам.'
  };
  const states = {
    en: { start: 'Start', resume: 'Continue', pause: 'Pause', paused: 'Paused', playing: 'Playing', ended: 'Finished', restart: 'Restart' },
    ru: { start: 'Запустить', resume: 'Продолжить', pause: 'Пауза', paused: 'Пауза', playing: 'Воспроизведение', ended: 'Завершено', restart: 'Заново' }
  };
  const passages = { en: 'A calm page finds its rhythm one word at a time.', ru: 'Спокойная страница находит ритм слово за словом.' };
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
