// ── Perfils MCU (GPIOs 0-21 per defecte; ampliar quan el fw suporti més) ──
const MCU_PROFILES = {
  'esp32c3': {
    name: 'ESP32-C3',
    caps: [
      {valid:true,  hasPwm:true,  hasAdc:true},   // 0
      {valid:true,  hasPwm:true,  hasAdc:true},   // 1
      {valid:true,  hasPwm:true,  hasAdc:true},   // 2
      {valid:true,  hasPwm:true,  hasAdc:true},   // 3
      {valid:true,  hasPwm:true,  hasAdc:true},   // 4
      {valid:true,  hasPwm:true,  hasAdc:false},  // 5
      {valid:true,  hasPwm:true,  hasAdc:false},  // 6
      {valid:true,  hasPwm:true,  hasAdc:false},  // 7
      {valid:true,  hasPwm:true,  hasAdc:false},  // 8
      {valid:true,  hasPwm:true,  hasAdc:false},  // 9
      {valid:true,  hasPwm:true,  hasAdc:false},  // 10
      {valid:true,  hasPwm:true,  hasAdc:false},  // 11 (flash CS)
      {valid:true,  hasPwm:true,  hasAdc:false},  // 12 (flash CLK)
      {valid:true,  hasPwm:true,  hasAdc:false},  // 13 (flash DIO)
      {valid:true,  hasPwm:true,  hasAdc:false},  // 14 (flash DIO)
      {valid:true,  hasPwm:true,  hasAdc:false},  // 15 (flash DIO)
      {valid:true,  hasPwm:true,  hasAdc:false},  // 16 (flash DIO)
      {valid:true,  hasPwm:true,  hasAdc:false},  // 17 (flash DIO)
      {valid:false, hasPwm:false, hasAdc:false},  // 18 (USB D-)
      {valid:false, hasPwm:false, hasAdc:false},  // 19 (USB D+)
      {valid:true,  hasPwm:true,  hasAdc:false},  // 20
      {valid:true,  hasPwm:true,  hasAdc:false},  // 21
    ]
  },
  'esp32s3': {
    name: 'ESP32-S3',
    caps: [
      {valid:true, hasPwm:true,  hasAdc:false},  // 0  (strapping)
      {valid:true, hasPwm:true,  hasAdc:true},   // 1
      {valid:true, hasPwm:true,  hasAdc:true},   // 2
      {valid:true, hasPwm:true,  hasAdc:true},   // 3
      {valid:true, hasPwm:true,  hasAdc:true},   // 4
      {valid:true, hasPwm:true,  hasAdc:true},   // 5
      {valid:true, hasPwm:true,  hasAdc:true},   // 6
      {valid:true, hasPwm:true,  hasAdc:true},   // 7
      {valid:true, hasPwm:true,  hasAdc:true},   // 8
      {valid:true, hasPwm:true,  hasAdc:true},   // 9
      {valid:true, hasPwm:true,  hasAdc:true},   // 10
      {valid:true, hasPwm:true,  hasAdc:false},  // 11
      {valid:true, hasPwm:true,  hasAdc:false},  // 12
      {valid:true, hasPwm:true,  hasAdc:false},  // 13
      {valid:true, hasPwm:true,  hasAdc:false},  // 14
      {valid:true, hasPwm:true,  hasAdc:false},  // 15
      {valid:true, hasPwm:true,  hasAdc:false},  // 16
      {valid:true, hasPwm:true,  hasAdc:false},  // 17
      {valid:true, hasPwm:false, hasAdc:false},  // 18 (USB D-)
      {valid:true, hasPwm:false, hasAdc:false},  // 19 (USB D+)
      {valid:true, hasPwm:true,  hasAdc:false},  // 20
      {valid:true, hasPwm:true,  hasAdc:false},  // 21
    ]
  },
};

let _currentMcu = null; // perfil MCU actiu (null = cap seleccionat)

function selectMcu(mcuId) {
  _currentMcu = MCU_PROFILES[mcuId] || null;
  try { localStorage.setItem('blau_mcu', mcuId || ''); } catch(e) {}
  applyMcuProfile();
}

