let _mcuProfiles = {}; // perfils MCU carregats des del servidor (/gpiocaps)
let _currentMcu  = null; // perfil MCU actiu (null = cap seleccionat)

function fetchGpioCaps() {
  return apiGetGpioCaps().then(function(data) {
    _mcuProfiles = data;
  }).catch(function() { _mcuProfiles = {}; });
}

function selectMcu(mcuId) {
  _currentMcu = _mcuProfiles[mcuId] || null;
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
      row.style.opacity = '0.35';
      row.title = 'GPIO no disponible en aquest MCU';
      row.querySelectorAll('select, input').forEach(function(el) { el.disabled = true; });
      if (sub) { sub.style.opacity = '0.35'; sub.querySelectorAll('select, input').forEach(function(el) { el.disabled = true; }); }
    } else {
      row.style.opacity = '';
      row.title = '';
      if (gpioState[g].func !== 'zcd_reserved') {
        row.querySelectorAll('select, input').forEach(function(el) { el.disabled = false; });
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
  { id: 'none',        i18nKey: 'func_none',       firmwareId: 'none',        hasSubRow: false },
  { id: 'btn',         i18nKey: 'func_btn',         firmwareId: 'btn',         hasSubRow: false },
  { id: 'btn_inv',     i18nKey: 'func_btn_inv',     firmwareId: 'btn_inv',     hasSubRow: false },
  { id: 'relay',       i18nKey: 'func_relay_out',   firmwareId: 'on_off',      hasSubRow: false },
  { id: 'neopixel',    i18nKey: 'func_neopixel',    firmwareId: 'digital_led', hasSubRow: true  },
  { id: 'pwm',         i18nKey: 'func_pwm',         firmwareId: 'pwm',         hasSubRow: true  },
  { id: 'triac_cycle', i18nKey: 'func_triac_cycle', firmwareId: 'triac_cycle', hasSubRow: true  },
  { id: 'triac_phase', i18nKey: 'func_triac_phase', firmwareId: 'triac_fase',  hasSubRow: true  },
];

let gpioState = Array.from({length: 22}, () =>
  ({ name: '', func: 'none', params: {}, zcdGpio: null })
);
let zcdReserved = new Set();

function needsChan() { return false; }

function defaultParams(func) {
  if (func === 'neopixel')    return { numLeds: 1, color: '#ffffff', brightness: 50 };
  if (func === 'pwm')         return { freq: 5000, duty: 50 };
  if (func === 'triac_cycle') return { duty: 50 };
  if (func === 'triac_phase') return { duty: 50 };
  return {};
}

function mapFirmwareToUiFunc(fwId) {
  const map = {
    none: 'none', adc: 'none',
    btn: 'btn', btn_inv: 'btn_inv',
    on_off: 'relay', relay: 'relay', led: 'relay', mosfet: 'relay',
    digital_led: 'neopixel', neopixel: 'neopixel',
    pwm: 'pwm', pwm_ww: 'pwm', pwm_cw: 'pwm',
    triac_fase: 'triac_phase', triac: 'triac_phase',
    triac_cycle: 'triac_cycle', mosfet_pwm: 'triac_cycle',
    zcd: 'zcd',
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
    if (tmpl.select) sel.value = i;
  });
  if (sel.value !== '') {
    const mcuSection = document.getElementById('mcuSection');
    if (mcuSection) mcuSection.style.display = 'none';
  }
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

  gpioState = Array.from({length: 22}, () =>
    ({ name: '', func: 'none', params: {}, zcdGpio: null })
  );
  zcdReserved.clear();

  // Pas 1: decodificar funcs
  for (let g = 0; g <= 21; g++) {
    const funcIdx = gpiomap['f' + g] || 0;
    const fwFuncDef = _funclist[funcIdx];
    const fwId = fwFuncDef ? fwFuncDef.id : 'none';
    gpioState[g].func = mapFirmwareToUiFunc(fwId);
    gpioState[g].params = defaultParams(gpioState[g].func);
  }

  // Pas 2: llegir params per GPIO i reconstruir parelles triac_phase ↔ ZCD
  for (let g = 0; g <= 21; g++) {
    const a = gpiomap['a' + g] || 0;
    const b = gpiomap['b' + g] || 0;
    const c = gpiomap['c' + g] || 0;
    switch (gpioState[g].func) {
      case 'neopixel':
        gpioState[g].params.color      = '#' + a.toString(16).padStart(6, '0');
        gpioState[g].params.brightness = b || 50;
        gpioState[g].params.numLeds    = c || 1;
        break;
      case 'pwm':
        gpioState[g].params.duty = a || 50;
        gpioState[g].params.freq = b || 5000;
        break;
      case 'triac_cycle':
        gpioState[g].params.duty = a || 50;
        break;
      case 'triac_phase':
        gpioState[g].params.duty = a || 50;
        if (b > 0 && b <= 21) {
          gpioState[g].zcdGpio = b;
          gpioState[b].func = 'zcd_reserved';
          zcdReserved.add(b);
        }
        break;
    }
  }

  // Pas 3: llegir noms
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
  td.colSpan = 3;
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
  if (oldZcdGpio !== null) {
    gpioState[oldZcdGpio].func = 'none';
    zcdReserved.delete(oldZcdGpio);
    refreshRow(oldZcdGpio);
  }
  gpioState[ownerGpio].zcdGpio = newZcdGpio;
  if (newZcdGpio !== null) {
    gpioState[newZcdGpio].func = 'zcd_reserved';
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
    zcdReserved.delete(oldZcd);
    gpioState[gpio].zcdGpio = null;
    refreshRow(oldZcd);
  }

  gpioState[gpio].func = newFunc;
  gpioState[gpio].params = defaultParams(newFunc);
  gpioState[gpio].zcdGpio = null;

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
    ({ name: '', func: 'none', params: {}, zcdGpio: null })
  );
  zcdReserved.clear();

  // Aplicar pins de la plantilla
  tmpl.pins.forEach(function(p) {
    const fwDef = _funclist[p.func];
    if (!fwDef) return;
    const uiFunc = mapFirmwareToUiFunc(fwDef.id);
    gpioState[p.gpio].func = uiFunc;
    gpioState[p.gpio].params = defaultParams(uiFunc);
  });

  // Reconstruir parelles triac ↔ zcd (parella pel primer ZCD trobat)
  for (let g = 0; g <= 21; g++) {
    if (gpioState[g].func !== 'triac_phase') continue;
    tmpl.pins.forEach(function(p) {
      if (gpioState[g].zcdGpio !== null) return;
      const fd = _funclist[p.func];
      if (fd && fd.id === 'zcd') {
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
    const nameStr  = s.name             ? ` "${s.name}"`                    : '';
    const zcdStr   = s.zcdGpio !== null ? ` ← ZCD:GPIO${s.zcdGpio}` : '';
    const ps       = Object.entries(s.params || {}).map(([k,v]) => `${k}:${v}`).join(', ');
    const extraStr = ps                 ? ` [${ps}]`                         : '';
    const tag      = s.func === 'zcd_reserved' ? ' (ZCD reservat)' : '';
    console.log(`  GPIO ${String(g).padStart(2)}${nameStr} → ${s.func}${zcdStr}${extraStr}${tag}`);
  }
  if (count === 0) console.log('  (cap GPIO configurat)');
  console.groupEnd();
}

function saveGpioMap() {
  const params = new URLSearchParams();

  for (let g = 0; g <= 21; g++) {
    const s = gpioState[g];
    let firmwareId, a = 0, b = 0, c = 0;

    if (s.func === 'zcd_reserved') {
      firmwareId = 'zcd';
    } else {
      const fd = FUNC_DEFS.find(function(f) { return f.id === s.func; });
      firmwareId = fd ? fd.firmwareId : 'none';
    }

    switch (s.func) {
      case 'neopixel':
        a = parseInt((s.params.color || '#000000').replace('#', ''), 16) || 0;
        b = s.params.brightness !== undefined ? s.params.brightness : 50;
        c = s.params.numLeds || 1;
        break;
      case 'pwm':
        a = s.params.duty !== undefined ? s.params.duty : 50;
        b = s.params.freq || 5000;
        break;
      case 'triac_cycle':
        a = s.params.duty !== undefined ? s.params.duty : 50;
        break;
      case 'triac_phase':
        a = s.params.duty !== undefined ? s.params.duty : 50;
        b = s.zcdGpio !== null ? s.zcdGpio : 0;
        break;
    }

    const funcIdx = _funclist.findIndex(function(f) { return f.id === firmwareId; });
    params.append('f' + g, funcIdx >= 0 ? funcIdx : 0);
    params.append('a' + g, a);
    params.append('b' + g, b);
    params.append('c' + g, c);
    if (s.name) params.set('n' + g, s.name);
  }

  const tmplSel = document.getElementById('templateSelect');
  const tmplIdx = (tmplSel && tmplSel.value !== '') ? tmplSel.value : '-1';
  params.append('tmpl', tmplIdx);

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
