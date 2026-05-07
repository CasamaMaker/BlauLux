let FW_VER = '';

function goTo(page) {
  document.querySelectorAll('.page').forEach(p => p.style.display = 'none');
  document.getElementById('page-' + page).style.display = '';
  if (page === 'wifi' && !_wifiSsid) scanWifi();
}

function fetchVersion() {
  apiGetVersion().then(v => {
      FW_VER = v.trim();
      Object.keys(i18n).forEach(lang => {
        i18n[lang].footer = 'BlauTrigger ' + FW_VER + ' | Martí Casamayor';
      });
      applyTranslations(currentLang);
    })
    .catch(() => {});
}

function sendDutyToServer(value) {
  DBG.hw(`→ /dutty  duty:${value}%`);
  apiSetDuty(value);
}

function sendDutyCWToServer(value) {
  DBG.hw(`→ /duttyCW  duty_cw:${value}%`);
  apiSetDutyCW(value);
}

function sendColorToServer(r, g, b) {
  const hex = '#' + [r, g, b].map(x => x.toString(16).padStart(2, '0')).join('');
  DBG.hw(`→ /color  rgb(${r},${g},${b}) ${hex}`);
  apiSetColor(r, g, b);
}

function handleTestSwitch(checkbox) {
  if (_currentMode === 1) {
    const neoIdx = gpioState.findIndex(function(s) { return s.func === 'neopixel'; });
    const label  = neoIdx >= 0 && gpioState[neoIdx].name ? `"${gpioState[neoIdx].name}" GPIO${neoIdx}` : `GPIO${neoIdx}`;
    DBG.hw(`NeoPixel ${label}: ${checkbox.checked ? 'ON' : 'OFF (apagat)'}`);
    if (checkbox.checked) {
      handleColorChange(document.getElementById('colorPicker'));
    } else {
      sendColorToServer(0, 0, 0);
    }
  }
}

function handleDutySwitch(checkbox) {
  const modeNames = {2: 'PWM Dimmer', 3: 'WW/CW', 4: 'Triac'};
  if (!checkbox.checked) {
    DBG.hw(`${modeNames[_currentMode] || 'Output'}: OFF (duty→0)`);
    sendDutyToServer(0);
    if (_currentMode === 3) sendDutyCWToServer(0);
  } else {
    if (_currentMode === 2) {
      const v = document.getElementById('dutySlider').value;
      DBG.hw(`PWM Dimmer: ON  duty:${v}%`);
      sendDutyToServer(v);
    } else if (_currentMode === 3) {
      const ww = document.getElementById('wwSlider').value;
      const cw = document.getElementById('cwSlider').value;
      DBG.hw(`WW/CW: ON  ww:${ww}%  cw:${cw}%`);
      sendDutyToServer(ww);
      sendDutyCWToServer(cw);
    } else if (_currentMode === 4) {
      const tr  = document.getElementById('triacSlider').value;
      const led = document.getElementById('triacLEDSlider').value;
      DBG.hw(`Triac: ON  potència:${tr}%  LED:${led}%`);
      sendDutyToServer(tr);
      sendDutyCWToServer(led);
    }
  }
}

function handleTriacChange(slider) {
  document.getElementById('triacValue').textContent = slider.value + '%';
  DBG.hw(`Triac: potència → ${slider.value}%`);
  sendDutyToServer(slider.value);
}

function handleTriacLEDChange(slider) {
  document.getElementById('triacLEDValue').textContent = slider.value + '%';
  DBG.hw(`Triac LED: brillantor → ${slider.value}%`);
  sendDutyCWToServer(slider.value);
}

let _freqPollInterval = null;

function fetchAcFreq() {
  apiGetAcFreq().then(data => {
      const el = document.getElementById('acFreqValue');
      if (el) el.textContent = data.freq > 0 ? data.freq.toFixed(1) + ' Hz' : '-- Hz';
    })
    .catch(() => {});
}

function startFreqPolling() {
  stopFreqPolling();
  fetchAcFreq();
  _freqPollInterval = setInterval(fetchAcFreq, 1000);
}

function stopFreqPolling() {
  if (_freqPollInterval !== null) { clearInterval(_freqPollInterval); _freqPollInterval = null; }
  const el = document.getElementById('acFreqValue');
  if (el) el.textContent = '-- Hz';
}

