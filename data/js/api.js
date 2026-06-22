// ── Capa HTTP — wrappers fetch sense lògica DOM ──────────────────────────────

function _postForm(path, body) {
  return fetch(path, {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: body
  });
}

// Dispositiu
function apiGetVersion()      { return fetch('/version').then(r => r.text()); }
function apiGetMyMac()        { return fetch('/mymac',       {method:'POST'}).then(r => r.text()); }
function apiGetDriverMode()   { return fetch('/driverMode',  {method:'POST'}).then(r => r.text()); }
function apiGetConfigMode()   { return fetch('/configMode',  {method:'POST'}).then(r => r.text()); }
function apiGetInitialSetup() { return fetch('/initialSetup',{method:'POST'}).then(r => r.text()); }
function apiGetBrightness()   { return fetch('/brightness',  {method:'POST'}).then(r => r.json()); }
function apiGetAcFreq()       { return fetch('/acfreq').then(r => r.json()); }

// Hardware test
function apiSetDuty(value)       { return _postForm('/dutty',       'value=' + value); }
function apiSetDutyCW(value)     { return _postForm('/duttyCW',     'value=' + value); }
function apiSetDutyMosfet(value) { return _postForm('/duttyMosfet', 'value=' + value); }
function apiSetColor(r, g, b)    { return _postForm('/color',       'r=' + r + '&g=' + g + '&b=' + b); }

// WiFi
function apiGetWifiStatus() { return fetch('/wifiStatus', {method:'POST'}).then(r => r.json()); }
function apiScanWifi()      { return fetch('/scan',       {method:'POST'}).then(r => r.json()); }
function apiSaveWifi(ssid, pass) {
  return _postForm('/wifi', 'sta_ssid=' + encodeURIComponent(ssid) + '&sta_pass=' + encodeURIComponent(pass));
}
function apiClearWifi() { return fetch('/clearwifi', {method:'POST'}); }

// MQTT
function apiGetMqttStatus() { return fetch('/mqttStatus', {method:'POST'}).then(r => r.json()); }
function apiSaveMqtt(host, port, client, topic, fulltopic, user, pass) {
  return _postForm('/mqtt',
    'mqtt_host='       + encodeURIComponent(host)      +
    '&mqtt_port='      + encodeURIComponent(port)      +
    '&mqtt_client='    + encodeURIComponent(client)    +
    '&mqtt_topic='     + encodeURIComponent(topic)     +
    '&mqtt_fulltopic=' + encodeURIComponent(fulltopic) +
    '&mqtt_user='      + encodeURIComponent(user)      +
    '&mqtt_pass='      + encodeURIComponent(pass)
  );
}
function apiClearMqtt() { return fetch('/clearmqtt', {method:'POST'}); }

// GPIO / Hardware configuration
function apiGetGpioCaps()       { return fetch('/gpiocaps').then(r => r.json()); }
function apiGetFunclist()       { return fetch('/funclist').then(r => r.json()); }
function apiGetTemplates()      { return fetch('/templates').then(r => r.json()); }
function apiGetGpioMap()        { return fetch('/gpiomap').then(r => r.json()); }
function apiSaveGpioMap(params) { return _postForm('/gpiomap', params.toString()); }

// Estat en arrencada
function apiGetPowerupMode()       { return fetch('/powerupmode').then(r => r.text()); }
function apiSavePowerupMode(mode)  { return _postForm('/powerupmode', 'mode=' + mode); }

// Nom dispositiu
function apiGetDeviceName()      { return fetch('/devicename').then(r => r.text()); }
function apiSaveDeviceName(name) { return _postForm('/devicename', 'device_name=' + encodeURIComponent(name)); }
function apiClearDeviceName()    { return fetch('/cleardevicename', {method:'POST'}).then(r => r.text()); }

// Seguretat BlauProtocol v2
function apiGetSecurityStatus() { return fetch('/securityStatus').then(r => r.json()); }
function apiSaveSecurity(pass)  { return _postForm('/security', 'protopass=' + encodeURIComponent(pass)); }
function apiClearSecurity()     { return fetch('/clearsecurity', {method:'POST'}); }
function apiStartLearning()     { return fetch('/startlearning', {method:'POST'}); }

// Administració
function apiRestart()       { return fetch('/restart',       {method:'POST'}); }
function apiClearConfig()   { return fetch('/clearconfig',   {method:'POST'}); }
function apiClearHardware() { return fetch('/clearhardware', {method:'POST'}); }

// Canals
function apiGetChannels()        { return fetch('/channels').then(r => r.json()); }
function apiSetChannel(ch, body) { return _postForm('/channel', 'ch=' + ch + '&' + body); }
