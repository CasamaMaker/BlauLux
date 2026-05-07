#include "webserver.h"
#include "output.h"
#include "nvsconfig.h"
#include "mqtt.h"

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
             control_type,
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
    String sPin1 = (pin1 == PIN_UNUSED) ? "null" : String(pin1);
    String sPin2 = (pin2 == PIN_UNUSED) ? "null" : String(pin2);
    String sPin3 = (pin3 == PIN_UNUSED) ? "null" : String(pin3);
    r->send(200, "application/json",
      "{\"pin1\":" + sPin1 + ",\"pin2\":" + sPin2 + ",\"pin3\":" + sPin3 + "}");
  });

  server.on("/brightness", HTTP_POST, [](AsyncWebServerRequest *r) {
    String json = "{\"b1\":" + String(brightness[1]) + ",\"b2\":" + String(brightness[2])
                + ",\"b3\":" + String(brightness[3]) + ",\"b4\":" + String(brightness[4])
                + ",\"bcw\":" + String(brightness_cw) + ",\"pf\":" + String(pwm_freq)
                + ",\"pd\":" + String(pwm_duty) + "}";
    r->send(200, "application/json", json);
  });

  server.on("/numLeds", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", String(num_leds));
  });

  server.on("/boto", HTTP_POST, [](AsyncWebServerRequest *r) {
    String b = (boto_pin == PIN_UNUSED) ? "null" : String(boto_pin);
    r->send(200, "application/json",
      "{\"boto\":" + b + ",\"bpu\":" + String(button_pullup) + "}");
  });

  server.on("/initialSetup", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", boto_pin == PIN_UNUSED ? "true" : "false");
  });

  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", FIRMWARE_VERSION);
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
        else if (p->name() == "brightness_cw") brightness_cw = p->value().toInt();
        else if (p->name() == "num_leds")      { int v = p->value().toInt(); num_leds = v > 0 ? v : 1; }
        else if (p->name() == "pwm_freq")      { int v = p->value().toInt(); if (v >= 100) pwm_freq = v; }
      }
    }
    if (newBrightness >= 0 && g_driver && g_driver->hasBrightness)
      brightness[control_type] = newBrightness;
    saveConfig();
    r->send(200, "text/plain", "OK");
  });

  server.on("/color", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("r", true) && r->hasParam("g", true) && r->hasParam("b", true)) {
      webTesting = true;
      int rv = r->getParam("r", true)->value().toInt();
      int gv = r->getParam("g", true)->value().toInt();
      int bv = r->getParam("b", true)->value().toInt();
      if (pin2 != PIN_UNUSED) digitalWrite(pin2, (rv == 0 && gv == 0 && bv == 0) ? LOW : HIGH);
      for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, strip.Color(rv, gv, bv));
      strip.show();
    }
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
      if (g_driver && g_driver->hasBrightness) brightness[control_type] = duty;
      state = (duty > 0);
      applyOutput(state);
    }
    r->send(200, "text/plain", "OK");
  });

  server.on("/duttyCW", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (r->hasParam("value", true)) {
      webTesting = true;
      brightness_cw = r->getParam("value", true)->value().toInt();
      if (state) applyOutput(true);
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
      WiFi.disconnect();
      if (sta_ssid.length() > 0) WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
    }
    r->send(200, "text/plain", "OK");
  });

  server.on("/clearwifi", HTTP_POST, [](AsyncWebServerRequest *r) {
    sta_ssid = ""; sta_pass = "";
    prefs.begin("blau", false);
    prefs.remove("sta_ssid"); prefs.remove("sta_pass");
    prefs.end();
    WiFi.disconnect();
    LOG_I("[WIFI] Credencials WiFi esborrades");
    r->send(200, "text/plain", "OK");
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
    for (int i = 0; i <= 21; i++) { snprintf(key, sizeof(key), "g%d", i); prefs.remove(key); }
    prefs.remove("b1"); prefs.remove("b2"); prefs.remove("b3"); prefs.remove("b4");
    prefs.remove("bcw"); prefs.remove("nl"); prefs.remove("pf"); prefs.remove("pd");
    prefs.remove("color");
    char nkey[4];
    for (int i = 0; i <= 21; i++) { snprintf(nkey, sizeof(nkey), "n%d", i); prefs.remove(nkey); }
    prefs.end();
    LOG_I("[CFG] Configuració hardware esborrada");
    r->send(200, "text/plain", "OK");
    delay(500); ESP.restart();
  });

  server.on("/gpiomap", HTTP_GET, [](AsyncWebServerRequest *r) {
    String json = "{";
    bool first = true;
    for (int i = 0; i <= 21; i++) {
      if (!first) json += ",";
      json += "\"g" + String(i) + "\":{\"func\":" + String((int)gpioMap[i].func)
            + ",\"chan\":" + String((int)gpioMap[i].canal) + "}";
      first = false;
    }
    for (int i = 0; i <= 21; i++) {
      if (gpio_names[i][0]) json += ",\"n" + String(i) + "\":\"" + String(gpio_names[i]) + "\"";
    }
    char colorHex[7];
    snprintf(colorHex, sizeof(colorHex), "%06lX", (unsigned long)currentColor);
    json += ",\"nl\":" + String(num_leds);
    json += ",\"pf\":" + String(pwm_freq);
    json += ",\"pd\":" + String(pwm_duty);
    json += ",\"b1\":" + String(brightness[1]);
    json += ",\"b4\":" + String(brightness[4]);
    json += ",\"color\":\"" + String(colorHex) + "\"";
    json += "}";
    r->send(200, "application/json", json);
  });

  server.on("/gpiomap", HTTP_POST, [](AsyncWebServerRequest *r) {
    char key[4];
    for (int i = 0; i <= 21; i++) {
      snprintf(key, sizeof(key), "g%d", i);
      if (r->hasParam(key, true)) {
        uint8_t packed = (uint8_t)r->getParam(key, true)->value().toInt();
        gpioMap[i].func  = (GpioFunc)(packed & 0x0F);
        gpioMap[i].canal = (packed >> 4) & 0x0F;
      }
    }
    if (r->hasParam("nl", true)) { int v = r->getParam("nl", true)->value().toInt(); if (v > 0) num_leds = v; }
    if (r->hasParam("pf", true)) { int v = r->getParam("pf", true)->value().toInt(); if (v >= 100) pwm_freq = v; }
    if (r->hasParam("pd", true)) { int v = r->getParam("pd", true)->value().toInt(); pwm_duty = constrain(v, 0, 100); }
    if (r->hasParam("color", true)) {
      String hex = r->getParam("color", true)->value();
      if (hex.length() == 6) currentColor = (uint32_t)strtoul(hex.c_str(), NULL, 16);
    }
    char nkey[4];
    for (int i = 0; i <= 21; i++) {
      snprintf(nkey, sizeof(nkey), "n%d", i);
      if (r->hasParam(nkey, true)) {
        String nm = r->getParam(nkey, true)->value();
        strncpy(gpio_names[i], nm.c_str(), 12);
        gpio_names[i][12] = '\0';
      }
    }
    saveConfig();
    applyGpioConfig();
    configuracioLlum();
    if (r->hasParam("b",   true)) { int v = r->getParam("b",   true)->value().toInt(); if (g_driver && g_driver->hasBrightness) brightness[control_type] = v; saveConfig(); }
    if (r->hasParam("bcw", true)) { int v = r->getParam("bcw", true)->value().toInt(); brightness_cw = v; saveConfig(); }
    LOG_I("[CFG] gpio_config actualitzat via /gpiomap");
    r->send(200, "text/plain", "OK");
  });

  server.on("/funclist", HTTP_GET, [](AsyncWebServerRequest *r) {
    String json = "[";
    for (int i = 0; i < (int)FUNC_COUNT; i++) {
      if (i > 0) json += ",";
      json += "{\"id\":\"" + String(FUNC_REGISTRY[i].id) + "\""
            + ",\"label\":\"" + String(FUNC_REGISTRY[i].label) + "\""
            + ",\"needsChan\":" + (FUNC_REGISTRY[i].needsChan ? "true" : "false")
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
              + ",\"func\":" + String((int)DEVICE_TEMPLATES[t].pins[p].func)
              + ",\"chan\":" + String((int)DEVICE_TEMPLATES[t].pins[p].canal) + "}";
      }
      json += "]}";
    }
    json += "]";
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

  // ── Administració ─────────────────────────────────────────────

  server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", "OK"); delay(500); ESP.restart();
  });

  server.on("/clearconfig", HTTP_POST, [](AsyncWebServerRequest *r) {
    clearConfig();
    r->send(200, "text/plain", "OK"); delay(500); ESP.restart();
  });

  server.onNotFound(serveixWifiManager);
  server.begin();
  LOG_I("[WEB] server started");
}