function handleColorChange(picker) {
  const isOn = document.getElementById('testSwitch').checked;
  if (!isOn) return;
  const hex = picker.value;
  DBG.hw(`NeoPixel: color → ${hex}`);
  sendColorToServer(
    parseInt(hex.slice(1,3), 16),
    parseInt(hex.slice(3,5), 16),
    parseInt(hex.slice(5,7), 16)
  );
}

function handleBrightnessChange(slider) {
  document.getElementById('brightnessValue').textContent = slider.value + '%';
  DBG.hw(`NeoPixel: brillantor → ${slider.value}%`);
  sendDutyToServer(slider.value);
}

function handleDutyChange(slider) {
  document.getElementById('dutyValue').textContent = slider.value + '%';
  DBG.hw(`PWM Dimmer: duty → ${slider.value}%`);
  sendDutyToServer(slider.value);
}

function handleWWChange(slider) {
  document.getElementById('wwValue').textContent = slider.value + '%';
  DBG.hw(`WW: brillantor → ${slider.value}%`);
  sendDutyToServer(slider.value);
}

function handleCWChange(slider) {
  document.getElementById('cwValue').textContent = slider.value + '%';
  DBG.hw(`CW: brillantor → ${slider.value}%`);
  sendDutyCWToServer(slider.value);
}

function fetchMyMac() {
  apiGetMyMac().then(data => {
    document.getElementById("myMac").innerHTML = data;
  }).catch(() => {});
}

function fetchBrightnessConfig() {
  const ct = _currentMode;
  if (ct <= 0) return;
  apiGetBrightness().then(data => {
    const map = {1: data.b1, 2: data.b2, 3: data.b3, 4: data.b4};
    const val = map[ct] ?? 100;
    const sliderEl = document.getElementById('brightnessConfigSlider');
    const valueEl  = document.getElementById('brightnessConfigValue');
    if (sliderEl) sliderEl.value = val;
    if (valueEl)  valueEl.textContent = val + '%';
    if (ct === 3 || ct === 4) {
      const cwVal   = data.bcw ?? 100;
      const cwSlider = document.getElementById('brightnessCWConfigSlider');
      const cwValue  = document.getElementById('brightnessCWConfigValue');
      if (cwSlider) cwSlider.value = cwVal;
      if (cwValue)  cwValue.textContent = cwVal + '%';
    }
  }).catch(() => {});
}


function fetchDriveMode() {
  apiGetDriverMode().then(function(mode) {
    _currentMode = parseInt(mode.trim());
    if (isNaN(_currentMode)) _currentMode = -1;
    updateControlTypeUI();
  }).catch(function() {
    _currentMode = -1;
    updateControlTypeUI();
  });
}

function fetchConfigMode() {
  apiGetConfigMode().then(mode => {
    applyConfigMode(mode.trim());
  }).catch(() => {
    applyConfigMode('web');
  });
}

function applyConfigMode(mode) {
  const modeInfo   = document.getElementById('modeInfo');
  const saveBtn    = document.getElementById('gpioSaveBtn');
  const tmplSelect = document.getElementById('templateSelect');
  const tbody      = document.getElementById('gpioTableBody');

  if (mode === 'hardcoded') {
    if (modeInfo)   modeInfo.style.display = 'block';
    if (saveBtn)    saveBtn.style.display = 'none';
    if (tmplSelect) tmplSelect.disabled = true;
    if (tbody) { tbody.querySelectorAll('select,input').forEach(function(el) { el.disabled = true; }); }
    document.getElementById('clearConfigCard').style.display = 'none';
  } else {
    if (modeInfo)   modeInfo.style.display = 'none';
    if (saveBtn)    saveBtn.style.display = '';
    if (tmplSelect) tmplSelect.disabled = false;
  }
}

let _clearPending = false;
let _clearTimer   = null;
function confirmClearConfig() {
  const btn = document.getElementById('clearConfigBtn');
  if (!_clearPending) {
    _clearPending = true;
    btn.textContent = t('clearConfigConfirm');
    btn.style.background   = '#e74c3c';
    btn.style.color        = '#fff';
    btn.style.borderColor  = '#c0392b';
    _clearTimer = setTimeout(function() {
      _clearPending = false;
      btn.textContent = t('clearConfigBtn');
      btn.style.background  = '';
      btn.style.color       = '';
      btn.style.borderColor = '';
    }, 5000);
  } else {
    clearTimeout(_clearTimer);
    _clearPending = false;
    apiClearConfig()
      .then(function() {
        showToast(t('clearConfigDone'));
        btn.disabled    = true;
        btn.textContent = '✓';
        setTimeout(function() { location.reload(); }, 4000);
      })
      .catch(function() {
        showToast(t('clearConfigDone'));
        btn.disabled    = true;
        btn.textContent = '✓';
        setTimeout(function() { location.reload(); }, 4000);
      });
  }
}

