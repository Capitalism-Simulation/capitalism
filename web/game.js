// Capitalism web edition.
// Game content comes from data/game.json (shared with the console version).
// URL options: ?fast (no typewriter effect), ?seed=123 (reproducible events).
'use strict';

(() => {
  const params = new URLSearchParams(location.search);
  const FAST = params.has('fast');
  const SLOT_KEYS = ['morning', 'afternoon', 'night'];
  const SLOT_LABEL = { morning: '아침', afternoon: '오후', night: '밤' };
  const STORE_KEY = 'capitalism.v1';
  const DATA_URLS = ['data/game.json', '../data/game.json'];

  const $ = (id) => document.getElementById(id);
  const el = (tag, cls, text) => {
    const node = document.createElement(tag);
    if (cls) node.className = cls;
    if (text != null) node.textContent = text;
    return node;
  };

  let data = null;
  const state = {
    name: '',
    job: 0,
    difficulty: 0,
    gender: 0,
    loverGender: 0,
    balance: 0,
    shownBalance: 0,
    affection: [],
    inventory: {},
    used: new Set(),
    turn: 0,
    rng: Math.random,
    result: null,
  };

  // ---------- Utilities ----------

  function mulberry32(seed) {
    let a = seed >>> 0;
    return () => {
      a = (a + 0x6d2b79f5) >>> 0;
      let t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  const fmt = (tmpl, vars) =>
    Object.entries(vars).reduce((s, [k, v]) => s.split(`{${k}}`).join(String(v)), tmpl);

  const won = (n) => `${n.toLocaleString('ko-KR')}원`;
  const signedWon = (n) => (n > 0 ? '+' : '') + won(n);
  const lastLine = (text) => text.trim().split('\n').filter(Boolean).pop() || '';
  const loverVar = (i) => `var(--lover-${i})`;
  const job = () => data.jobs[state.job];

  const storage = {
    load() {
      try {
        return JSON.parse(localStorage.getItem(STORE_KEY)) || {};
      } catch {
        return {};
      }
    },
    save(value) {
      try {
        localStorage.setItem(STORE_KEY, JSON.stringify(value));
      } catch {
        /* private mode or storage blocked: progress just isn't remembered */
      }
    },
  };

  let toastTimer = 0;
  function toast(message) {
    const t = $('toast');
    t.textContent = message;
    t.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => (t.hidden = true), 2200);
  }

  function show(screen) {
    document.querySelectorAll('.screen').forEach((s) => (s.hidden = s.id !== `screen-${screen}`));
    if (screen === 'title') renderTitle();
    if (screen === 'collection') renderCollection();
    window.scrollTo(0, 0);
  }

  // ---------- Endings & collection ----------

  const endingKeys = ['true', 'noMoney', 'noLove'];
  // Jobs may override the shared confession lines, ending texts and true-ending balance.
  const endingText = (j, key) => (j.endings && j.endings[key]) || data.endings[key];
  const confessText = (j, i) => (j.confess && j.confess[i]) || data.lovers[i].confess;
  const minBalance = (j) => (j.trueEnding && j.trueEnding.minBalance != null ? j.trueEnding.minBalance : data.settings.trueEnding.minBalance);
  const endingLabel = (key, j) =>
    key === 'broke' ? '파산 엔딩' : lastLine(endingText(j, key)).replace('{color}색 ', '').replace('{color}', '');
  const collectionId = (jobId, key, lover) => (key === 'broke' ? `${jobId}:broke` : `${jobId}:${key}:${lover}`);
  const collectionTotal = () => data.jobs.length * (data.lovers.length * endingKeys.length + 1);

  function unlocked() {
    return storage.load().endings || {};
  }

  function unlock(id) {
    const saved = storage.load();
    saved.endings = saved.endings || {};
    const isNew = !saved.endings[id];
    if (isNew) saved.endings[id] = Date.now();
    storage.save(saved);
    return isNew;
  }

  function renderTitle() {
    const count = Object.keys(unlocked()).length;
    $('title-collection-count').textContent = `${count} / ${collectionTotal()}`;
  }

  function renderCollection() {
    const got = unlocked();
    const list = $('collection-list');
    list.replaceChildren();
    let total = 0;
    for (const j of data.jobs) {
      const block = el('section', 'job-block');
      const h = el('h3');
      const grid = el('div', 'ending-grid');
      let count = 0;
      grid.append(el('span'));
      for (const key of endingKeys) grid.append(el('span', 'head', endingLabel(key, j)));
      data.lovers.forEach((lover, i) => {
        const name = el('span', 'name', `${lover.color} 머리`);
        name.style.setProperty('--c', loverVar(i));
        grid.append(name);
        for (const key of endingKeys) {
          const on = !!got[collectionId(j.id, key, i)];
          if (on) count++;
          const cell = el('span', `cell${on ? ' on' : ''}`, on ? '달성' : '???');
          cell.style.setProperty('--c', loverVar(i));
          grid.append(cell);
        }
      });
      const brokeOn = !!got[collectionId(j.id, 'broke')];
      if (brokeOn) count++;
      grid.append(el('span', 'name', '파산'));
      const brokeCell = el('span', `cell wide${brokeOn ? ' on' : ''}`, brokeOn ? '파산 엔딩 달성' : '???');
      brokeCell.style.setProperty('--c', 'var(--muted)');
      grid.append(brokeCell);

      const perJob = data.lovers.length * endingKeys.length + 1;
      h.append(el('span', null, j.name), el('span', 'muted', `${count} / ${perJob}`));
      block.append(h, grid);
      list.append(block);
      total += count;
    }
    $('collection-count').textContent = `${total} / ${collectionTotal()}`;
  }

  // ---------- Setup ----------

  function renderSetup() {
    const saved = storage.load();
    $('input-name').value = saved.name || '';

    const jobs = $('job-options');
    jobs.replaceChildren();
    data.jobs.forEach((j, i) => {
      const label = el('label', 'choice');
      const input = el('input');
      Object.assign(input, { type: 'radio', name: 'job', value: String(i), checked: i === (saved.job ?? 0) });
      label.append(input, el('strong', null, j.name), el('span', null, `시작 자금 ${won(j.startBalance)}`));
      jobs.append(label);
    });

    fillSegmented('difficulty-options', 'difficulty', data.difficulties.map((d) => d.name), saved.difficulty ?? 1);
    const describe = () => {
      const i = Number(document.querySelector('input[name="difficulty"]:checked').value);
      $('difficulty-desc').textContent = data.difficulties[i].description || '';
    };
    $('difficulty-options').onchange = describe;
    describe();
    fillSegmented('gender-options', 'gender', data.genders.player.map((g) => g.name), saved.gender ?? 0);
    fillSegmented('lover-gender-options', 'loverGender', data.genders.lover.map((g) => g.name), saved.loverGender ?? 0);
    if (!jobs.querySelector('input:checked')) jobs.querySelector('input').checked = true;
  }

  function fillSegmented(containerId, name, labels, selected) {
    const box = $(containerId);
    box.replaceChildren();
    labels.forEach((text, i) => {
      const label = el('label');
      const input = el('input');
      Object.assign(input, { type: 'radio', name, value: String(i), checked: i === selected });
      label.append(input, document.createTextNode(text));
      box.append(label);
    });
    if (!box.querySelector('input:checked')) box.querySelector('input').checked = true;
  }

  // Resolves {P:a|b}, {L:a|b|c} and named genderWords; other {tokens} stay as they are.
  function localize(text) {
    const p = data.genders.player[state.gender].id;
    const l = data.genders.lover[state.loverGender].id;
    return text.replace(/\{([^{}]*)\}/g, (whole, inner) => {
      if (/^[PL]:/.test(inner)) {
        const options = inner.slice(2).split('|');
        const value = options[inner[0] === 'P' ? state.gender : state.loverGender];
        return value ?? whole;
      }
      const word = data.genderWords && data.genderWords[inner];
      if (!word) return whole;
      const key = word.by === 'player' ? p : word.by === 'lover' ? l : `${l}:${p}`;
      return word.values[key] ?? whole;
    });
  }

  // ---------- Panel, typing and controls ----------

  const panel = () => $('panel');
  let typing = false;
  let skipTyping = null;
  let keyHandler = null;

  function clearPanel() {
    panel().replaceChildren();
    panel().scrollTop = 0;
  }

  function scrollPanel() {
    const p = panel();
    p.scrollTop = p.scrollHeight;
  }

  // Splits text into narration and dialogue. A dialogue line looks like `@red “...”`:
  // red/blue/green/black = the four characters, me = the player, anything else is a name.
  function segmentsOf(text) {
    const segs = [];
    let narration = [];
    const flush = () => {
      const t = narration.join('\n').replace(/^\n+|\n+$/g, '');
      if (t) segs.push({ speaker: null, text: t });
      narration = [];
    };
    for (const line of text.split('\n')) {
      const m = /^@(\S+)\s+(.*)$/.exec(line);
      if (m) {
        flush();
        segs.push({ speaker: m[1], text: m[2].trim() });
      } else {
        narration.push(line);
      }
    }
    flush();
    return segs;
  }

  function speakerInfo(id) {
    const s = (data.speakers || {})[id];
    if (s && s.lover != null) return { name: `${data.lovers[s.lover].color} 머리`, color: loverVar(s.lover), me: false };
    if (s && s.name) return { name: s.name.replace('{player}', state.name || '나'), color: 'var(--accent)', me: id === 'me' };
    return { name: id, color: 'var(--muted)', me: false };
  }

  function typeBlock(text, cls) {
    const block = el('div', `block ${cls || ''}`);
    panel().append(block);
    // Build every line first (speaker chips appear immediately), then type the words in order.
    const targets = segmentsOf(localize(text)).map((seg) => {
      const node = document.createTextNode('');
      if (!seg.speaker) {
        const p = el('p', 'narr');
        p.append(node);
        block.append(p);
      } else {
        const info = speakerInfo(seg.speaker);
        const row = el('div', `say${info.me ? ' me' : ''}`);
        row.style.setProperty('--c', info.color);
        const line = el('span', 'line');
        line.append(node);
        row.append(el('span', 'speaker', info.name), line);
        block.append(row);
        row.hidden = true;  // revealed when its turn to type comes
      }
      return { node, chars: Array.from(seg.text), row: node.parentNode.closest('.say') };
    });
    const delay = FAST ? 0 : data.settings.typingDelayMs;
    const revealAll = () => targets.forEach((t) => {
      if (t.row) t.row.hidden = false;
      t.node.data = t.chars.join('');
    });
    return new Promise((resolve) => {
      if (!delay) {
        revealAll();
        scrollPanel();
        resolve();
        return;
      }
      let seg = 0;
      let i = 0;
      let timer = 0;
      typing = true;
      panel().classList.add('typing');
      const finish = () => {
        typing = false;
        skipTyping = null;
        panel().classList.remove('typing');
        scrollPanel();
        resolve();
      };
      // Called on click / Enter / Space: show the rest of the block immediately.
      skipTyping = () => {
        clearTimeout(timer);
        revealAll();
        finish();
      };
      const step = () => {
        while (seg < targets.length && i >= targets[seg].chars.length) {
          seg++;
          i = 0;
        }
        if (seg >= targets.length) {
          finish();
          return;
        }
        const t = targets[seg];
        if (t.row) t.row.hidden = false;
        t.node.data += t.chars[i++];
        if (i % 8 === 0 || i === 1 || t.chars[i - 1] === '\n') scrollPanel();
        timer = setTimeout(step, delay);
      };
      step();
    });
  }

  // buttons: [{ label, value, keys, cls, color, hint }]
  function ask(buttons, layout) {
    const controls = $('controls');
    controls.className = `controls${layout ? ` ${layout}` : ''}`;
    return new Promise((resolve) => {
      const finish = (value) => {
        keyHandler = null;
        controls.replaceChildren();
        resolve(value);
      };
      controls.replaceChildren(
        ...buttons.map((b) => {
          const btn = el('button', `btn ${b.cls || ''}`, b.label);
          btn.type = 'button';
          if (b.color) btn.style.setProperty('--c', b.color);
          if (b.hint) btn.append(el('kbd', null, b.hint));
          btn.addEventListener('click', () => finish(b.value));
          return btn;
        }),
      );
      keyHandler = (e) => {
        const key = e.key === ' ' ? 'space' : e.key.toLowerCase();
        const match = buttons.find((b) => b.keys && b.keys.includes(key));
        if (match) {
          e.preventDefault();
          finish(match.value);
        }
      };
      scrollPanel();
    });
  }

  const next = (label = '다음') => ask([{ label, value: true, keys: ['enter', 'space'], cls: 'primary', hint: '↵' }]);
  const askYesNo = () =>
    ask([
      { label: '예', value: true, keys: ['y'], cls: 'primary', hint: 'Y' },
      { label: '아니오', value: false, keys: ['n'], hint: 'N' },
    ]);

  document.addEventListener('keydown', (e) => {
    if (e.repeat || e.isComposing) return;
    if (!$('bag-sheet').hidden) {
      if (e.key === 'Escape') openBag(false);
      return;  // the bag is open: don't advance the game underneath it
    }
    if (e.target instanceof Element && e.target.closest('input, textarea, select')) return;
    if (typing && (e.key === 'Enter' || e.key === ' ')) {
      e.preventDefault();
      if (skipTyping) skipTyping();
      return;
    }
    if (keyHandler) keyHandler(e);
  });

  // ---------- Status bar ----------

  let balanceAnim = 0;
  function renderStatus() {
    const d = data.difficulties[state.difficulty];
    $('status-who').textContent = `${state.name} · ${job().name} · ${d.name}`;

    const endingTurn = data.settings.endingTurn;
    const totalDays = Math.ceil((endingTurn - 1) / 3) || 1;
    let time = '프롤로그';
    if (state.turn >= endingTurn) time = '마지막 밤 · 고백';
    else if (state.turn >= 1) {
      const day = Math.floor((state.turn - 1) / 3) + 1;
      time = `Day ${day} · ${SLOT_LABEL[SLOT_KEYS[(state.turn - 1) % 3]]}`;
    }
    $('status-time').textContent = time;

    const days = $('status-days');
    days.style.setProperty('--days', totalDays);
    if (days.children.length !== totalDays) {
      days.replaceChildren(...Array.from({ length: totalDays }, () => {
        const s = el('span');
        s.append(el('i'));
        return s;
      }));
    }
    const done = state.turn >= endingTurn ? totalDays * 3 : state.turn;
    Array.from(days.children).forEach((s, i) => {
      const filled = Math.max(0, Math.min(3, done - i * 3));
      s.firstChild.style.setProperty('--fill', `${(filled / 3) * 100}%`);
    });
    setBalanceText(state.shownBalance);
    renderHearts();
    renderBag();
  }

  const showAffection = () => difficulty().showAffection !== false;

  function renderHearts(pulseIndex) {
    const box = $('status-hearts');
    if (!showAffection()) {
      box.replaceChildren(el('span', 'hidden-note', '호감도 비공개 (Hard)'));
      return;
    }
    box.replaceChildren(
      ...data.lovers.map((l, i) => {
        const value = state.affection[i] ?? 0;
        const h = el('span', `heart${pulseIndex === i ? ' pulse' : ''}`, String(value));
        h.style.setProperty('--c', loverVar(i));
        h.title = `${l.color} 머리 호감도 ${value}`;
        h.setAttribute('aria-label', h.title);
        return h;
      }),
    );
  }

  const itemCount = () => Object.values(state.inventory).reduce((a, b) => a + b, 0);
  const hasItem = (id) => (state.inventory[id] || 0) > 0;

  function renderBag() {
    $('bag-count').textContent = String(itemCount());
    const list = $('bag-list');
    const entries = Object.entries(state.inventory).filter(([, n]) => n > 0);
    if (!entries.length) {
      list.replaceChildren(el('li', 'empty', '아직 아무것도 없어요.'));
      return;
    }
    list.replaceChildren(
      ...entries.map(([id, n]) => {
        const item = data.items[id];
        const li = el('li');
        li.append(el('strong', null, n > 1 ? `${item.name} ×${n}` : item.name), el('p', null, item.description || ''));
        const fan = data.lovers.findIndex((l) => l.favoriteItem === id);
        if (fan >= 0) {
          const likes = el('p', 'likes', `${data.lovers[fan].color} 머리가 좋아할 것 같다`);
          likes.style.setProperty('--c', loverVar(fan));
          li.append(likes);
        }
        return li;
      }),
    );
  }

  function openBag(open) {
    $('bag-sheet').hidden = !open;
    if (open) $('btn-bag-close').focus();
  }

  function setBalanceText(value) {
    const b = $('status-balance');
    b.textContent = won(Math.round(value));
    b.classList.toggle('negative', value < 0);
  }

  function animateBalance(from, to) {
    cancelAnimationFrame(balanceAnim);
    const start = performance.now();
    const duration = FAST ? 0 : 600;
    const tick = (now) => {
      const t = duration ? Math.min(1, (now - start) / duration) : 1;
      const eased = 1 - Math.pow(1 - t, 3);
      state.shownBalance = from + (to - from) * eased;
      setBalanceText(state.shownBalance);
      if (t < 1) balanceAnim = requestAnimationFrame(tick);
    };
    balanceAnim = requestAnimationFrame(tick);

    const delta = $('status-delta');
    delta.textContent = signedWon(to - from);
    delta.className = `delta show ${to > from ? 'gain' : 'loss'}`;
    setTimeout(() => delta.classList.remove('show'), 1600);
  }

  // ---------- Game flow ----------

  const outcomesOf = (e) => (e.choices ? [e.choices.y, e.choices.n] : [e.outcome]);
  // Relationship events (affection or items involved) are kept on every difficulty.
  const hasAffection = (e) =>
    !!e.needs ||
    outcomesOf(e).some(
      (o) => (o.affection && o.affection.length) || (o.items && ((o.items.gain || []).length || (o.items.lose || []).length)),
    );
  const averageMoney = (e) => {
    const outs = outcomesOf(e);
    return outs.reduce((sum, o) => sum + (o.money || 0), 0) / outs.length;
  };

  function pickEvent(slot, phase) {
    let pool = job().events.filter((e) => e.slot === slot && e.phase === phase);
    if (!pool.length) return null;
    // Same order as the console version: affection events first (always kept), then
    // pure-money events from harshest to most generous. Array#sort is stable.
    pool.sort((a, b) => {
      const affA = hasAffection(a), affB = hasAffection(b);
      if (affA !== affB) return affA ? -1 : 1;
      return affA ? 0 : averageMoney(a) - averageMoney(b);
    });
    // Harder difficulties only use the first part of each pool (same rule as the console version).
    const ratio = data.difficulties[state.difficulty].eventPoolRatio;
    pool = pool.slice(0, Math.max(1, Math.ceil(pool.length * ratio - 1e-9)));
    let fresh = pool.filter((e) => !state.used.has(e.id));
    if (!fresh.length) {
      pool.forEach((e) => state.used.delete(e.id));
      fresh = pool;
    }
    const chosen = fresh[Math.floor(state.rng() * fresh.length)];
    state.used.add(chosen.id);
    return chosen;
  }

  function effectChip(cls, text, color) {
    const chip = el('span', `effect ${cls}`, text);
    if (color) chip.style.setProperty('--c', color);
    panel().append(chip);
  }

  function changeAffection(index, amount) {
    state.affection[index] += amount;
    if (!showAffection()) return;
    effectChip('aff', `♥ ${data.lovers[index].color} 머리 ${amount > 0 ? '+' : ''}${amount}`, loverVar(index));
    renderHearts(index);
  }

  function applyAffection(list) {
    for (const change of list || []) {
      data.lovers.forEach((_, i) => {
        if (change.target === 'all' || change.target === i) changeAffection(i, change.amount);
      });
    }
  }

  function applyItems(items) {
    if (!items) return;
    for (const id of items.lose || []) {
      if (!hasItem(id)) continue;
      state.inventory[id] -= 1;
      effectChip('item', `가방: ${data.items[id].name} 사용`);
    }
    for (const id of items.gain || []) {
      state.inventory[id] = (state.inventory[id] || 0) + 1;
      effectChip('item', `가방: ${data.items[id].name} 획득`);
      $('btn-bag').classList.remove('bump');
      void $('btn-bag').offsetWidth;
      $('btn-bag').classList.add('bump');
    }
    renderBag();
  }

  const difficulty = () => data.difficulties[state.difficulty];

  function scaleMoney(amount) {
    const d = difficulty();
    const mult = amount < 0 ? d.lossMultiplier ?? 1 : d.gainMultiplier ?? 1;
    return Math.round((amount * mult) / 100) * 100 || 0;
  }

  const minAffection = (j) =>
    ((j.trueEnding && j.trueEnding.minAffection != null) ? j.trueEnding.minAffection : data.settings.trueEnding.minAffection) +
    (difficulty().minAffectionDelta || 0);

  function showMoney(rawAmount) {
    const amount = scaleMoney(rawAmount);
    const chip = el('div', `money ${amount > 0 ? 'gain' : amount < 0 ? 'loss' : ''}`,
      amount === 0 ? `잔고 ${won(state.balance)} (변동 없음)` : signedWon(amount));
    panel().append(chip);
    const before = state.balance;
    state.balance += amount;
    if (amount !== 0) animateBalance(before, state.balance);
    scrollPanel();
  }

  async function playStories(turn) {
    const pages = job().stories[String(turn)];
    if (!pages) return;
    for (const page of pages) {
      clearPanel();
      await typeBlock(page, 'story');
      await next();
    }
  }

  async function playEvent() {
    const slot = SLOT_KEYS[(state.turn - 1) % 3];
    const phase = state.turn >= data.settings.lateGameStartTurn ? 'late' : 'early';
    const ev = pickEvent(slot, phase);
    if (!ev) return;

    clearPanel();
    await typeBlock(ev.text, 'event');
    let outcome = ev.outcome;
    if (ev.choices) {
      if (ev.needs) {
        const have = hasItem(ev.needs.item);
        panel().append(
          el('div', `need-tag ${have ? 'have' : 'missing'}`,
            `필요한 아이템: ${data.items[ev.needs.item].name} · ${have ? '가방에 있음' : '가방에 없음'}`),
        );
        scrollPanel();
      }
      const yes = await askYesNo();
      panel().append(el('div', 'choice-tag', yes ? '▸ 예' : '▸ 아니오'));
      outcome = yes ? ev.choices.y : ev.choices.n;
      const needsChoice = ev.needs && (ev.needs.choice === 'y') === yes;
      if (needsChoice && !hasItem(ev.needs.item)) outcome = ev.needs.missing;
    }
    if (outcome.text) await typeBlock(outcome.text, 'outcome');
    applyAffection(outcome.affection);
    applyItems(outcome.items);
    showMoney(outcome.money || 0);
    await next();
  }

  async function runGame() {
    state.balance = job().startBalance;
    state.shownBalance = state.balance;
    state.affection = data.lovers.map(() => 0);
    state.inventory = {};
    state.used = new Set();
    state.turn = 0;
    state.result = null;
    const seed = params.has('seed') ? Number(params.get('seed')) : Math.floor(Math.random() * 2 ** 32);
    state.rng = mulberry32(seed);

    show('game');
    renderStatus();
    await playStories(0);

    while (true) {
      state.turn++;
      renderStatus();
      if (state.turn >= data.settings.endingTurn) return runEnding();
      await playEvent();
      await playStories(state.turn);
      if (state.balance <= 0) return runBroke();
    }
  }

  async function runEnding() {
    clearPanel();
    await typeBlock(job().endingIntro, 'story');
    const chosen = await ask(
      data.lovers.map((l, i) => ({
        label: `${l.color} 머리`,
        value: i,
        keys: [String(i + 1)],
        cls: 'lover',
        color: loverVar(i),
        hint: String(i + 1),
      })),
      'lovers',
    );
    clearPanel();
    await typeBlock(confessText(job(), chosen), 'story');
    const lover = data.lovers[chosen];
    if (lover.favoriteItem && hasItem(lover.favoriteItem)) {
      // Carrying the lover's favorite item makes the confession more convincing.
      state.inventory[lover.favoriteItem] -= 1;
      renderBag();
      await typeBlock(fmt(data.ui.giftLine, { item: data.items[lover.favoriteItem].object, color: lover.color }), 'outcome');
      changeAffection(chosen, data.settings.giftBonus || 0);
    }
    await next('고백한다');

    let key = 'noMoney';
    if (state.balance >= minBalance(job()))
      key = state.affection[chosen] >= minAffection(job()) ? 'true' : 'noLove';
    const text = fmt(localize(endingText(job(), key)), { color: data.lovers[chosen].color });
    clearPanel();
    await typeBlock(text, 'ending');
    await finish({ key, lover: chosen, text, title: lastLine(text) });
  }

  async function runBroke() {
    const text = localize(endingText(job(), 'broke'));
    clearPanel();
    await typeBlock(text, 'ending');
    await finish({ key: 'broke', lover: null, text, title: '파산 엔딩' });
  }

  async function finish(result) {
    result.isNew = unlock(collectionId(job().id, result.key, result.lover));
    state.result = result;
    await next('결과 보기');
    renderResult();
    show('result');
  }

  // ---------- Result ----------

  function renderResult() {
    const r = state.result;
    const card = $('result-card');
    card.style.setProperty('--c', r.lover == null ? 'var(--muted)' : loverVar(r.lover));
    $('result-who').textContent =
      `${state.name}의 7일 · ${job().name} · ${data.difficulties[state.difficulty].name}` + (r.isNew ? ' · 새 엔딩!' : '');
    $('result-title').textContent = r.title;
    const body = r.text
      .trim()
      .split('\n')
      .map((l) => l.replace(/^@\S+\s+/, ''))
      .filter((l) => l.trim() && l.trim() !== r.title)
      .join('\n');
    $('result-text').textContent = body;
    $('result-balance').textContent = won(state.balance);
    $('result-lover').textContent = r.lover == null ? '-' : `${data.lovers[r.lover].color} 머리`;

    const max = Math.max(10, ...state.affection);
    const aff = $('result-affection');
    aff.replaceChildren(el('p', 'muted small', '호감도 공개'));
    data.lovers.forEach((l, i) => {
      const row = el('div', `aff-row${r.lover === i ? ' chosen' : ''}`);
      row.style.setProperty('--c', loverVar(i));
      const bar = el('div', 'bar');
      const fill = el('i');
      fill.style.setProperty('--w', `${(Math.max(0, state.affection[i]) / max) * 100}%`);
      bar.append(fill);
      row.append(el('span', null, `${l.color} 머리`), bar, el('b', null, String(state.affection[i])));
      aff.append(row);
    });
    $('result-rule').textContent =
      `고백 성공 조건: 잔고 ${won(minBalance(job()))} 이상, 상대 호감도 ${minAffection(job())} 이상 · 엔딩 도감 ${Object.keys(unlocked()).length} / ${collectionTotal()}`;
  }

  function shareText() {
    const r = state.result;
    return `[Capitalism] ${state.name}(${job().name})의 7일: ${r.title} · 최종 잔고 ${won(state.balance)}`;
  }

  async function shareResult() {
    const url = location.origin + location.pathname;
    const text = shareText();
    try {
      if (navigator.share) {
        await navigator.share({ title: 'Capitalism', text, url });
        return;
      }
      await navigator.clipboard.writeText(`${text}\n${url}`);
      toast('결과를 클립보드에 복사했어요');
    } catch (err) {
      if (err && err.name === 'AbortError') return;
      toast('공유하지 못했어요');
    }
  }

  function cssVar(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }

  function resolveColor(value) {
    const m = /^var\((--[\w-]+)\)$/.exec(value);
    return m ? cssVar(m[1]) : value;
  }

  async function saveImage() {
    const r = state.result;
    const W = 1080, H = 1350;
    const canvas = document.createElement('canvas');
    canvas.width = W;
    canvas.height = H;
    const ctx = canvas.getContext('2d');
    if (document.fonts && document.fonts.ready) await document.fonts.ready;
    const font = (weight, size) => `${weight} ${size}px Pretendard, "Apple SD Gothic Neo", "Malgun Gothic", sans-serif`;
    const accent = resolveColor(r.lover == null ? '#8a847b' : loverVar(r.lover));
    const loverColors = data.lovers.map((_, i) => resolveColor(loverVar(i)));

    ctx.fillStyle = '#f6f3ee';
    ctx.fillRect(0, 0, W, H);
    ctx.fillStyle = accent;
    ctx.fillRect(0, 0, W, 24);

    ctx.fillStyle = '#1d1b19';
    ctx.font = font(800, 56);
    ctx.fillText('Capitalism', 80, 150);
    ctx.fillStyle = '#6f6a63';
    ctx.font = font(500, 34);
    ctx.fillText(`${state.name}의 7일 · ${job().name} · ${data.difficulties[state.difficulty].name}`, 80, 210);

    let size = 96;
    ctx.font = font(800, size);
    while (ctx.measureText(r.title).width > W - 160 && size > 48) {
      size -= 4;
      ctx.font = font(800, size);
    }
    ctx.fillStyle = accent;
    ctx.fillText(r.title, 80, 380);

    ctx.fillStyle = '#6f6a63';
    ctx.font = font(600, 34);
    ctx.fillText('최종 잔고', 80, 500);
    ctx.fillStyle = state.balance < 0 ? '#e5484d' : '#1d1b19';
    ctx.font = font(800, 80);
    ctx.fillText(won(state.balance), 80, 590);

    ctx.fillStyle = '#6f6a63';
    ctx.font = font(600, 34);
    ctx.fillText('호감도', 80, 720);
    const max = Math.max(10, ...state.affection);
    data.lovers.forEach((l, i) => {
      const y = 790 + i * 100;
      ctx.fillStyle = r.lover === i ? '#1d1b19' : '#4a4640';
      ctx.font = font(r.lover === i ? 800 : 600, 38);
      ctx.fillText(`${l.color} 머리`, 80, y);
      ctx.fillStyle = '#e7e1d7';
      ctx.fillRect(300, y - 26, 560, 24);
      ctx.fillStyle = loverColors[i];
      ctx.fillRect(300, y - 26, (560 * Math.max(0, state.affection[i])) / max, 24);
      ctx.fillStyle = '#1d1b19';
      ctx.font = font(700, 38);
      ctx.textAlign = 'right';
      ctx.fillText(String(state.affection[i]), W - 80, y);
      ctx.textAlign = 'left';
    });

    ctx.fillStyle = '#6f6a63';
    ctx.font = font(600, 32);
    ctx.fillText('돈이냐 사랑이냐, 7일 생존기', 80, H - 90);

    canvas.toBlob((blob) => {
      if (!blob) {
        toast('이미지를 만들지 못했어요');
        return;
      }
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'capitalism-result.png';
      document.body.append(a);
      a.click();
      a.remove();
      setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    }, 'image/png');
  }

  // ---------- Wiring ----------

  function startFromSetup(e) {
    e.preventDefault();
    const name = $('input-name').value.trim();
    if (!name) {
      $('input-name').focus();
      toast('이름을 입력해주세요');
      return;
    }
    state.name = name;
    state.job = Number(document.querySelector('input[name="job"]:checked').value);
    state.difficulty = Number(document.querySelector('input[name="difficulty"]:checked').value);
    state.gender = Number(document.querySelector('input[name="gender"]:checked').value);
    state.loverGender = Number(document.querySelector('input[name="loverGender"]:checked').value);
    const saved = storage.load();
    Object.assign(saved, {
      name,
      job: state.job,
      difficulty: state.difficulty,
      gender: state.gender,
      loverGender: state.loverGender,
    });
    storage.save(saved);
    runGame();
  }

  function wire() {
    $('btn-start').addEventListener('click', () => {
      renderSetup();
      show('setup');
      $('input-name').focus();
    });
    $('btn-collection').addEventListener('click', () => show('collection'));
    $('setup-form').addEventListener('submit', startFromSetup);
    $('btn-again').addEventListener('click', () => runGame());
    $('btn-share').addEventListener('click', shareResult);
    $('btn-image').addEventListener('click', saveImage);
    $('btn-reset-collection').addEventListener('click', () => {
      if (!confirm('엔딩 도감 기록을 모두 지울까요?')) return;
      const saved = storage.load();
      delete saved.endings;
      storage.save(saved);
      renderCollection();
    });
    document.querySelectorAll('[data-go]').forEach((b) => b.addEventListener('click', () => show(b.dataset.go)));
    $('btn-bag').addEventListener('click', () => openBag(true));
    $('btn-bag-close').addEventListener('click', () => openBag(false));
    $('bag-sheet').addEventListener('click', (e) => {
      if (e.target === $('bag-sheet')) openBag(false);
    });
    panel().addEventListener('click', () => {
      if (typing && skipTyping) skipTyping();
    });
  }

  async function loadData() {
    for (const url of DATA_URLS) {
      try {
        const res = await fetch(url, { cache: 'no-cache' });
        if (res.ok) return await res.json();
      } catch {
        /* try the next location */
      }
    }
    throw new Error('data/game.json을 찾지 못했어요');
  }

  async function boot() {
    try {
      data = await loadData();
    } catch (err) {
      $('loading-message').textContent =
        location.protocol === 'file:'
          ? '파일을 직접 열면 게임 데이터를 읽을 수 없어요. 로컬 서버로 열어주세요 (README 참고).'
          : `게임 데이터를 불러오지 못했어요: ${err.message}`;
      return;
    }
    wire();
    show('title');
  }

  boot();
})();