function applyMcuProfile() {
  if (!_currentMcu) return;
  for (let g = 0; g <= 21; g++) {
    const cap = _currentMcu.caps[g];
    const row = document.getElementById('gpio_row_' + g);
    const sub = document.getElementById('gpio_sub_' + g);
    if (!row) continue;
    if (!cap || !cap.valid) {
      gpioState[g].func = 'none';
      gpioState[g].chan  = 0;
      row.style.opacity = '0.35';
      row.title = 'GPIO no disponible en aquest MCU';
      row.querySelectorAll('select, input').forEach(function(el) { el.disabled = true; });
      if (sub) { sub.style.opacity = '0.35'; sub.querySelectorAll('select, input').forEach(function(el) { el.disabled = true; }); }
    } else {
      row.style.opacity = '';
      row.title = '';
      if (gpioState[g].func !== 'zcd_reserved') {
        row.querySelectorAll('select, input').forEach(function(el) { el.disabled = false; });
        const cInp = document.getElementById('gpio_chan_' + g);
        if (cInp) cInp.disabled = !needsChan(gpioState[g].func);
      }
      if (sub) { sub.style.opacity = ''; sub.querySelectorAll('select, input').forEach(function(el) { el.disabled = false; }); }
    }
  }
  validateGpioMap();
}

let _funclist  = [];
let _templates = [];

// ── Model de dades intern ──
const FUNC_DEFS = [
  { id: 'none',        i18nKey: 'func_none',       firmwareId: 'none',       hasSubRow: false, needsChan: false },
  { id: 'btn',         i18nKey: 'func_btn',         firmwareId: 'btn',        hasSubRow: false, needsChan: true  },
  { id: 'btn_inv',     i18nKey: 'func_btn_inv',     firmwareId: 'btn_inv',    hasSubRow: false, needsChan: true  },
  { id: 'relay',       i18nKey: 'func_relay_out',   firmwareId: 'relay',      hasSubRow: false, needsChan: true  },
  { id: 'neopixel',    i18nKey: 'func_neopixel',    firmwareId: 'neopixel',   hasSubRow: true,  needsChan: true  },
  { id: 'pwm',         i18nKey: 'func_pwm',         firmwareId: 'pwm',        hasSubRow: true,  needsChan: true  },
  { id: 'triac_cycle', i18nKey: 'func_triac_cycle', firmwareId: 'mosfet_pwm', hasSubRow: true,  needsChan: true  },
  { id: 'triac_phase', i18nKey: 'func_triac_phase', firmwareId: 'triac',      hasSubRow: true,  needsChan: true  },
];

let gpioState = Array.from({length: 22}, () =>
  ({ name: '', func: 'none', chan: 0, params: {}, zcdGpio: null })
);
let zcdReserved = new Set();

function needsChan(func) {
  const fd = FUNC_DEFS.find(f => f.id === func);
  return fd ? fd.needsChan : false;
}

function defaultParams(func) {
  if (func === 'neopixel')    return { numLeds: 1, color: '#ffffff', brightness: 50 };
  if (func === 'pwm')         return { freq: 5000, duty: 50 };
  if (func === 'triac_cycle') return { duty: 50 };
  if (func === 'triac_phase') return { duty: 50 };
  return {};
}

function mapFirmwareToUiFunc(fwId) {
  const map = {
    relay: 'relay', led: 'relay', mosfet: 'relay',
    neopixel: 'neopixel', pwm: 'pwm',
    pwm_ww: 'pwm', pwm_cw: 'pwm',
    triac: 'triac_phase', mosfet_pwm: 'triac_cycle',
    zcd: 'none',
    btn: 'btn', btn_inv: 'btn_inv',
    none: 'none', adc: 'none',
  };
  return map[fwId] || 'none';
}

function fetchFunclist() {
  return apiGetFunclist().then(data => {
    _funclist = data;
  }).catch(() => { _funclist = []; });
}

function fetchTemplates() {
  return apiGetTemplates().then(data => {
    _templates = data;
    populateTemplateSelect();
  }).catch(() => { _templates = []; });
}

function populateTemplateSelect() {
  const sel = document.getElementById('templateSelect');
  if (!sel) return;
  sel.innerHTML = '<option value="">' + t('templateCustom') + '</option>';
  _templates.forEach(function(tmpl, i) {
    const opt = document.createElement('option');
    opt.value = i;
    opt.textContent = tmpl.name;
    sel.appendChild(opt);
  });
}

function onTemplateChange(val) {
  const mcuSection = document.getElementById('mcuSection');
  if (mcuSection) mcuSection.style.display = (val === '') ? '' : 'none';
  applyTemplate(val);
}

function fetchGpioMap() {
  return apiGetGpioMap().then(function(data) {
    buildGpioTable(data);
    // Restaura el selector MCU des de localStorage
    try {
      const savedMcu = localStorage.getItem('blau_mcu');
      const mcuSel   = document.getElementById('mcuSelect');
      if (mcuSel && savedMcu) { mcuSel.value = savedMcu; selectMcu(savedMcu); }
    } catch(e) {}
    validateGpioMap();
    updateMosfetTest();
  }).catch(function() {});
}

