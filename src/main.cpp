// BlauLux — firmware per controlar càrregues AC (bombeta, tira PWM, CW/WW, RGB, relé, triac)
// Plataformes suportades: ESP32-C3 / ESP32-S3

#include "globals.h"
#include "nvsconfig.h"
#include "output.h"
#include "mqtt.h"
#include "espnow.h"
#include "webserver.h"
#include "button.h"
#include "watchdog.h"
#include <esp_wifi.h>
#include <esp_now.h>

// ════════════════════════════════════════════════════════════════
//  DEFINICIONS DE VARIABLES GLOBALS (font de veritat única)
// ════════════════════════════════════════════════════════════════
Gpio   gpioMap[MAX_GPIO_COUNT];
String device_name = WIFI_SSID;

AsyncWebServer server(HTTP_PORT);
DNSServer      dnsServer;
String macAP, macAPSuffix;

bool webTesting = false;

String sta_ssid, sta_pass;
String mqtt_host, mqtt_user, mqtt_pass;
String mqtt_client = HC_MQTT_CLIENT, mqtt_topic = HC_MQTT_TOPIC, mqtt_fulltopic = HC_MQTT_FULLTOPIC;
uint16_t mqtt_port = 1883;
String mqttClientId, mqttWillTopic;
AsyncMqttClient mqttClient;
TimerHandle_t   mqttReconnectTimer = nullptr;
Preferences     prefs;

ButtonState btn;

// ════════════════════════════════════════════════════════════════
//  WiFi / Access Point
// ════════════════════════════════════════════════════════════════
static void getMyMacAddress() {
  macAP = WiFi.softAPmacAddress();
  macAP.replace(":", "");
  macAPSuffix = macAP.substring(macAP.length() - 4);
  LOG_D("[WIFI] MAC AP: %s", macAP.c_str());
}

static void configDeviceAP() {
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  getMyMacAddress();
  String apSsid = device_name + "_" + macAPSuffix;
  bool apOk = WiFi.softAP(apSsid.c_str(), nullptr, ESPNOW_CHANNEL);
  if (!apOk) { LOG_E("[WIFI] AP Config failed"); }
  else { LOG_I("[WIFI] AP ok: %s ch=%d MAC=%s", apSsid.c_str(), WiFi.channel(), WiFi.softAPmacAddress().c_str()); }
}

static void wifiApModeServer() {
  if (!LittleFS.begin()) { LOG_E("[FS] Error muntant LittleFS"); return; }
  LOG_I("[WIFI] AP IP: %s", WiFi.softAPIP().toString().c_str());
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  webServerSetup();
}

// ════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);
  wdtSetup();
  logResetReason();

  #ifdef CLEAR_CONFIG
    clearConfig();
  #endif
  loadConfig();
  driverSetupAll();
  applyPowerupState();
  renderVisualFeedback("inici");

  // delay(2000);

  loadSecurityConfig();   // BlauProtocol v2: clau AES + whitelist + nonces ("blau_rx")

  if (getBotonPin() == PIN_UNUSED) {
    bool wifiOk = false;
    #ifdef ENABLE_WIFI_STA
    if (sta_ssid.length() > 0) {
      WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
      LOG_I("[CFG] Boto no configurat, provant WiFi: %s", sta_ssid.c_str());
      unsigned long t = millis();
      while (!WiFi.isConnected() && millis() - t < WIFI_STA_TIMEOUT_MS)
        { wdtReset(); delay(500); }
      wifiOk = WiFi.isConnected();
    }
    #endif
    if (!wifiOk) {
      LOG_I("[CFG] Boto no configurat%s, mode AP inicial",
            sta_ssid.length() > 0 ? ", WiFi no disponible" : "");
      configDeviceAP();
      wifiApModeServer();
      unsigned long apStart = millis();
      while (getBotonPin() == PIN_UNUSED) {
        wdtReset();
        dnsServer.processNextRequest();
        if (getPin1() != PIN_UNUSED && getControlType() >= 0) renderVisualFeedback("wifiAP");
        delay(DNS_POLL_MS);
        // if (millis() - apStart > WIFI_AP_TIMEOUT_MS) {
        //   LOG_I("[AP] Temps excedit en setup inicial");
        //   ESP.restart();
        // }
      }
      delay(2000);
      ESP.restart();
    }
    LOG_I("[CFG] WiFi ok, operacio normal (sense boto)");
  }

  
  configDeviceAP();

  if (!LittleFS.begin()) { LOG_E("[FS] Error muntant LittleFS"); }
  else { LOG_I("[WIFI] AP IP: %s", WiFi.softAPIP().toString().c_str()); webServerSetup(); }

  initEspNow();
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