let _restartPending = false;
let _restartTimer   = null;
function confirmRestart() {
  const btn = document.getElementById('restartBtn');
  if (!_restartPending) {
    _restartPending = true;
    btn.textContent = t('restartConfirm');
    btn.style.background  = '#e67e22';
    btn.style.color       = '#fff';
    btn.style.borderColor = '#ca6f1e';
    _restartTimer = setTimeout(function() {
      _restartPending = false;
      btn.textContent = t('restartBtn');
      btn.style.background  = '';
      btn.style.color       = '';
      btn.style.borderColor = '';
    }, 5000);
  } else {
    clearTimeout(_restartTimer);
    _restartPending = false;
    apiRestart()
      .then(function() {
        showToast(t('restartDone'));
        btn.disabled    = true;
        btn.textContent = '✓';
        setTimeout(function() { location.reload(); }, 4000);
      })
      .catch(function() {
        showToast(t('restartDone'));
        btn.disabled    = true;
        btn.textContent = '✓';
        setTimeout(function() { location.reload(); }, 4000);
      });
  }
}

let _clearWifiPending = false, _clearWifiTimer = null;
function confirmClearWifi() {
  const btn = document.getElementById('clearWifiBtn');
  if (!_clearWifiPending) {
    _clearWifiPending = true;
    btn.textContent = t('clearConfigConfirm');
    btn.style.background = '#e74c3c'; btn.style.color = '#fff'; btn.style.borderColor = '#c0392b';
    _clearWifiTimer = setTimeout(function() {
      _clearWifiPending = false;
      btn.textContent = t('clearWifiBtn');
      btn.style.background = ''; btn.style.color = ''; btn.style.borderColor = '';
    }, 5000);
  } else {
    clearTimeout(_clearWifiTimer); _clearWifiPending = false;
    apiClearWifi().then(function() {
      showToast(t('clearWifiDone')); btn.disabled = true; btn.textContent = '✓';
      _wifiSsid = ''; wifiConfigLoaded = false;
      scanWifi(); setTimeout(fetchWifiStatus, 2000);
    }).catch(function() {
      showToast(t('clearWifiDone')); btn.disabled = true; btn.textContent = '✓';
      _wifiSsid = ''; wifiConfigLoaded = false;
      scanWifi(); setTimeout(fetchWifiStatus, 2000);
    });
  }
}

let _clearMqttPending = false, _clearMqttTimer = null;
function confirmClearMqtt() {
  const btn = document.getElementById('clearMqttBtn');
  if (!_clearMqttPending) {
    _clearMqttPending = true;
    btn.textContent = t('clearConfigConfirm');
    btn.style.background = '#e74c3c'; btn.style.color = '#fff'; btn.style.borderColor = '#c0392b';
    _clearMqttTimer = setTimeout(function() {
      _clearMqttPending = false;
      btn.textContent = t('clearMqttBtn');
      btn.style.background = ''; btn.style.color = ''; btn.style.borderColor = '';
    }, 5000);
  } else {
    clearTimeout(_clearMqttTimer); _clearMqttPending = false;
    apiClearMqtt().then(function() {
      showToast(t('clearMqttDone')); btn.disabled = true; btn.textContent = '✓';
      mqttConfigLoaded = false; setTimeout(fetchMqttStatus, 2000);
    }).catch(function() {
      showToast(t('clearMqttDone')); btn.disabled = true; btn.textContent = '✓';
      mqttConfigLoaded = false; setTimeout(fetchMqttStatus, 2000);
    });
  }
}