function buildGpioTable(gpiomap) {
  const tbody = document.getElementById('gpioTableBody');
  if (!tbody) return;

  // Reinicialitzar estat
  gpioState = Array.from({length: 22}, () =>
    ({ name: '', func: 'none', chan: 0, params: {}, zcdGpio: null })
  );
  zcdReserved.clear();

  // Pas 1: decodificar totes les entrades del firmware
  for (let g = 0; g <= 21; g++) {
    const info = gpiomap['g' + g] || {func: 0, chan: 0};
    const fwFuncDef = _funclist[info.func];
    const fwId = fwFuncDef ? fwFuncDef.id : 'none';
    gpioState[g].func = mapFirmwareToUiFunc(fwId);
    gpioState[g].chan  = info.chan || 0;
    gpioState[g].params = defaultParams(gpioState[g].func);
  }

  // Pas 2: reconstruir parelles triac_phase ↔ zcd
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func !== 'triac_phase') continue;
    const ch = gpioState[g].chan;
    for (let z = 0; z <= 21; z++) {
      if (z === g) continue;
      const info = gpiomap['g' + z] || {func: 0, chan: 0};
      const fwFuncDef = _funclist[info.func];
      if (fwFuncDef && fwFuncDef.id === 'zcd' && info.chan === ch) {
        gpioState[g].zcdGpio = z;
        gpioState[z].func = 'zcd_reserved';
        zcdReserved.add(z);
        break;
      }
    }
  }

  // Pas 3: semar params globals als GPIOs corresponents
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func === 'neopixel') {
      if (gpiomap.nl    !== undefined) gpioState[g].params.numLeds   = gpiomap.nl;
      if (gpiomap.b1    !== undefined) gpioState[g].params.brightness = gpiomap.b1;
      if (gpiomap.color !== undefined) gpioState[g].params.color      = '#' + gpiomap.color;
      break;
    }
  }
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func === 'pwm' || gpioState[g].func === 'triac_cycle') {
      if (gpiomap.pf !== undefined) gpioState[g].params.freq = gpiomap.pf;
      if (gpiomap.pd !== undefined) gpioState[g].params.duty = gpiomap.pd;
      break;
    }
  }
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func === 'triac_phase') {
      if (gpiomap.b4 !== undefined) gpioState[g].params.duty = gpiomap.b4;
      break;
    }
  }
  // Semar noms dels pins
  for (let g = 0; g <= 21; g++) {
    const nm = gpiomap['n' + g];
    if (nm !== undefined) gpioState[g].name = nm;
  }

  // Pas 4: construir DOM
  tbody.innerHTML = '';
  for (let g = 0; g <= 21; g++) {
    tbody.appendChild(buildGpioRow(g));
    const fd = FUNC_DEFS.find(f => f.id === gpioState[g].func);
    if (fd && fd.hasSubRow) tbody.appendChild(buildSubRow(g));
  }
}