#ifdef ENABLE_WIFI_STA
  if (sta_ssid.length() > 0 && !WiFi.isConnected()) {
    WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
    LOG_I("[WIFI] STA connecting to: %s", sta_ssid.c_str());
  }
#endif

#ifdef ENABLE_MQTT
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)0,
                        [](TimerHandle_t){ LOG_I("[MQTT] timer -> reconnect"); connectMqtt(); });
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  if (mqtt_host.length() > 0) {
    mqttClientId  = mqttResolvedClientId();
    mqttWillTopic = mqttBaseTopic() + "/available";
    mqttClient.setClientId(mqttClientId.c_str());
    mqttClient.setServer(mqtt_host.c_str(), mqtt_port);
    if (mqtt_user.length() > 0) mqttClient.setCredentials(mqtt_user.c_str(), mqtt_pass.c_str());
    mqttClient.setWill(mqttWillTopic.c_str(), 1, true, "offline");
    LOG_I("[MQTT] configured: broker=%s:%d user=%s",
      mqtt_host.c_str(), mqtt_port, mqtt_user.length() > 0 ? mqtt_user.c_str() : "(none)");
  }
#endif
}

// ════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ════════════════════════════════════════════════════════════════
void loop() {
  wdtReset();
  processEspNowPending();

#if defined(ENABLE_WIFI_STA) && defined(ENABLE_MQTT)
  {
    static bool lastWifiConnected = false;
    bool wifiNow = WiFi.isConnected();
    if (wifiNow && !lastWifiConnected) {
      LOG_I("[WIFI] STA connectat, IP: %s, canal: %d (ESP-NOW segueix aquest canal)",
        WiFi.localIP().toString().c_str(), WiFi.channel());
      if (mqtt_host.length() > 0 && !mqttClient.connected()) connectMqtt();
    }
    if (!wifiNow && lastWifiConnected) {
      LOG_I("[WIFI] STA disconnected");
      if (mqttReconnectTimer != nullptr) xTimerStop(mqttReconnectTimer, 0);
    }
    lastWifiConnected = wifiNow;
  }
#endif

#ifdef ENABLE_MQTT
  {
    static bool lastState = false;
    if (lastState != getState()) { lastState = getState(); publishState(); }
  }
#endif

  // ── Mode AP actiu ───────────────────────────────────────────────
  if (btn.apActive) {
    uint32_t now = millis();
    if (now - btn.lastDnsPoll >= DNS_POLL_MS) {
      dnsServer.processNextRequest();
      btn.lastDnsPoll = now;
    }
    if (!webTesting) renderVisualFeedback("wifiAP");
    if (now - btn.apStart > WIFI_AP_TIMEOUT_MS) { LOG_I("[AP] Temps excedit"); ESP.restart(); }
    bool pressed = buttonPressed();
    if (!pressed && btn.apLastBtn && !btn.apBtnReleased) {
      btn.apBtnReleased   = true;
      btn.apBtnReleasedAt = millis();
      LOG_I("[BTN] Boto alliberat (mode AP)");
    }
    if (pressed && !btn.apLastBtn && btn.apBtnReleased &&
        millis() - btn.apBtnReleasedAt >= BUTTON_RELEASE_DEBOUNCE_MS) {
      LOG_I("[BTN] Boto premut despres d'alliberar -> reiniciant");
      ESP.restart();
    }
    btn.apLastBtn = pressed;
    return;
  }

  // ── Debounce post-clic ──────────────────────────────────────────
  if (btn.debouncing) {
    if (millis() - btn.upTime >= BUTTON_DEBOUNCE_MS) btn.debouncing = false;
    return;
  }

  // ── Detecció del botó ───────────────────────────────────────────
  bool pressed = buttonPressed();

  if (pressed && !btn.down) {
    btn.down = true; btn.downTime = millis();
    LOG_I("[BTN] Boto presionat");
  }

  if (pressed && btn.down && millis() - btn.downTime >= WIFI_AP_HOLD_MS) {
    LOG_I("[BTN] Hold -> mode AP (captiu)");
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    renderVisualFeedback("wifiAP");
    btn.apActive = true; btn.apStart = millis(); btn.apBtnReleased = false;
    btn.apLastBtn = true; btn.down = false; webTesting = false;
    return;
  }

  if (!pressed && btn.down) {
    if (millis() - btn.downTime < WIFI_AP_HOLD_MS) {
      LOG_I("[BTN] Click curt -> toggle");
      driverToggleAll();
    }
    btn.down = false; btn.debouncing = true; btn.upTime = millis();
  }
}