let _clearHwPending = false, _clearHwTimer = null;
function confirmClearHardware() {
  const btn = document.getElementById('clearHwBtn');
  if (!_clearHwPending) {
    _clearHwPending = true;
    btn.textContent = t('clearConfigConfirm');
    btn.style.background = '#e74c3c'; btn.style.color = '#fff'; btn.style.borderColor = '#c0392b';
    _clearHwTimer = setTimeout(function() {
      _clearHwPending = false;
      btn.textContent = t('clearHwBtn');
      btn.style.background = ''; btn.style.color = ''; btn.style.borderColor = '';
    }, 5000);
  } else {
    clearTimeout(_clearHwTimer); _clearHwPending = false;
    apiClearHardware().then(function() {
      showToast(t('clearHwDone')); btn.disabled = true; btn.textContent = '✓';
      setTimeout(function() { location.reload(); }, 4000);
    }).catch(function() {
      showToast(t('clearHwDone')); btn.disabled = true; btn.textContent = '✓';
      setTimeout(function() { location.reload(); }, 4000);
    });
  }
}

let _clearAltresPending = false, _clearAltresTimer = null;
function confirmClearAltres() {
  const btn = document.getElementById('clearAltresBtn');
  if (!_clearAltresPending) {
    _clearAltresPending = true;
    btn.textContent = t('clearConfigConfirm');
    btn.style.background = '#e74c3c'; btn.style.color = '#fff'; btn.style.borderColor = '#c0392b';
    _clearAltresTimer = setTimeout(function() {
      _clearAltresPending = false;
      btn.textContent = t('clearAltresBtn');
      btn.style.background = ''; btn.style.color = ''; btn.style.borderColor = '';
    }, 5000);
  } else {
    clearTimeout(_clearAltresTimer); _clearAltresPending = false;
    apiClearDeviceName().then(function(name) {
      document.getElementById('deviceNameSubtitle').textContent = name;
      document.getElementById('deviceNameInput').value = name;
      showToast(t('clearAltresDone')); btn.disabled = true; btn.textContent = '✓';
    }).catch(function() {
      showToast(t('clearAltresDone')); btn.disabled = true; btn.textContent = '✓';
    });
  }
}

// Detecta si hi ha triac_cycle (mosfet_pwm) standalone i mostra/oculta la secció de test
function updateMosfetTest() {
  const hasMosfetPwm = gpioState.some(function(s) { return s.func === 'triac_cycle'; });
  const sec = document.getElementById('testMosfetSection');
  if (sec) {
    const ctrlVisible = document.getElementById('hwTestControls');
    sec.style.display = (hasMosfetPwm && ctrlVisible && ctrlVisible.style.display !== 'none') ? 'flex' : 'none';
  }
}

function handleMosfetDutyChange(slider) {
  document.getElementById('mosfetDutyValue').textContent = slider.value + '%';
  DBG.hw(`MOSFET PWM: duty → ${slider.value}%`);
  apiSetDutyMosfet(slider.value);
}

function updateControlTypeUI() {
  const ct = _currentMode;

  document.getElementById('testDutySwitch').checked = false;

  const noCt = (ct < 0);
  document.getElementById('hwStatusRow').style.display    = noCt ? 'flex' : 'none';
  document.getElementById('hwTestControls').style.display = noCt ? 'none' : 'flex';

  if (noCt) { stopFreqPolling(); updateMosfetTest(); return; }

  const testToggle  = document.getElementById('testToggleSection');
  const testDigital = document.getElementById('testDigitalSection');
  const testDuty    = document.getElementById('testDutySection');
  const testDimmer  = document.getElementById('testDimmerSlider');
  const testWW      = document.getElementById('testWWSlider');
  const testCW      = document.getElementById('testCWSlider');
  const testTriac   = document.getElementById('testTriacSlider');
  const testTriacLED = document.getElementById('testTriacLEDSlider');

  testToggle.style.display   = (ct === 0 || ct === 1) ? 'flex' : 'none';
  testDigital.style.display  = (ct === 1)             ? 'flex' : 'none';
  testDuty.style.display     = (ct >= 2 && ct <= 4)   ? 'flex' : 'none';
  testDimmer.style.display   = (ct === 2)             ? 'flex' : 'none';
  testWW.style.display       = (ct === 3)             ? 'flex' : 'none';
  testCW.style.display       = (ct === 3)             ? 'flex' : 'none';
  testTriac.style.display    = (ct === 4)             ? 'flex' : 'none';
  testTriacLED.style.display = (ct === 4)             ? 'flex' : 'none';

  updateExtraParams();
  updateMosfetTest();

  if (ct === 4) startFreqPolling(); else stopFreqPolling();
  if (ct >= 1)  fetchBrightnessConfig();
}