function buildGpioRow(gpio) {
  const state = gpioState[gpio];
  const isZcdReserved = (state.func === 'zcd_reserved');
  const tr = document.createElement('tr');
  tr.id = 'gpio_row_' + gpio;
  const fd0 = FUNC_DEFS.find(function(f) { return f.id === state.func; });
  const rowClasses = [];
  if (isZcdReserved) rowClasses.push('gpio-zcd-reserved');
  if (fd0 && fd0.hasSubRow) rowClasses.push('gpio-has-subrow');
  tr.className = rowClasses.join(' ');

  // Col 1: nom editable
  const tdName = document.createElement('td');
  tdName.style.cssText = 'padding:2px 3px;';
  const nameInp = document.createElement('input');
  nameInp.type = 'text';
  nameInp.id = 'gpio_name_' + gpio;
  nameInp.value = state.name || '';
  nameInp.placeholder = t('gpioNamePlaceholder');
  nameInp.maxLength = 12;
  nameInp.style.cssText = 'width:100%; font-size:0.82em; border:1px solid #ddd; border-radius:4px; padding:2px 4px; box-sizing:border-box;';
  if (isZcdReserved) nameInp.disabled = true;
  nameInp.oninput = function() { onNameChange(gpio); };
  tdName.appendChild(nameInp);
  tr.appendChild(tdName);

  // Col 2: número GPIO
  const tdGpio = document.createElement('td');
  tdGpio.textContent = gpio;
  tdGpio.style.cssText = 'text-align:center; font-weight:600; color:#555; padding:4px 2px; white-space:nowrap;';
  if (isZcdReserved) {
    const badge = document.createElement('span');
    badge.textContent = ' ZCD';
    badge.style.cssText = 'font-size:0.7em; color:#e67e22; font-weight:400;';
    tdGpio.appendChild(badge);
  }
  tr.appendChild(tdGpio);

  // Col 3: funció
  const tdFunc = document.createElement('td');
  tdFunc.style.cssText = 'padding:2px 4px;';
  const sel = document.createElement('select');
  sel.id = 'gpio_func_' + gpio;
  sel.style.cssText = 'width:100%; font-size:0.82em;';
  sel.disabled = isZcdReserved;
  FUNC_DEFS.forEach(function(fd) {
    const opt = document.createElement('option');
    opt.value = fd.id;
    opt.textContent = t(fd.i18nKey);
    if (fd.id === state.func) opt.selected = true;
    sel.appendChild(opt);
  });
  if (isZcdReserved) {
    // Mostrar com a "ZCD reservat" però sense afegir-ho a FUNC_DEFS
    const opt = document.createElement('option');
    opt.value = 'zcd_reserved';
    opt.textContent = 'ZCD (reservat)';
    opt.selected = true;
    sel.appendChild(opt);
  }
  sel.onchange = function() { onFuncChange(gpio); };
  tdFunc.appendChild(sel);
  tr.appendChild(tdFunc);

  // Col 4: canal
  const tdChan = document.createElement('td');
  tdChan.style.cssText = 'padding:2px 4px; text-align:center;';
  const chanInp = document.createElement('input');
  chanInp.type = 'number';
  chanInp.id = 'gpio_chan_' + gpio;
  chanInp.min = 1; chanInp.max = 15;
  chanInp.style.cssText = 'width:44px; text-align:center; font-size:0.82em;';
  const hasCh = needsChan(state.func);
  const isInputFunc = (state.func === 'btn' || state.func === 'btn_inv');
  chanInp.disabled = !hasCh || isZcdReserved;
  chanInp.min = isInputFunc ? 0 : 1;
  chanInp.value = (hasCh && state.chan > 0) ? state.chan : '';
  chanInp.placeholder = isInputFunc ? '—' : (hasCh ? '1' : '—');
  chanInp.oninput = function() { onChanChange(gpio); };
  tdChan.appendChild(chanInp);
  tr.appendChild(tdChan);

  return tr;
}

function buildSubRow(gpio) {
  const state = gpioState[gpio];
  const func = state.func;
  const params = state.params;

  const tr = document.createElement('tr');
  tr.id = 'gpio_sub_' + gpio;
  tr.className = 'gpio-sub-row';

  const td = document.createElement('td');
  td.colSpan = 4;
  td.className = 'gpio-sub-cell';

  const inner = document.createElement('div');
  inner.className = 'gpio-sub-inner';

  if (func === 'neopixel') {
    inner.appendChild(makeSubField('labelNumLedsShort',
      makeNumberInput('sub_nl_' + gpio, params.numLeds || 1, 1, 500, '', function(v) {
        DBG.change(`GPIO ${gpio} neopixel: numLeds → ${v}`);
        gpioState[gpio].params.numLeds = v;
      })));
    inner.appendChild(makeSubField('labelColor',
      makeColorInput('sub_color_' + gpio, params.color || '#ffffff', function(v) {
        DBG.change(`GPIO ${gpio} neopixel: color → ${v}`);
        gpioState[gpio].params.color = v;
      })));
    inner.appendChild(makeSubField('labelBrightness',
      makeSliderInput('sub_br_' + gpio, params.brightness !== undefined ? params.brightness : 50, 0, 100, '%', function(v) {
        DBG.change(`GPIO ${gpio} neopixel: brightness → ${v}%`);
        gpioState[gpio].params.brightness = v;
      })));
  } else if (func === 'pwm') {
    inner.appendChild(makeSubField('labelPwmFreq',
      makeNumberInput('sub_pf_' + gpio, params.freq || 5000, 100, 40000, 'Hz', function(v) {
        DBG.change(`GPIO ${gpio} pwm: freq → ${v} Hz`);
        gpioState[gpio].params.freq = v;
      })));
    inner.appendChild(makeSubField('labelPwmDuty',
      makeSliderInput('sub_pd_' + gpio, params.duty !== undefined ? params.duty : 50, 0, 100, '%', function(v) {
        DBG.change(`GPIO ${gpio} pwm: duty → ${v}%`);
        gpioState[gpio].params.duty = v;
      })));
  } else if (func === 'triac_cycle') {
    inner.appendChild(makeSubField('labelPwmDuty',
      makeSliderInput('sub_dc_' + gpio, params.duty !== undefined ? params.duty : 50, 0, 100, '%', function(v) {
        DBG.change(`GPIO ${gpio} triac_cycle: duty → ${v}%`);
        gpioState[gpio].params.duty = v;
      })));
  } else if (func === 'triac_phase') {
    inner.appendChild(makeSubField('labelZcdGpio', makeZcdSelect(gpio)));
    inner.appendChild(makeSubField('labelPwmDuty',
      makeSliderInput('sub_dp_' + gpio, params.duty !== undefined ? params.duty : 50, 0, 100, '%', function(v) {
        DBG.change(`GPIO ${gpio} triac_phase: duty → ${v}%`);
        gpioState[gpio].params.duty = v;
      })));
  }

  td.appendChild(inner);
  tr.appendChild(td);
  return tr;
}

