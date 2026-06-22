#include "webserver.h"
#include "output.h"
#include "nvsconfig.h"
#include "mqtt.h"
#include "espnow.h"
#include <Update.h>

void serveixWifiManager(AsyncWebServerRequest *request) {
  request->send(LittleFS, "/wifimanager.html", "text/html");
}

void stopWebServer() {
  server.reset();
  dnsServer.stop();
  LittleFS.end();
  LOG_I("[WEB] Servidor i LittleFS aturats");
}

void webServerSetup() {
  // Pàgina principal
  server.on("/", HTTP_GET, serveixWifiManager);

  // Portal captiu — Windows 11
  server.on("/connecttest.txt", [](AsyncWebServerRequest *r) { r->redirect("http://logout.net"); });
  server.on("/wpad.dat",        [](AsyncWebServerRequest *r) { r->send(404); });

  // Portal captiu — Android / Firefox / Windows addicionals
  server.on("/generate_204",                HTTP_GET, serveixWifiManager);
  server.on("/ncsi.txt",                    HTTP_GET, serveixWifiManager);
  server.on("/hotspot-detect.html",         HTTP_GET, serveixWifiManager);
  server.on("/library/test/success.html",   HTTP_GET, serveixWifiManager);
  server.on("/redirect",                    HTTP_GET, serveixWifiManager);
  server.on("/fwlink",                      HTTP_GET, serveixWifiManager);
  server.on("/cdn-cgi/",                    HTTP_GET, serveixWifiManager);
  server.on("/canonical.html",              HTTP_GET, serveixWifiManager);
  server.on("/success.txt", [](AsyncWebServerRequest *r) { r->send(200); });
  server.on("/favicon.ico", [](AsyncWebServerRequest *r) { r->send(404); });

  // Full d'estils
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(LittleFS, "/style.css", "text/css");
  });

  // Scripts JS externs
  server.serveStatic("/js/", LittleFS, "/js/");

  // ── Endpoints d'estat (GET/POST) ─────────────────────────────

  server.on("/driverMode", HTTP_POST, [](AsyncWebServerRequest *r) {
    char buf[96];
    snprintf(buf, sizeof(buf),
             "{\"ct\":%d,\"type\":\"%s\",\"hasBr\":%s,\"hasCol\":%s,\"hasCCT\":%s}",
             getControlType(),
             g_driver ? g_driver->typeName       : "none",
             (g_driver && g_driver->hasBrightness) ? "true" : "false",
             (g_driver && g_driver->hasColor)       ? "true" : "false",
             (g_driver && g_driver->hasCCT)         ? "true" : "false");
    r->send(200, "application/json", buf);
  });

  server.on("/configMode", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", "web");
  });

  server.on("/mymac", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", WiFi.softAPmacAddress());
  });

  server.on("/pins", HTTP_POST, [](AsyncWebServerRequest *r) {
    String sPin1 = (getPin1() == PIN_UNUSED) ? "null" : String(getPin1());
    String sPin2 = (getPin2() == PIN_UNUSED) ? "null" : String(getPin2());
    String sPin3 = (getPin3() == PIN_UNUSED) ? "null" : String(getPin3());
    r->send(200, "application/json",
      "{\"pin1\":" + sPin1 + ",\"pin2\":" + sPin2 + ",\"pin3\":" + sPin3 + "}");
  });

  server.on("/brightness", HTTP_POST, [](AsyncWebServerRequest *r) {
    String json = "{\"b1\":" + String(getBrightnessForType(1))
                + ",\"b2\":" + String(getBrightnessForType(2))
                + ",\"b3\":" + String(getBrightnessForType(3))
                + ",\"bcw\":" + String(getBrightnessCW()) + "}";
    r->send(200, "application/json", json);
  });

  server.on("/boto", HTTP_POST, [](AsyncWebServerRequest *r) {
    String b = (getBotonPin() == PIN_UNUSED) ? "null" : String(getBotonPin());
    r->send(200, "application/json",
      "{\"boto\":" + b + ",\"bpu\":" + String(getButtonPullup() ? 1 : 0) + "}");
  });

  server.on("/initialSetup", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", getBotonPin() == PIN_UNUSED ? "true" : "false");
  });

  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", FIRMWARE_VERSION);
  });

  server.on("/chipinfo", HTTP_GET, [](AsyncWebServerRequest *r) {
    prefs.begin("blau", true);
    String fwFile = prefs.getString("fw_file", "");
    prefs.end();
    String mac = WiFi.macAddress();
    String json = "{";
    json += "\"fw_ver\":\""   + String(FIRMWARE_VERSION)            + "\",";
    json += "\"fw_file\":\""  + fwFile                              + "\",";
    json += "\"chip\":\""     + String(ESP.getChipModel())          + "\",";
    json += "\"chip_rev\":"   + String(ESP.getChipRevision())       + ",";
    json += "\"cores\":"      + String(ESP.getChipCores())          + ",";
    json += "\"cpu_mhz\":"    + String(ESP.getCpuFreqMHz())         + ",";
    json += "\"idf_ver\":\""  + String(ESP.getSdkVersion())         + "\",";
    json += "\"heap_free\":"  + String(ESP.getFreeHeap())           + ",";
    json += "\"heap_total\":" + String(ESP.getHeapSize())           + ",";
    json += "\"psram_size\":" + String(ESP.getPsramSize())          + ",";
    json += "\"psram_free\":" + String(ESP.getFreePsram())          + ",";
    json += "\"flash_size\":" + String(ESP.getFlashChipSize())      + ",";
    json += "\"flash_mhz\":"  + String(ESP.getFlashChipSpeed()/1000000) + ",";
    json += "\"sketch_size\":"+ String(ESP.getSketchSize())         + ",";
    json += "\"sketch_free\":"+ String(ESP.getFreeSketchSpace())    + ",";
    json += "\"fs_used\":"    + String(LittleFS.usedBytes())        + ",";
    json += "\"fs_total\":"   + String(LittleFS.totalBytes())       + ",";
    json += "\"mac\":\""      + mac                                 + "\"";
    json += "}";
    r->send(200, "application/json", json);
  });

  server.on("/acfreq", HTTP_GET, [](AsyncWebServerRequest *r) {
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"freq\":%.1f}", getAcFreqHz());
    r->send(200, "application/json", buf);
  });

  // ── Endpoints d'escriptura (POST) ────────────────────────────

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *r) {
    int newBrightness = -1;
    int params = r->params();
    for (int i = 0; i < params; i++) {
      const AsyncWebParameter* p = r->getParam(i);
      if (p->isPost()) {
        if      (p->name() == "brightness")    newBrightness = p->value().toInt();
        else if (p->name() == "brightness_cw") setBrightnessCW(p->value().toInt());
      }
    }
    if (newBrightness >= 0 && g_driver && g_driver->hasBrightness)
      setBrightnessForType(getControlType(), newBrightness);
    saveConfig();
    r->send(200, "text/plain", "OK");
  });

  server.on("/duttyMosfet", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("value", true)) {
      webTesting = true;
      int duty = constrain(r->getParam("value", true)->value().toInt(), 0, 100);
      applyMosfetDuty(duty);
    }
    r->send(200, "text/plain", "OK");
  });

  server.on("/dutty", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("value", true)) {
      webTesting = true;
      int duty = r->getParam("value", true)->value().toInt();
      if (g_driver && g_driver->hasBrightness) setBrightnessForType(getControlType(), duty);
      applyOutput(duty > 0);
    }
    r->send(200, "text/plain", "OK");
  });

  server.on("/duttyCW", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("value", true)) {
      webTesting = true;
      setBrightnessCW(r->getParam("value", true)->value().toInt());
      if (getState()) applyOutput(true);
    }
    r->send(200, "text/plain", "OK");
  });

  // ── Canals / estats en temps real ────────────────────────────

  server.on("/channels", HTTP_GET, [](AsyncWebServerRequest *r) {
    String json = "{\"channels\":[";
    bool first = true;
    for (int i = 0; i < MAX_GPIO_COUNT; i++) {
      GpioFunc f = gpioMap[i].cfg.func;
      if (f == FUNC_NONE || f == FUNC_BTN || f == FUNC_BTN_INV || f == FUNC_ZCD) continue;

      const char* type   = "onoff";
      bool hasBr         = false;
      bool hasColor      = false;
      bool hasFreq       = false;
      uint32_t br        = 0;
      uint32_t colorVal  = 0xFFFFFF;
      uint32_t freq      = 0;

      switch (f) {
        case FUNC_PWM:
          type = "pwm"; hasBr = true; hasFreq = true;
          br = gpioMap[i].rt.param1;
          freq = gpioMap[i].rt.param2 ? gpioMap[i].rt.param2 : PWM_FREQ;
          break;
        case FUNC_DIGITAL_LED:
          type = "neopixel"; hasBr = true; hasColor = true;
          br = gpioMap[i].rt.param2; colorVal = gpioMap[i].rt.param1; break;
        case FUNC_TRIAC_CYCLE:
          type = "triac_cycle"; hasBr = true; hasFreq = true;
          br = gpioMap[i].rt.param1;
          freq = gpioMap[i].rt.param2 ? gpioMap[i].rt.param2 : 10;
          break;
        case FUNC_TRIAC_FASE:
          type = "triac_phase"; hasBr = true; br = gpioMap[i].rt.param1; break;
        default: break;
      }

      char colorHex[7];
      snprintf(colorHex, sizeof(colorHex), "%06lx", (unsigned long)colorVal);

      if (!first) json += ",";
      first = false;
      json += "{\"id\":"    + String(i)
            + ",\"name\":\"" + String(gpioMap[i].cfg.name) + "\""
            + ",\"type\":\"" + String(type) + "\""
            + ",\"on\":"    + String(gpioMap[i].rt.state ? "true" : "false")
            + ",\"br\":"    + String(br)
            + ",\"freq\":"  + String(freq)
            + ",\"color\":\"" + String(colorHex) + "\""
            + ",\"hasBr\":" + String(hasBr    ? "true" : "false")
            + ",\"hasFreq\":" + String(hasFreq  ? "true" : "false")
            + ",\"hasColor\":" + String(hasColor ? "true" : "false")
            + "}";
    }
    json += "],\"mosfet\":-1}";
    r->send(200, "application/json", json);
  });

  server.on("/channel", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!r->hasParam("ch", true)) { r->send(400, "text/plain", "missing ch"); return; }
    int ch = r->getParam("ch", true)->value().toInt();
    if (ch < 0 || ch > 21) { r->send(400, "text/plain", "bad ch"); return; }

    GpioFunc f = gpioMap[ch].cfg.func;
    bool isOutput = (f == FUNC_ON_OFF || f == FUNC_PWM || f == FUNC_DIGITAL_LED ||
                     f == FUNC_TRIAC_CYCLE || f == FUNC_TRIAC_FASE);
    if (!isOutput) { r->send(400, "text/plain", "not output"); return; }

    if (r->hasParam("on", true)) {
      gpioMap[ch].rt.state        = r->getParam("on", true)->value() == "1";
      gpioMap[ch].rt.lastChangeMs = (uint32_t)millis();
      driverApply(ch);
    }
    if (r->hasParam("br", true)) {
      int br = constrain(r->getParam("br", true)->value().toInt(), 0, 100);
      if (f == FUNC_DIGITAL_LED) gpioMap[ch].rt.param2 = (uint16_t)br;
      else                       gpioMap[ch].rt.param1 = (uint32_t)br;
      driverApply(ch);
    }
    if (r->hasParam("freq", true)) {
      int fv = r->getParam("freq", true)->value().toInt();
      if (f == FUNC_PWM)         gpioMap[ch].rt.param2 = (uint16_t)constrain(fv, 300, 40000);
      if (f == FUNC_TRIAC_CYCLE) gpioMap[ch].rt.param2 = (uint16_t)constrain(fv, 5, 50);
      driverApply(ch);
    }
    if (r->hasParam("r", true) && f == FUNC_DIGITAL_LED) {
      int rv = constrain(r->getParam("r", true)->value().toInt(), 0, 255);
      int gv = r->hasParam("g", true) ? constrain(r->getParam("g", true)->value().toInt(), 0, 255) : 0;
      int bv = r->hasParam("b", true) ? constrain(r->getParam("b", true)->value().toInt(), 0, 255) : 0;
      gpioMap[ch].rt.param1 = ((uint32_t)rv << 16) | ((uint32_t)gv << 8) | (uint32_t)bv;
      driverApply(ch);
    }
    r->send(200, "text/plain", "OK");
  });

  // ── WiFi ─────────────────────────────────────────────────────

  server.on("/wifiStatus", HTTP_POST, [](AsyncWebServerRequest *r) {
    bool connected = WiFi.status() == WL_CONNECTED;
    String ip   = connected ? WiFi.localIP().toString() : "";
    int    rssi = connected ? WiFi.RSSI() : 0;
    String json = "{\"connected\":" + String(connected ? "true" : "false")
                + ",\"ip\":\"" + ip + "\""
                + ",\"ssid\":\"" + sta_ssid + "\""
                + ",\"pass\":\"" + sta_pass + "\""
                + ",\"rssi\":" + String(rssi) + "}";
    r->send(200, "application/json", json);
  });

  server.on("/scan", HTTP_POST, [](AsyncWebServerRequest *r) {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i))
            + ",\"enc\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1 : 0) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    r->send(200, "application/json", json);
  });

  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("sta_ssid", true)) {
      sta_ssid = r->getParam("sta_ssid", true)->value();
      sta_pass = r->hasParam("sta_pass", true) ? r->getParam("sta_pass", true)->value() : "";
      prefs.begin("blau", false);
      prefs.putString("sta_ssid", sta_ssid);
      prefs.putString("sta_pass", sta_pass);
      prefs.end();
      LOG_I("[WIFI] STA credentials saved: %s", sta_ssid.c_str());
    }
    r->send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
  });

  server.on("/clearwifi", HTTP_POST, [](AsyncWebServerRequest *r) {
    sta_ssid = ""; sta_pass = "";
    prefs.begin("blau", false);
    prefs.remove("sta_ssid"); prefs.remove("sta_pass");
    prefs.end();
    LOG_I("[WIFI] Credencials WiFi esborrades");
    r->send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
  });

  // ── MQTT ─────────────────────────────────────────────────────

  server.on("/mqttStatus", HTTP_POST, [](AsyncWebServerRequest *r) {
    bool mqConnected = mqttClient.connected();
    String json = "{\"connected\":" + String(mqConnected ? "true" : "false")
                + ",\"broker\":\"" + mqtt_host + "\""
                + ",\"port\":" + String(mqtt_port)
                + ",\"user\":\"" + mqtt_user + "\""
                + ",\"pass\":\"" + mqtt_pass + "\""
                + ",\"client\":\"" + mqtt_client + "\""
                + ",\"mqtt_topic\":\"" + mqtt_topic + "\""
                + ",\"fulltopic\":\"" + mqtt_fulltopic + "\""
                + ",\"topic\":\"" + (mqConnected ? mqttBaseTopic() : "") + "\"}";
    r->send(200, "application/json", json);
  });

  server.on("/mqtt", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("mqtt_host", true)) {
      mqtt_host      = r->getParam("mqtt_host", true)->value();
      mqtt_port      = r->hasParam("mqtt_port", true) ?
                       (uint16_t)r->getParam("mqtt_port", true)->value().toInt() : 1883;
      mqtt_user      = r->hasParam("mqtt_user",      true) ? r->getParam("mqtt_user",      true)->value() : "";
      mqtt_pass      = r->hasParam("mqtt_pass",      true) ? r->getParam("mqtt_pass",      true)->value() : "";
      mqtt_client    = r->hasParam("mqtt_client",    true) ? r->getParam("mqtt_client",    true)->value() : HC_MQTT_CLIENT;
      mqtt_topic     = r->hasParam("mqtt_topic",     true) ? r->getParam("mqtt_topic",     true)->value() : HC_MQTT_TOPIC;
      mqtt_fulltopic = r->hasParam("mqtt_fulltopic", true) ? r->getParam("mqtt_fulltopic", true)->value() : HC_MQTT_FULLTOPIC;
      if (mqtt_client.length()    == 0) mqtt_client    = HC_MQTT_CLIENT;
      if (mqtt_topic.length()     == 0) mqtt_topic     = HC_MQTT_TOPIC;
      if (mqtt_fulltopic.length() == 0) mqtt_fulltopic = HC_MQTT_FULLTOPIC;
      prefs.begin("blau", false);
      prefs.putString("mqtt_host",      mqtt_host);
      prefs.putInt("mqtt_port",         (int)mqtt_port);
      prefs.putString("mqtt_user",      mqtt_user);
      prefs.putString("mqtt_pass",      mqtt_pass);
      prefs.putString("mqtt_client",    mqtt_client);
      prefs.putString("mqtt_topic",     mqtt_topic);
      prefs.putString("mqtt_fulltopic", mqtt_fulltopic);
      prefs.end();
      LOG_I("[MQTT] config saved: %s:%d client='%s' topic='%s' fulltopic='%s'",
        mqtt_host.c_str(), mqtt_port, mqtt_client.c_str(), mqtt_topic.c_str(), mqtt_fulltopic.c_str());
      mqttClientId  = mqttResolvedClientId();
      mqttWillTopic = mqttBaseTopic() + "/available";
      mqttClient.setClientId(mqttClientId.c_str());
      mqttClient.setServer(mqtt_host.c_str(), mqtt_port);
      if (mqtt_user.length() > 0) mqttClient.setCredentials(mqtt_user.c_str(), mqtt_pass.c_str());
      else mqttClient.setCredentials("", "");
      mqttClient.setWill(mqttWillTopic.c_str(), 1, true, "offline");
      if (mqttClient.connected()) mqttClient.disconnect();
      else if (WiFi.isConnected() && mqtt_host.length() > 0 && mqttReconnectTimer != nullptr) connectMqtt();
    }
    r->send(200, "text/plain", "OK");
  });

  server.on("/clearmqtt", HTTP_POST, [](AsyncWebServerRequest *r) {
    mqtt_host = ""; mqtt_port = 1883; mqtt_user = ""; mqtt_pass = "";
    mqtt_client = HC_MQTT_CLIENT; mqtt_topic = HC_MQTT_TOPIC; mqtt_fulltopic = HC_MQTT_FULLTOPIC;
    prefs.begin("blau", false);
    prefs.remove("mqtt_host"); prefs.remove("mqtt_port"); prefs.remove("mqtt_user");
    prefs.remove("mqtt_pass"); prefs.remove("mqtt_client"); prefs.remove("mqtt_topic");
    prefs.remove("mqtt_fulltopic");
    prefs.end();
    if (mqttClient.connected()) mqttClient.disconnect();
    LOG_I("[MQTT] Configuració MQTT esborrada");
    r->send(200, "text/plain", "OK");
  });

  // ── Configuració hardware ─────────────────────────────────────

  server.on("/clearhardware", HTTP_POST, [](AsyncWebServerRequest *r) {
    prefs.begin("blau", false);
    char key[4];
    for (int i = 0; i < MAX_GPIO_COUNT; i++) {
      snprintf(key, sizeof(key), "f%d", i); prefs.remove(key);
      snprintf(key, sizeof(key), "a%d", i); prefs.remove(key);
      snprintf(key, sizeof(key), "b%d", i); prefs.remove(key);
      snprintf(key, sizeof(key), "c%d", i); prefs.remove(key);
      snprintf(key, sizeof(key), "n%d", i); prefs.remove(key);
      snprintf(key, sizeof(key), "e%d", i); prefs.remove(key);
      // snprintf(key, sizeof(key), "g%d", i); prefs.remove(key);  // clau antiga
    }
    // prefs.remove("b1"); prefs.remove("b2"); prefs.remove("b3");
    // prefs.remove("bcw"); prefs.remove("pf"); prefs.remove("pd");
    prefs.putUChar("schema", CONFIG_SCHEMA_VERSION);
    prefs.end();
    LOG_I("[CFG] Configuració hardware esborrada");
    r->send(200, "text/plain", "OK");
    delay(500); ESP.restart();
  });

  server.on("/gpiomap", HTTP_GET, [](AsyncWebServerRequest *r) {
    String json = "{";
    char key[4];
    for (int i = 0; i < MAX_GPIO_COUNT; i++) {
      if (i > 0) json += ",";
      snprintf(key, sizeof(key), "f%d", i);
      json += "\"" + String(key) + "\":" + String((int)gpioMap[i].cfg.func);
      snprintf(key, sizeof(key), "a%d", i);
      json += ",\"" + String(key) + "\":" + String(gpioMap[i].cfg.param1);
      snprintf(key, sizeof(key), "b%d", i);
      json += ",\"" + String(key) + "\":" + String(gpioMap[i].cfg.param2);
      snprintf(key, sizeof(key), "c%d", i);
      json += ",\"" + String(key) + "\":" + String(gpioMap[i].cfg.param3);
      snprintf(key, sizeof(key), "e%d", i);
      json += ",\"" + String(key) + "\":" + (gpioMap[i].cfg.notificador ? "1" : "0");
      if (gpioMap[i].cfg.name[0]) {
        snprintf(key, sizeof(key), "n%d", i);
        json += ",\"" + String(key) + "\":\"" + String(gpioMap[i].cfg.name) + "\"";
      }
    }
    json += "}";
    r->send(200, "application/json", json);
  });

  server.on("/gpiomap", HTTP_POST, [](AsyncWebServerRequest *r) {
    char key[4];
    for (int i = 0; i < MAX_GPIO_COUNT; i++) {
      snprintf(key, sizeof(key), "f%d", i);
      if (r->hasParam(key, true))
        gpioMap[i].cfg.func = (GpioFunc)r->getParam(key, true)->value().toInt();
      snprintf(key, sizeof(key), "a%d", i);
      if (r->hasParam(key, true))
        gpioMap[i].cfg.param1 = (uint32_t)r->getParam(key, true)->value().toInt();
      snprintf(key, sizeof(key), "b%d", i);
      if (r->hasParam(key, true))
        gpioMap[i].cfg.param2 = (uint16_t)r->getParam(key, true)->value().toInt();
      snprintf(key, sizeof(key), "c%d", i);
      if (r->hasParam(key, true))
        gpioMap[i].cfg.param3 = (uint16_t)r->getParam(key, true)->value().toInt();
      snprintf(key, sizeof(key), "n%d", i);
      if (r->hasParam(key, true)) {
        String nm = r->getParam(key, true)->value();
        strncpy(gpioMap[i].cfg.name, nm.c_str(), 12);
        gpioMap[i].cfg.name[12] = '\0';
      }
      snprintf(key, sizeof(key), "e%d", i);
      if (r->hasParam(key, true))
        gpioMap[i].cfg.notificador = r->getParam(key, true)->value().toInt() == 1;
    }
    if (r->hasParam("tmpl", true))
      selectedTemplate = (int8_t)r->getParam("tmpl", true)->value().toInt();
    saveConfig();
    driverSetupAll();
    LOG_I("[CFG] gpioMap actualitzat via /gpiomap");
    r->send(200, "text/plain", "OK");
  });

  server.on("/funclist", HTTP_GET, [](AsyncWebServerRequest *r) {
    String json = "[";
    for (int i = 0; i < (int)FUNC_COUNT; i++) {
      if (i > 0) json += ",";
      json += "{\"id\":\"" + String(FUNC_REGISTRY[i].id) + "\""
            + ",\"label\":\"" + String(FUNC_REGISTRY[i].label) + "\""
            + ",\"isInput\":" + (FUNC_REGISTRY[i].isInput ? "true" : "false") + "}";
    }
    json += "]";
    r->send(200, "application/json", json);
  });

  server.on("/templates", HTTP_GET, [](AsyncWebServerRequest *r) {
    int numTemplates = sizeof(DEVICE_TEMPLATES) / sizeof(DEVICE_TEMPLATES[0]);
    String json = "[";
    for (int t = 0; t < numTemplates; t++) {
      if (t > 0) json += ",";
      json += "{\"name\":\"" + String(DEVICE_TEMPLATES[t].name) + "\",\"pins\":[";
      for (int p = 0; p < DEVICE_TEMPLATES[t].count; p++) {
        if (p > 0) json += ",";
        json += "{\"gpio\":" + String((int)DEVICE_TEMPLATES[t].pins[p].gpio)
              + ",\"func\":" + String((int)DEVICE_TEMPLATES[t].pins[p].func) + "}";
      }
      json += "],\"select\":" + String(t == selectedTemplate ? "true" : "false") + "}";
    }
    json += "]";
    Serial.println(json);
    r->send(200, "application/json", json);
  });

  server.on("/gpiocaps", HTTP_GET, [](AsyncWebServerRequest *r) {
    auto capsJson = [](const GpioCaps caps[], int n) {
      String s = "[";
      for (int i = 0; i < n; i++) {
        if (i > 0) s += ",";
        s += "{\"valid\":"     + String(caps[i].valid     ? "true" : "false")
           + ",\"hasPwm\":"    + String(caps[i].hasPwm    ? "true" : "false")
           + ",\"hasAdc\":"    + String(caps[i].hasAdc    ? "true" : "false")
           + ",\"inputOnly\":" + String(caps[i].inputOnly ? "true" : "false") + "}";
      }
      return s + "]";
    };
    String json = "{"
      "\"esp32\":{\"name\":\"ESP32\",\"caps\":"      + capsJson(ESP32_GPIO_CAPS,   40) + "},"
      "\"esp32c3\":{\"name\":\"ESP32-C3\",\"caps\":" + capsJson(ESP32C3_GPIO_CAPS, 22) + "},"
      "\"esp32s3\":{\"name\":\"ESP32-S3\",\"caps\":" + capsJson(ESP32S3_GPIO_CAPS, 22) + "}"
      "}";
    r->send(200, "application/json", json);
  });

  // ── Nom del dispositiu ────────────────────────────────────────

  server.on("/devicename", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", device_name);
  });

  server.on("/devicename", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("device_name", true)) {
      String name = r->getParam("device_name", true)->value();
      name.trim();
      if (name.length() > 0 && name.length() <= 32) {
        device_name = name;
        prefs.begin("blau", false);
        prefs.putString("devname", device_name);
        prefs.end();
        LOG_I("[CFG] device_name: %s", device_name.c_str());
      }
    }
    r->send(200, "text/plain", "OK");
  });

  server.on("/cleardevicename", HTTP_POST, [](AsyncWebServerRequest *r) {
    device_name = WIFI_SSID;
    prefs.begin("blau", false);
    prefs.remove("devname");
    prefs.end();
    LOG_I("[CFG] Nom del dispositiu esborrat (default: %s)", device_name.c_str());
    r->send(200, "text/plain", device_name);
  });

  // ── Estat en arrencada ────────────────────────────────────────

  server.on("/powerupmode", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", String(powerupMode));
  });

  server.on("/powerupmode", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("mode", true)) {
      int m = r->getParam("mode", true)->value().toInt();
      if (m >= 0 && m <= 2) {
        powerupMode = (uint8_t)m;
        savePowerupMode(powerupMode);
        LOG_I("[CFG] powerupMode: %d", powerupMode);
      }
    }
    r->send(200, "text/plain", "OK");
  });

  // ── Seguretat BlauProtocol v2 ─────────────────────────────────

  server.on("/securityStatus", HTTP_GET, [](AsyncWebServerRequest *r) {
    String json = "{\"configured\":" + String(securityConfigured() ? "true" : "false")
                + ",\"wl\":" + String(securityWhitelistCount())
                + ",\"wlMacs\":[";
    for (int i = 0; i < securityWhitelistCount(); i++) {
      uint8_t m[6];
      if (!securityWhitelistMac(i, m)) break;
      char buf[20];
      snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
               m[0], m[1], m[2], m[3], m[4], m[5]);
      if (i > 0) json += ",";
      json += "\"" + String(buf) + "\"";
    }
    json += "]}";
    r->send(200, "application/json", json);
  });

  server.on("/security", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!r->hasParam("protopass", true)) { r->send(400, "text/plain", "missing protopass"); return; }
    String pass = r->getParam("protopass", true)->value();
    pass.trim();
    if (pass.length() < 8 || pass.length() > 63) {
      r->send(400, "text/plain", "password length 8-63");
      return;
    }
    bool ok = saveSecurityPassword(pass.c_str());   // PBKDF2 ~100 ms
    r->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "ERROR");
  });

  server.on("/clearsecurity", HTTP_POST, [](AsyncWebServerRequest *r) {
    clearSecurity();
    r->send(200, "text/plain", "OK");
  });

  server.on("/startlearning", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!securityConfigured()) { r->send(400, "text/plain", "not configured"); return; }
    securityStartLearning();
    r->send(200, "text/plain", "OK");
  });

  // ── Administració ─────────────────────────────────────────────

  server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", "OK"); delay(500); ESP.restart();
  });

  server.on("/clearconfig", HTTP_POST, [](AsyncWebServerRequest *r) {
    clearConfig();
    r->send(200, "text/plain", "OK"); delay(500); ESP.restart();
  });

  // ── OTA update ───────────────────────────────────────────────
  static String s_pending_fw_file;

  server.on("/ota-upload", HTTP_POST,
    [](AsyncWebServerRequest *r) {
      bool ok = !Update.hasError();
      if (ok && s_pending_fw_file.length() > 0) {
        prefs.begin("blau", false);
        prefs.putString("fw_file", s_pending_fw_file);
        prefs.end();
      }
      r->send(200, "application/json",
        ok ? "{\"ok\":true}"
           : String("{\"ok\":false,\"err\":\"") + Update.errorString() + "\"}");
      if (ok) { delay(200); ESP.restart(); }
    },
    [](AsyncWebServerRequest*, const String& filename, size_t index,
       uint8_t *data, size_t len, bool) {
      static uint32_t _app_sz, _lfs_sz, _written, _phase, _hdr_bytes;
      static uint8_t  _hdr[12];

      if (index == 0) {
        if (filename.length() > 0) s_pending_fw_file = filename;
        _app_sz = _lfs_sz = _written = _phase = _hdr_bytes = 0;
        Update.abort();
      }

      size_t off = 0;
      if (_hdr_bytes < 12) {
        size_t take = min((size_t)(12 - _hdr_bytes), len);
        memcpy(_hdr + _hdr_bytes, data, take);
        _hdr_bytes += take;
        off += take;
        if (_hdr_bytes < 12) return;
        if (memcmp(_hdr, "BLAU", 4) != 0) { Update.abort(); return; }
        memcpy(&_app_sz, _hdr + 4, 4);
        memcpy(&_lfs_sz, _hdr + 8, 4);
        if (!Update.begin(_app_sz, U_FLASH)) { Update.abort(); return; }
        _phase = 0; _written = 0;
      }

      uint8_t *ptr = data + off;
      size_t   rem = len  - off;
      while (rem > 0) {
        if (_phase == 0) {
          size_t can = min((size_t)(_app_sz - _written), rem);
          if (can > 0) { Update.write(ptr, can); _written += can; ptr += can; rem -= can; }
          if (_written >= _app_sz) {
            Update.end(true);
            if (_lfs_sz > 0 && Update.begin(_lfs_sz, U_SPIFFS)) {
              _phase = 1; _written = 0;
            } else { _phase = 2; }
          }
        } else if (_phase == 1) {
          size_t can = min((size_t)(_lfs_sz - _written), rem);
          if (can > 0) { Update.write(ptr, can); _written += can; ptr += can; rem -= can; }
          if (_written >= _lfs_sz) { Update.end(true); _phase = 2; }
        } else { break; }
      }
    }
  );

  server.onNotFound(serveixWifiManager);
  server.begin();
  LOG_I("[WEB] server started");
}
