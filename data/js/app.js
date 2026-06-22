let FW_VER = '';

function goTo(page) {
  document.querySelectorAll('.page').forEach(p => p.style.display = 'none');
  document.getElementById('page-' + page).style.display = '';
  if (page === 'wifi' && !_wifiSsid) scanWifi();
  if (page === 'seguretat') fetchSecurityStatus();
  if (page === 'ota') fetchOtaVersion();
}

function fetchVersion() {
  apiGetVersion().then(v => {
      FW_VER = v.trim();
      Object.keys(i18n).forEach(lang => {
        i18n[lang].footer = 'BlauLux ' + FW_VER + ' | Martí Casamayor';
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

// ── ESP-NOW / BlauProtocol v2 — estat home ───────────────────────

function fetchEspNowStatus() {
  apiGetSecurityStatus().then(function(data) {
    const badge = document.getElementById('espnowStatusBadge');
    if (badge) {
      badge.textContent = data.configured ? t('espnowActive') : t('espnowLegacy');
      badge.style.color = data.configured ? '#27ae60' : '#888';
    }
    const wlLabel = document.getElementById('espnowWlLabel');
    if (wlLabel) {
      if (!data.configured) {
        wlLabel.textContent = '';
      } else if (!data.wlMacs || data.wlMacs.length === 0) {
        wlLabel.textContent = t('secWlLearning');
      } else {
        wlLabel.textContent = data.wlMacs.length + ' ' + t('espnowWlLabel');
      }
    }
  }).catch(function() {});
}

// ── Seguretat BlauProtocol v2 ─────────────────────────────────────

function fetchSecurityStatus() {
  apiGetSecurityStatus().then(function(data) {
    const badge = document.getElementById('secStatusBadge');
    badge.textContent = data.configured ? t('secConfigured') : t('secNotConfigured');
    badge.style.color = data.configured ? '#27ae60' : '#888';
    const list = document.getElementById('secWlList');
    if (!data.configured) {
      list.textContent = '—';
    } else if (!data.wlMacs || data.wlMacs.length === 0) {
      list.textContent = t('secWlLearning');
    } else {
      list.innerHTML = data.wlMacs.map(function(m) {
        return '<div style="font-weight:600;">' + m + '</div>';
      }).join('');
    }
  }).catch(function() {});
}

var _learningTimer = null;
var _learningEnd = 0;

function startLearning() {
  apiStartLearning().then(function(r) {
    if (!r.ok) { showToast(t('secError')); return; }
    _learningEnd = Date.now() + 60000;
    var btn = document.getElementById('btnStartLearning');
    var cd  = document.getElementById('learningCountdown');
    btn.disabled = true;
    cd.style.display = '';
    clearInterval(_learningTimer);
    _learningTimer = setInterval(function() {
      var rem = Math.ceil((_learningEnd - Date.now()) / 1000);
      if (rem <= 0) {
        clearInterval(_learningTimer);
        cd.style.display = 'none';
        btn.disabled = false;
        fetchSecurityStatus();
        return;
      }
      cd.textContent = t('secWlLearningActive').replace('{s}', rem);
      fetchSecurityStatus();
    }, 1000);
  }).catch(function() { showToast(t('secError')); });
}

function saveSecurityPass() {
  const pass = document.getElementById('secPassInput').value.trim();
  if (pass.length < 8 || pass.length > 63) { showToast(t('secLenError')); return; }
  apiSaveSecurity(pass).then(function(r) {
    if (!r.ok) throw new Error();
    document.getElementById('secPassInput').value = '';
    showToast(t('secSaved'));
    fetchSecurityStatus();
  }).catch(function() { showToast(t('secError')); });
}

let _clearSecPending = false, _clearSecTimer = null;
function confirmClearSecurity() {
  const btn = document.getElementById('clearSecBtn');
  if (!_clearSecPending) {
    _clearSecPending = true;
    btn.textContent = t('clearConfigConfirm');
    btn.style.background = '#e74c3c'; btn.style.color = '#fff'; btn.style.borderColor = '#c0392b';
    _clearSecTimer = setTimeout(function() {
      _clearSecPending = false;
      btn.textContent = t('secClearBtn');
      btn.style.background = ''; btn.style.color = ''; btn.style.borderColor = '';
    }, 5000);
  } else {
    clearTimeout(_clearSecTimer); _clearSecPending = false;
    btn.textContent = t('secClearBtn');
    btn.style.background = ''; btn.style.color = ''; btn.style.borderColor = '';
    apiClearSecurity().then(function() {
      showToast(t('secCleared'));
      fetchSecurityStatus();
    }).catch(function() {
      showToast(t('secCleared'));
      fetchSecurityStatus();
    });
  }
}

// Mosfet standalone: la visibilitat la gestiona fetchChannels() via el camp mosfet
function updateMosfetTest() {}

function handleMosfetDutyChange(slider) {
  document.getElementById('mosfetDutyValue').textContent = slider.value + '%';
  DBG.hw(`MOSFET PWM: duty → ${slider.value}%`);
  apiSetDutyMosfet(slider.value);
}

function updateControlTypeUI() {
  updateExtraParams();
  updateMosfetTest();
  if (_currentMode >= 1) fetchBrightnessConfig();
}


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

function fetchPowerupMode() {
  apiGetPowerupMode().then(v => {
    const sel = document.getElementById('powerupSelect');
    if (sel) sel.value = v.trim();
  }).catch(() => {});
}

function savePowerupMode() {
  const v = document.getElementById('powerupSelect').value;
  apiSavePowerupMode(v).then(() => {
    const st = document.getElementById('powerupStatus');
    if (st) {
      st.textContent = t('powerupSaved');
      setTimeout(() => { st.textContent = ''; }, 2000);
    }
  }).catch(() => showToast(t('altresError')));
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

// ── Canals (Fase 5) ───────────────────────────────────────────────

let _chPollInterval = null;
let _chDebounce = {};

const _chTypeLabels = {
  onoff: 'On/Off', pwm: 'PWM', pwm_cct: 'CCT',
  neopixel: 'Led digital', triac_cycle: 'Triac/cicle', triac_phase: 'Triac/fase'
};

function startChannelPolling() {
  stopChannelPolling();
  fetchChannels();
  _chPollInterval = setInterval(fetchChannels, 3000);
}

function stopChannelPolling() {
  if (_chPollInterval !== null) { clearInterval(_chPollInterval); _chPollInterval = null; }
}

function fetchChannels() {
  apiGetChannels().then(function(data) {
    renderChannelList(data.channels || []);
    const mosfetSec = document.getElementById('testMosfetSection');
    if (mosfetSec) {
      const hasMosfet = data.mosfet >= 0;
      mosfetSec.style.display = hasMosfet ? 'flex' : 'none';
      if (hasMosfet) {
        const slider = document.getElementById('mosfetDutySlider');
        const valEl  = document.getElementById('mosfetDutyValue');
        if (slider && !_chDebounce['mosfet']) { slider.value = data.mosfet; }
        if (valEl  && !_chDebounce['mosfet']) { valEl.textContent = data.mosfet + '%'; }
      }
    }
  }).catch(function() {});
}

function renderChannelList(channels) {
  const list = document.getElementById('channelList');
  if (!list) return;

  const existing = {};
  list.querySelectorAll('[data-ch]').forEach(function(el) { existing[el.dataset.ch] = el; });
  const newIds = channels.map(function(c) { return String(c.id); });

  Object.keys(existing).forEach(function(id) {
    if (!newIds.includes(id)) existing[id].remove();
  });

  if (channels.length === 0) {
    if (!list.querySelector('.ch-empty'))
      list.innerHTML = '<span class="ch-empty" style="font-size:0.9em; color:#888;">' + t('channelsEmpty') + '</span>';
    return;
  }
  list.querySelectorAll('.ch-empty').forEach(function(el) { el.remove(); });

  channels.forEach(function(ch) {
    if (existing[String(ch.id)]) {
      _updateChannelCard(ch);
    } else {
      list.appendChild(_createChannelCard(ch));
    }
  });
}

function _createChannelCard(ch) {
  const div = document.createElement('div');
  div.dataset.ch = ch.id;
  div.style.cssText = 'border:1px solid #eee; border-radius:8px; padding:10px; display:flex; flex-direction:column; gap:10px;';

  const typeLbl  = _chTypeLabels[ch.type] || ch.type;
  const nameBadge = ch.name ? ' <span style="font-size:0.78em; background:#e8f4fd; color:#2980b9; padding:1px 6px; border-radius:4px;">' + ch.name + '</span>' : '';
  let html = '<div style="display:flex; justify-content:space-between; align-items:center;">'
           + '<span style="font-weight:600;">' + typeLbl + nameBadge + '</span>'
           + '<label class="toggle-switch">'
           + '<input type="checkbox" id="ch' + ch.id + '-sw"' + (ch.on ? ' checked' : '') + ' onchange="chSetOn(' + ch.id + ', this.checked)">'
           + '<span class="toggle-slider"></span>'
           + '</label></div>';

  if (ch.hasFreq) {
    const fMin  = ch.type === 'triac_cycle' ? 5   : 300;
    const fMax  = ch.type === 'triac_cycle' ? 50  : 40000;
    const fStep = ch.type === 'triac_cycle' ? 1   : 100;
    const fDisp = ch.type === 'triac_cycle' ? (ch.freq / 10).toFixed(1) + ' Hz' : ch.freq + ' Hz';
    const fLabel = ch.type === 'triac_cycle' ? t('labelCycleFreq') : t('labelPwmFreq');
    html += '<div style="display:flex; flex-direction:column; gap:4px;">'
          + '<div style="display:flex; justify-content:space-between; align-items:center;">'
          + '<label style="margin:0; font-size:0.85em;">' + fLabel + '</label>'
          + '<span id="ch' + ch.id + '-freq-v" style="font-size:0.85em; font-weight:600; color:var(--primary-color);">' + fDisp + '</span>'
          + '</div>'
          + '<input type="range" id="ch' + ch.id + '-freq" min="' + fMin + '" max="' + fMax + '" step="' + fStep + '" value="' + ch.freq + '" class="duty-slider" oninput="chSetFreq(' + ch.id + ', this, \'' + ch.type + '\')">'
          + '</div>';
  }

  if (ch.hasBr) {
    html += '<div style="display:flex; flex-direction:column; gap:4px;">'
          + '<div style="display:flex; justify-content:space-between; align-items:center;">'
          + '<label style="margin:0; font-size:0.85em;">' + t('labelBrightness') + '</label>'
          + '<span id="ch' + ch.id + '-br-v" style="font-size:0.85em; font-weight:600; color:var(--primary-color);">' + ch.br + '%</span>'
          + '</div>'
          + '<input type="range" id="ch' + ch.id + '-br" min="0" max="100" value="' + ch.br + '" class="duty-slider" oninput="chSetBr(' + ch.id + ', this)">'
          + '</div>';
  }

  if (ch.hasCCT) {
    html += '<div style="display:flex; flex-direction:column; gap:4px;">'
          + '<div style="display:flex; justify-content:space-between; align-items:center;">'
          + '<label style="margin:0; font-size:0.85em;">' + t('labelBrightnessCWTest') + '</label>'
          + '<span id="ch' + ch.id + '-br2-v" style="font-size:0.85em; font-weight:600; color:var(--primary-color);">' + ch.br2 + '%</span>'
          + '</div>'
          + '<input type="range" id="ch' + ch.id + '-br2" min="0" max="100" value="' + ch.br2 + '" class="duty-slider" oninput="chSetBr2(' + ch.id + ', this)">'
          + '</div>';
  }

  if (ch.hasColor) {
    const hex = '#' + (ch.color || 'ffffff').toLowerCase().padStart(6, '0');
    html += '<div style="display:flex; justify-content:space-between; align-items:center;">'
          + '<label style="margin:0; font-size:0.85em;">' + t('labelColor') + '</label>'
          + '<input type="color" id="ch' + ch.id + '-color" value="' + hex + '" class="color-picker" oninput="chSetColor(' + ch.id + ', this)">'
          + '</div>';
  }

  div.innerHTML = html;
  return div;
}

function _updateChannelCard(ch) {
  const sw = document.getElementById('ch' + ch.id + '-sw');
  if (sw && !_chDebounce['on' + ch.id]) sw.checked = ch.on;

  const freqEl = document.getElementById('ch' + ch.id + '-freq');
  const freqV  = document.getElementById('ch' + ch.id + '-freq-v');
  if (freqEl && !_chDebounce['freq' + ch.id]) {
    freqEl.value = ch.freq;
    if (freqV) freqV.textContent = ch.type === 'triac_cycle' ? (ch.freq / 10).toFixed(1) + ' Hz' : ch.freq + ' Hz';
  }

  const brEl = document.getElementById('ch' + ch.id + '-br');
  const brV  = document.getElementById('ch' + ch.id + '-br-v');
  if (brEl && !_chDebounce['br' + ch.id]) {
    brEl.value = ch.br;
    if (brV) brV.textContent = ch.br + '%';
  }

  const br2El = document.getElementById('ch' + ch.id + '-br2');
  const br2V  = document.getElementById('ch' + ch.id + '-br2-v');
  if (br2El && !_chDebounce['br2' + ch.id]) {
    br2El.value = ch.br2;
    if (br2V) br2V.textContent = ch.br2 + '%';
  }

  if (ch.hasColor) {
    const colorEl = document.getElementById('ch' + ch.id + '-color');
    if (colorEl && !_chDebounce['color' + ch.id])
      colorEl.value = '#' + (ch.color || 'ffffff').toLowerCase().padStart(6, '0');
  }
}

function chSetOn(ch, on) {
  DBG.hw('CH' + ch + ': ' + (on ? 'ON' : 'OFF'));
  _chDebounce['on' + ch] = true;
  apiSetChannel(ch, 'on=' + (on ? '1' : '0')).finally(function() {
    delete _chDebounce['on' + ch];
  });
}

function chSetFreq(ch, slider, type) {
  const v   = parseInt(slider.value);
  const vEl = document.getElementById('ch' + ch + '-freq-v');
  if (vEl) vEl.textContent = type === 'triac_cycle' ? (v / 10).toFixed(1) + ' Hz' : v + ' Hz';
  clearTimeout(_chDebounce['freq' + ch + '_t']);
  _chDebounce['freq' + ch] = true;
  _chDebounce['freq' + ch + '_t'] = setTimeout(function() {
    DBG.hw('CH' + ch + ': freq → ' + v);
    apiSetChannel(ch, 'freq=' + v).finally(function() { delete _chDebounce['freq' + ch]; });
  }, 200);
}

function chSetBr(ch, slider) {
  const v = slider.value;
  const vEl = document.getElementById('ch' + ch + '-br-v');
  if (vEl) vEl.textContent = v + '%';
  clearTimeout(_chDebounce['br' + ch + '_t']);
  _chDebounce['br' + ch] = true;
  _chDebounce['br' + ch + '_t'] = setTimeout(function() {
    DBG.hw('CH' + ch + ': brightness → ' + v + '%');
    apiSetChannel(ch, 'br=' + v).finally(function() { delete _chDebounce['br' + ch]; });
  }, 200);
}

function chSetBr2(ch, slider) {
  const v = slider.value;
  const vEl = document.getElementById('ch' + ch + '-br2-v');
  if (vEl) vEl.textContent = v + '%';
  clearTimeout(_chDebounce['br2' + ch + '_t']);
  _chDebounce['br2' + ch] = true;
  _chDebounce['br2' + ch + '_t'] = setTimeout(function() {
    DBG.hw('CH' + ch + ': brightness2 → ' + v + '%');
    apiSetChannel(ch, 'br2=' + v).finally(function() { delete _chDebounce['br2' + ch]; });
  }, 200);
}

function chSetColor(ch, picker) {
  const hex = picker.value;
  clearTimeout(_chDebounce['color' + ch + '_t']);
  _chDebounce['color' + ch] = true;
  _chDebounce['color' + ch + '_t'] = setTimeout(function() {
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    DBG.hw('CH' + ch + ': color → ' + hex);
    apiSetChannel(ch, 'r=' + r + '&g=' + g + '&b=' + b).finally(function() { delete _chDebounce['color' + ch]; });
  }, 200);
}

// ── OTA ──────────────────────────────────────────────────────────

function fetchOtaVersion() {
  var el = document.getElementById('otaVersion');
  if (el) el.textContent = FW_VER ? t('otaCurrentVer') + FW_VER : '';
}

function startOTA() {
  var file = document.getElementById('otaFile').files[0];
  if (!file) { _otaMsg(t('otaNoFile'), false); return; }
  var xhr = new XMLHttpRequest();
  xhr.upload.onprogress = function(e) {
    if (!e.lengthComputable) return;
    var pct = Math.round(e.loaded / e.total * 100);
    document.getElementById('otaProgress').value = pct;
    document.getElementById('otaPct').textContent = pct + '%';
  };
  xhr.onload = function() {
    var ok = false;
    try { ok = JSON.parse(xhr.responseText).ok; } catch(e) {}
    _otaMsg(ok ? t('otaOk') : t('otaError'), !ok);
  };
  xhr.onerror = function() { _otaMsg(t('otaError'), true); };
  var form = new FormData();
  form.append('file', file, file.name);
  document.getElementById('otaProgressWrap').style.display = 'block';
  _otaMsg(t('otaUploading'), false);
  xhr.open('POST', '/ota-upload');
  xhr.send(form);
}

function _otaMsg(text, isError) {
  var el = document.getElementById('otaStatus');
  if (el) { el.textContent = text; el.style.color = isError ? '#e74c3c' : '#555'; }
}

window.onload = function() {
  currentLang = detectLang();
  document.getElementById('langSelector').value = currentLang;
  applyTranslations(currentLang);

  fetchVersion();
  fetchMyMac();
  fetchPowerupMode();
  fetchDeviceName();
  fetchConfigMode();
  fetchDriveMode();
  fetchInitialSetup();
  fetchWifiStatus();
  setInterval(fetchWifiStatus, 5000);

  fetchMqttStatus();
  setInterval(fetchMqttStatus, 10000);

  fetchEspNowStatus();
  setInterval(fetchEspNowStatus, 30000);

  startChannelPolling();

  fetchGpioCaps().then(fetchFunclist).then(fetchTemplates).then(fetchGpioMap);
}