function makeSubField(labelKey, inputEl) {
  const div = document.createElement('div');
  div.className = 'gpio-sub-field';
  const lbl = document.createElement('label');
  lbl.textContent = t(labelKey);
  div.appendChild(lbl);
  div.appendChild(inputEl);
  return div;
}

function makeNumberInput(id, val, min, max, unit, onChange) {
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex; align-items:center; gap:4px;';
  const inp = document.createElement('input');
  inp.type = 'number'; inp.id = id;
  inp.min = min; inp.max = max; inp.value = val;
  inp.style.cssText = 'width:6ch; text-align:center; font-size:1em;';
  inp.oninput = function() { onChange(parseInt(inp.value) || min); };
  wrap.appendChild(inp);
  if (unit) {
    const sp = document.createElement('span');
    sp.textContent = unit;
    sp.style.cssText = 'font-weight:600; color:var(--primary-color);';
    wrap.appendChild(sp);
  }
  return wrap;
}

function makeColorInput(id, val, onChange) {
  const inp = document.createElement('input');
  inp.type = 'color'; inp.id = id; inp.value = val;
  inp.oninput = function() { onChange(inp.value); };
  return inp;
}

function makeSliderInput(id, val, min, max, unit, onChange) {
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex; align-items:center; gap:6px; flex:1;';
  const slider = document.createElement('input');
  slider.type = 'range'; slider.id = id;
  slider.min = min; slider.max = max; slider.value = val;
  slider.className = 'duty-slider';
  slider.style.cssText = 'flex:1;';
  const valSpan = document.createElement('span');
  valSpan.textContent = val + unit;
  valSpan.style.cssText = 'min-width:3ch; font-weight:600; color:var(--primary-color); text-align:right;';
  slider.oninput = function() {
    valSpan.textContent = slider.value + unit;
    onChange(parseInt(slider.value));
  };
  wrap.appendChild(slider);
  wrap.appendChild(valSpan);
  return wrap;
}

function makeZcdSelect(ownerGpio) {
  const sel = document.createElement('select');
  sel.id = 'sub_zcd_' + ownerGpio;
  sel.style.cssText = 'font-size:0.9em;';
  const blank = document.createElement('option');
  blank.value = '';
  blank.textContent = t('zcdSelectHint');
  sel.appendChild(blank);
  for (let z = 0; z <= 21; z++) {
    if (z === ownerGpio) continue;
    const isCurrentZcd = (z === gpioState[ownerGpio].zcdGpio);
    const isAvailable = (gpioState[z].func === 'none' || isCurrentZcd);
    if (!isAvailable) continue;
    const opt = document.createElement('option');
    opt.value = z;
    const name = gpioState[z].name ? ' (' + gpioState[z].name + ')' : '';
    opt.textContent = 'GPIO ' + z + name;
    if (isCurrentZcd) opt.selected = true;
    sel.appendChild(opt);
  }
  sel.onchange = function() {
    const v = sel.value === '' ? null : parseInt(sel.value);
    onZcdGpioChange(ownerGpio, v);
  };
  return sel;
}