document.addEventListener('DOMContentLoaded', function() {
  document.getElementById('testToggleSection').style.display = 'none';
  document.getElementById('testDigitalSection').style.display = 'none';
  document.getElementById('testDutySection').style.display = 'none';
  document.getElementById('testTriacSlider').style.display = 'none';
  document.getElementById('testTriacLEDSlider').style.display = 'none';
});

let _currentMode = -1;
let isInitialSetup = false;

function fetchInitialSetup() {
  apiGetInitialSetup().then(data => {
    isInitialSetup = data.trim() === 'true';
    if (isInitialSetup) {
      document.getElementById('initialSetupBanner').style.display = 'block';
    }
  }).catch(() => {});
}

function showToast(msg) {
  const toast = document.getElementById('toast');
  toast.textContent = msg;
  toast.classList.add('show');
  setTimeout(() => toast.classList.remove('show'), 3000);
}

function fetchWifiStatus() {
  apiGetWifiStatus().then(data => {
    const badge   = document.getElementById('wifiStatusBadge');
    const ipRow   = document.getElementById('wifiIpRow');
    const ssidRow = document.getElementById('wifiSsidRow');
    const pageBadge = document.getElementById('wifiPageBadge');
    const pageIp    = document.getElementById('wifiPageIp');
    if (data.connected) {
      const txt = t('wifiConnected') + ' (' + data.rssi + ' dBm)';
      badge.textContent = txt;
      badge.style.color = '#27ae60';
      pageBadge.textContent = txt;
      pageBadge.style.color = '#27ae60';
      pageIp.textContent = 'IP: ' + data.ip;
      pageIp.style.display = 'block';
      document.getElementById('wifiIp').textContent = data.ip;
      ipRow.style.display = 'block';
      ssidRow.style.display = 'none';
    } else if (data.ssid) {
      badge.textContent = t('wifiDisconnected');
      badge.style.color = '#e67e22';
      pageBadge.textContent = t('wifiDisconnected');
      pageBadge.style.color = '#e67e22';
      pageIp.style.display = 'none';
      document.getElementById('wifiSsidLabel').textContent = t('wifiConfigured') + data.ssid;
      ipRow.style.display = 'none';
      ssidRow.style.display = 'block';
    } else {
      badge.textContent = t('wifiNotConfigured');
      badge.style.color = '#888';
      pageBadge.textContent = t('wifiNotConfigured');
      pageBadge.style.color = '#888';
      pageIp.style.display = 'none';
      document.getElementById('wifiSsidLabel').textContent = t('wifiNotConfigured');
      ipRow.style.display = 'none';
      ssidRow.style.display = 'block';
    }
    if (!wifiConfigLoaded) {
      wifiConfigLoaded = true;
      _wifiSsid = data.ssid || '';
      document.getElementById('wifiPassInput').value = data.pass || '';
      if (_wifiSsid) {
        const sel = document.getElementById('wifiSsidSelect');
        let found = false;
        for (const opt of sel.options) { if (opt.value === _wifiSsid) { opt.selected = true; found = true; break; } }
        if (!found) {
          const opt = document.createElement('option');
          opt.value = _wifiSsid;
          opt.textContent = _wifiSsid;
          opt.selected = true;
          sel.appendChild(opt);
        }
      }
    }
  }).catch(() => {});
}

let wifiConfigLoaded = false;
let _wifiSsid = '';
let mqttConfigLoaded = false;