function onZcdGpioChange(ownerGpio, newZcdGpio) {
  const oldZcdGpio = gpioState[ownerGpio].zcdGpio;
  // Alliberar antic
  if (oldZcdGpio !== null) {
    gpioState[oldZcdGpio].func = 'none';
    gpioState[oldZcdGpio].chan = 0;
    zcdReserved.delete(oldZcdGpio);
    refreshRow(oldZcdGpio);
  }
  // Reservar nou
  gpioState[ownerGpio].zcdGpio = newZcdGpio;
  if (newZcdGpio !== null) {
    gpioState[newZcdGpio].func = 'zcd_reserved';
    gpioState[newZcdGpio].chan = gpioState[ownerGpio].chan;
    zcdReserved.add(newZcdGpio);
    refreshRow(newZcdGpio);
  }
  // Reconstruir sub-row del propietari per actualitzar les opcions disponibles
  const existingSub = document.getElementById('gpio_sub_' + ownerGpio);
  const newSub = buildSubRow(ownerGpio);
  if (existingSub) existingSub.replaceWith(newSub);
  validateGpioMap();
}

function refreshRow(gpio) {
  const oldRow = document.getElementById('gpio_row_' + gpio);
  const oldSub = document.getElementById('gpio_sub_' + gpio);
  if (!oldRow) return;
  const newRow = buildGpioRow(gpio);
  oldRow.replaceWith(newRow);
  const fd = FUNC_DEFS.find(f => f.id === gpioState[gpio].func);
  if (fd && fd.hasSubRow) {
    const newSub = buildSubRow(gpio);
    if (oldSub) oldSub.replaceWith(newSub);
    else newRow.insertAdjacentElement('afterend', newSub);
  } else {
    if (oldSub) oldSub.remove();
  }
}

function rebuildZcdReserved() {
  zcdReserved.clear();
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func !== 'triac_phase') continue;
    const z = gpioState[g].zcdGpio;
    if (z !== null) {
      gpioState[z].func = 'zcd_reserved';
      zcdReserved.add(z);
    }
  }
}

function onFuncChange(gpio) {
  const sel = document.getElementById('gpio_func_' + gpio);
  if (!sel) return;
  const newFunc = sel.value;
  const oldFunc = gpioState[gpio].func;
  const gpioName = gpioState[gpio].name ? ` "${gpioState[gpio].name}"` : '';
  DBG.change(`GPIO ${gpio}${gpioName}: func  ${oldFunc} → ${newFunc}`);

  // Alliberar ZCD si era triac_phase
  if (oldFunc === 'triac_phase' && gpioState[gpio].zcdGpio !== null) {
    const oldZcd = gpioState[gpio].zcdGpio;
    gpioState[oldZcd].func = 'none';
    gpioState[oldZcd].chan = 0;
    zcdReserved.delete(oldZcd);
    gpioState[gpio].zcdGpio = null;
    refreshRow(oldZcd);
  }

  gpioState[gpio].func = newFunc;
  gpioState[gpio].params = defaultParams(newFunc);
  gpioState[gpio].zcdGpio = null;

  if (!needsChan(newFunc)) {
    gpioState[gpio].chan = 0;
    const cInp = document.getElementById('gpio_chan_' + gpio);
    if (cInp) { cInp.disabled = true; cInp.value = ''; }
  } else {
    const cInp = document.getElementById('gpio_chan_' + gpio);
    if (cInp) cInp.disabled = false;
  }

  // Actualitzar sub-row
  const existingSub = document.getElementById('gpio_sub_' + gpio);
  const fd = FUNC_DEFS.find(f => f.id === newFunc);
  if (fd && fd.hasSubRow) {
    const newSub = buildSubRow(gpio);
    if (existingSub) existingSub.replaceWith(newSub);
    else {
      const row = document.getElementById('gpio_row_' + gpio);
      if (row) row.insertAdjacentElement('afterend', newSub);
    }
  } else {
    if (existingSub) existingSub.remove();
  }

  validateGpioMap();
}

function onChanChange(gpio) {
  const inp = document.getElementById('gpio_chan_' + gpio);
  if (!inp) return;
  const v = parseInt(inp.value);
  const oldChan = gpioState[gpio].chan;
  const isInput = (gpioState[gpio].func === 'btn' || gpioState[gpio].func === 'btn_inv');
  gpioState[gpio].chan = isInput ? (v >= 0 && v <= 15 ? v : 0) : (v >= 1 && v <= 15 ? v : 0);
  const gpioName = gpioState[gpio].name ? ` "${gpioState[gpio].name}"` : '';
  DBG.change(`GPIO ${gpio}${gpioName}: canal  ${oldChan} → ${gpioState[gpio].chan}`);
  // Propagar canal al GPIO ZCD si existeix
  if (gpioState[gpio].func === 'triac_phase' && gpioState[gpio].zcdGpio !== null) {
    gpioState[gpioState[gpio].zcdGpio].chan = gpioState[gpio].chan;
    DBG.change(`GPIO ${gpioState[gpio].zcdGpio} (ZCD): canal propagat → ${gpioState[gpio].chan}`);
  }
  validateGpioMap();
}

function onNameChange(gpio) {
  const inp = document.getElementById('gpio_name_' + gpio);
  if (!inp) return;
  const oldName = gpioState[gpio].name;
  gpioState[gpio].name = inp.value.trim();
  if (oldName !== gpioState[gpio].name)
    DBG.change(`GPIO ${gpio}: nom  "${oldName}" → "${gpioState[gpio].name}"`);
  // Actualitzar les opcions dels selects ZCD que mostren aquest GPIO
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func !== 'triac_phase') continue;
    const zcdSel = document.getElementById('sub_zcd_' + g);
    if (!zcdSel) continue;
    for (const opt of zcdSel.options) {
      if (parseInt(opt.value) === gpio) {
        const name = gpioState[gpio].name ? ' (' + gpioState[gpio].name + ')' : '';
        opt.textContent = 'GPIO ' + gpio + name;
        break;
      }
    }
  }
}

function applyTemplate(idx) {
  if (idx === '' || idx === null) return;
  const tmpl = _templates[parseInt(idx)];
  if (!tmpl) return;

  // Reinicialitzar estat
  gpioState = Array.from({length: 22}, () =>
    ({ name: '', func: 'none', chan: 0, params: {}, zcdGpio: null })
  );
  zcdReserved.clear();

  // Aplicar pins de la plantilla
  tmpl.pins.forEach(function(p) {
    const fwDef = _funclist[p.func];
    if (!fwDef) return;
    const uiFunc = mapFirmwareToUiFunc(fwDef.id);
    gpioState[p.gpio].func = uiFunc;
    gpioState[p.gpio].chan  = p.chan || 0;
    gpioState[p.gpio].params = defaultParams(uiFunc);
  });

  // Reconstruir parelles triac ↔ zcd
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func !== 'triac_phase') continue;
    const ch = gpioState[g].chan;
    tmpl.pins.forEach(function(p) {
      const fd = _funclist[p.func];
      if (fd && fd.id === 'zcd' && p.chan === ch) {
        gpioState[g].zcdGpio = p.gpio;
        gpioState[p.gpio].func = 'zcd_reserved';
        zcdReserved.add(p.gpio);
      }
    });
  }

  // Reconstruir DOM
  const tbody = document.getElementById('gpioTableBody');
  if (!tbody) return;
  tbody.innerHTML = '';
  for (let g = 0; g <= 21; g++) {
    tbody.appendChild(buildGpioRow(g));
    const fd = FUNC_DEFS.find(f => f.id === gpioState[g].func);
    if (fd && fd.hasSubRow) tbody.appendChild(buildSubRow(g));
  }
  applyMcuProfile();
  validateGpioMap();
}

function updateExtraParams() {
  // Els paràmetres per funció estan als sub-rows per GPIO.
  // Les seccions legacy de brillantor global estan amagades.
  const brSection   = document.getElementById('brightnessConfigSection');
  const brCwSection = document.getElementById('brightnessCWConfigSection');
  if (brSection)   brSection.style.display   = 'none';
  if (brCwSection) brCwSection.style.display = 'none';
}

function validateGpioMap() {
  const msgEl = document.getElementById('gpioValidation');
  if (!msgEl) return;
  const msgs = [];
  let hasErrors = false;
  let hasBtn = false;

  for (let g = 0; g <= 21; g++) {
    const s = gpioState[g];
    if (s.func === 'zcd_reserved') continue;
    if (s.func === 'btn' || s.func === 'btn_inv') hasBtn = true;
    if (s.func !== 'none') {
      const isInput = (s.func === 'btn' || s.func === 'btn_inv');
      // Inputs poden tenir chan=0 (controla tots els canals); outputs necessiten chan 1-15
      if (!isInput && !(s.chan >= 1 && s.chan <= 15)) {
        msgs.push('GPIO ' + g + ': ' + t('gpioValidNeedsChan'));
        hasErrors = true;
      }
    }
    if (s.func === 'triac_phase' && s.zcdGpio === null) {
      msgs.push('GPIO ' + g + ': ' + t('gpioValidNeedsZcd'));
      hasErrors = true;
    }
  }

  if (!hasBtn) msgs.push(t('gpioValidNoBtn'));

  if (hasErrors) {
    msgs.forEach(function(m) { DBG.valid('ERROR: ' + m); });
  } else if (msgs.length > 0) {
    msgs.forEach(function(m) { DBG.valid('Avís: ' + m); });
  } else {
    DBG.ok('Validació GPIO OK — sense errors ni avisos');
  }

  msgEl.textContent = msgs.join(' | ');
  msgEl.style.color = hasErrors ? '#e74c3c' : '#e67e22';
  const saveBtn = document.getElementById('gpioSaveBtn');
  if (saveBtn) saveBtn.disabled = hasErrors;
}