function fetchMqttStatus() {
  apiGetMqttStatus().then(data => {
    const badge      = document.getElementById('mqttStatusBadge');
    const topicRow   = document.getElementById('mqttTopicRow');
    const pageBadge  = document.getElementById('mqttPageBadge');
    const pageTopic  = document.getElementById('mqttPageTopic');
    if (data.connected) {
      badge.textContent = t('mqttConnected');
      badge.style.color = '#27ae60';
      pageBadge.textContent = t('mqttConnected');
      pageBadge.style.color = '#27ae60';
      pageTopic.textContent = data.topic;
      pageTopic.style.display = 'block';
      document.getElementById('mqttTopic').textContent = data.topic;
      topicRow.style.display = 'block';
    } else if (data.broker) {
      badge.textContent = t('mqttDisconnected');
      badge.style.color = '#e67e22';
      pageBadge.textContent = t('mqttDisconnected');
      pageBadge.style.color = '#e67e22';
      pageTopic.style.display = 'none';
      topicRow.style.display = 'none';
    } else {
      badge.textContent = t('wifiNotConfigured');
      badge.style.color = '#888';
      pageBadge.textContent = t('wifiNotConfigured');
      pageBadge.style.color = '#888';
      pageTopic.style.display = 'none';
      topicRow.style.display = 'none';
    }
    if (data.broker !== undefined && !mqttConfigLoaded) {
      mqttConfigLoaded = true;
      document.getElementById('mqttHostInput').value      = data.broker;
      document.getElementById('mqttPortInput').value      = data.port || 1883;
      document.getElementById('mqttClientInput').value    = data.client    || '';
      document.getElementById('mqttTopicInput').value     = data.mqtt_topic || '';
      document.getElementById('mqttFulltopicInput').value = data.fulltopic || '';
      document.getElementById('mqttUserInput').value      = data.user || '';
      document.getElementById('mqttPassInput').value      = data.pass || '';
    }
  }).catch(() => {});
}

function saveMqttConfig() {
  const host      = document.getElementById('mqttHostInput').value.trim();
  const port      = document.getElementById('mqttPortInput').value.trim() || '1883';
  const client    = document.getElementById('mqttClientInput').value.trim();
  const topic     = document.getElementById('mqttTopicInput').value.trim();
  const fulltopic = document.getElementById('mqttFulltopicInput').value.trim();
  const user      = document.getElementById('mqttUserInput').value.trim();
  const pass      = document.getElementById('mqttPassInput').value;
  apiSaveMqtt(host, port, client, topic, fulltopic, user, pass).then(() => {
    showToast(t('mqttSaved'));
    mqttConfigLoaded = false;
    setTimeout(fetchMqttStatus, 3000);
  }).catch(() => showToast(t('mqttError')));
}

function fetchDeviceName() {
  apiGetDeviceName().then(name => {
      const n = name.trim();
      document.getElementById('deviceNameSubtitle').textContent = n;
      document.getElementById('deviceNameInput').value = n;
    })
    .catch(() => {});
}

function saveDeviceName() {
  const name = document.getElementById('deviceNameInput').value.trim();
  if (!name) return;
  apiSaveDeviceName(name).then(() => {
    document.getElementById('deviceNameSubtitle').textContent = name;
    showToast(t('altresSaved'));
  }).catch(() => showToast(t('altresError')));
}

function scanWifi() {
  const sel = document.getElementById('wifiSsidSelect');
  sel.disabled = true;
  sel.innerHTML = '<option value="">' + t('wifiScanning') + '</option>';
  apiScanWifi().then(nets => {
    sel.innerHTML = '<option value="">' + t('wifiSelectSsid') + '</option>';
    nets.sort((a, b) => b.rssi - a.rssi).forEach(n => {
      const opt = document.createElement('option');
      opt.value = n.ssid;
      opt.textContent = n.ssid + '  (' + n.rssi + ' dBm)' + (n.enc ? ' 🔒' : '');
      sel.appendChild(opt);
    });
    const target = _wifiSsid;
    if (target) {
      for (const opt of sel.options) { if (opt.value === target) { opt.selected = true; break; } }
    }
  }).catch(() => {
    sel.innerHTML = '<option value="">' + t('wifiSelectSsid') + '</option>';
  }).finally(() => {
    sel.disabled = false;
  });
}

function saveWifiCredentials() {
  const wifiSsid = document.getElementById('wifiSsidSelect').value.trim();
  const wifiPass = document.getElementById('wifiPassInput').value;
  apiSaveWifi(wifiSsid, wifiPass).then(() => {
    showToast(t('wifiSaved'));
    wifiConfigLoaded = false;
    setTimeout(fetchWifiStatus, 3000);
  }).catch(() => showToast(t('wifiError')));
}

window.onload = function() {
  currentLang = detectLang();
  document.getElementById('langSelector').value = currentLang;
  applyTranslations(currentLang);

  fetchVersion();
  fetchMyMac();
  fetchDeviceName();
  fetchConfigMode();
  fetchDriveMode();
  fetchInitialSetup();
  fetchWifiStatus();
  setInterval(fetchWifiStatus, 5000);

  fetchMqttStatus();
  setInterval(fetchMqttStatus, 10000);

  fetchFunclist().then(fetchTemplates).then(fetchGpioMap);
}