function debugLogGpioConfig() {
  console.groupCollapsed('%c[CONFIG] Guardat: mapa GPIO complet', 'color:#2980b9;font-weight:700');
  let count = 0;
  for (let g = 0; g <= 21; g++) {
    const s = gpioState[g];
    if (s.func === 'none') continue;
    count++;
    const nameStr  = s.name      ? ` "${s.name}"`                    : '';
    const chanStr  = s.chan > 0  ? ` canal:${s.chan}`                 : '';
    const zcdStr   = s.zcdGpio !== null ? ` ← ZCD:GPIO${s.zcdGpio}` : '';
    const ps       = Object.entries(s.params || {}).map(([k,v]) => `${k}:${v}`).join(', ');
    const extraStr = ps          ? ` [${ps}]`                         : '';
    const tag      = s.func === 'zcd_reserved' ? ' (ZCD reservat)' : '';
    console.log(`  GPIO ${String(g).padStart(2)}${nameStr} → ${s.func}${chanStr}${zcdStr}${extraStr}${tag}`);
  }
  if (count === 0) console.log('  (cap GPIO configurat)');
  console.groupEnd();
}

function saveGpioMap() {
  const params = new URLSearchParams();

  // Pas 1: escriure tots els GPIOs (ZCD reservats → none provisionalment)
  for (let g = 0; g <= 21; g++) {
    const s = gpioState[g];
    let firmwareId, chan;
    if (s.func === 'zcd_reserved') {
      firmwareId = 'none'; chan = 0;
    } else {
      const fd = FUNC_DEFS.find(function(f) { return f.id === s.func; });
      firmwareId = fd ? fd.firmwareId : 'none';
      chan = (s.func !== 'none') ? (s.chan || 0) : 0;
    }
    const funcIdx = _funclist.findIndex(function(f) { return f.id === firmwareId; });
    const func = funcIdx >= 0 ? funcIdx : 0;
    params.append('g' + g, ((chan & 0xF) << 4) | (func & 0xF));
  }

  // Pas 2: sobreescriure GPIOs ZCD amb FUNC_ZCD al canal del triac
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func !== 'triac_phase' || gpioState[g].zcdGpio === null) continue;
    const zcdGpio = gpioState[g].zcdGpio;
    const chan = gpioState[g].chan || 0;
    const zcdIdx = _funclist.findIndex(function(f) { return f.id === 'zcd'; });
    const zcdFunc = zcdIdx >= 0 ? zcdIdx : 0;
    params.set('g' + zcdGpio, ((chan & 0xF) << 4) | (zcdFunc & 0xF));
  }

  // Params globals (backward compat amb el firmware)
  const neopixelIdx = gpioState.findIndex(function(s) { return s.func === 'neopixel'; });
  const triacIdx    = gpioState.findIndex(function(s) { return s.func === 'triac_phase'; });
  const pwmIdx      = gpioState.findIndex(function(s) { return s.func === 'pwm' || s.func === 'triac_cycle'; });

  if (neopixelIdx >= 0) {
    const np = gpioState[neopixelIdx].params;
    if (np.numLeds)                  params.set('nl', np.numLeds);
    if (np.color)                    params.set('color', np.color.replace('#', ''));
    if (np.brightness !== undefined) params.set(triacIdx >= 0 ? 'bcw' : 'b', np.brightness);
  }
  if (pwmIdx >= 0) {
    const pp = gpioState[pwmIdx].params;
    if (pp.freq)              params.set('pf', pp.freq);
    if (pp.duty !== undefined) params.set('pd', pp.duty);
  }
  if (triacIdx >= 0 && gpioState[triacIdx].params.duty !== undefined) {
    params.set('b', gpioState[triacIdx].params.duty);
  }

  // Noms dels pins
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].name) params.set('n' + g, gpioState[g].name);
  }

  debugLogGpioConfig();
  DBG.action('Enviant configuració GPIO al servidor...');

  apiSaveGpioMap(params).then(function() {
    DBG.ok('Configuració GPIO guardada correctament');
    showToast(t('toastSavedRestart'));
    setTimeout(function() { location.reload(); }, 4000);
  }).catch(function() {
    DBG.valid('Error en guardar la configuració GPIO');
    showToast(t('toastError'));
  });
}
