#include "nvsconfig.h"
#include "output.h"

int8_t selectedTemplate = -1;
uint8_t powerupMode    = 0;
uint8_t lastSavedState = 0;

const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_SW:        return "reset SW";
    case ESP_RST_PANIC:     return "excepcio/panic";
    case ESP_RST_INT_WDT:   return "WDT interrupcio";
    case ESP_RST_TASK_WDT:  return "WDT tasca";
    case ESP_RST_WDT:       return "WDT (altre)";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    default:                return "desconegut";
  }
}

void logResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  LOG_I("[BOOT] Reset: %s (%d)", resetReasonStr(reason), (int)reason);
  prefs.begin("blau", false);
  prefs.putUChar("lastReset", (uint8_t)reason);
  prefs.end();
}

void clearConfig() {
  prefs.begin("blau", false);
  prefs.clear();
  prefs.end();
  LOG_I("[CFG] NVS esborrada");
}

void loadConfig() {
  memset(gpioMap, 0, sizeof(gpioMap));
  prefs.begin("blau", true);

  powerupMode    = prefs.getUChar("pwrup",  0);
  lastSavedState = prefs.getUChar("lastst", 0);

  // si NO coincideix la versió de la configuració, inicialitza amb valors predeterminats
  uint8_t schema = prefs.getUChar("schema", 0);
  if (schema != CONFIG_SCHEMA_VERSION) {
    prefs.end();
    LOG_I("[CFG] Schema v%d -> esperava v%d, usant defaults", schema, CONFIG_SCHEMA_VERSION);
    sta_ssid = sta_pass = mqtt_host = mqtt_user = mqtt_pass = "";
    mqtt_client = HC_MQTT_CLIENT; mqtt_topic = HC_MQTT_TOPIC; mqtt_fulltopic = HC_MQTT_FULLTOPIC; mqtt_port = HC_MQTT_PORT;
    device_name = WIFI_SSID;
    return;
  }

  // assigna valors guardats --> gpioMap[i].cfg (funció, params, nom)
  char key[4];
  for (int i = 0; i < MAX_GPIO_COUNT; i++) {
    snprintf(key, sizeof(key), "f%d", i);
    gpioMap[i].cfg.func   = (GpioFunc)prefs.getUChar(key, 0);
    snprintf(key, sizeof(key), "a%d", i);
    gpioMap[i].cfg.param1 = prefs.getUInt(key, 0);
    snprintf(key, sizeof(key), "b%d", i);
    gpioMap[i].cfg.param2 = (uint16_t)prefs.getUShort(key, 0);
    snprintf(key, sizeof(key), "c%d", i);
    gpioMap[i].cfg.param3 = (uint16_t)prefs.getUShort(key, 0);
    snprintf(key, sizeof(key), "n%d", i);
    String nm = prefs.getString(key, "");
    strncpy(gpioMap[i].cfg.name, nm.c_str(), 12);
    gpioMap[i].cfg.name[12] = '\0';
    snprintf(key, sizeof(key), "e%d", i);
    gpioMap[i].cfg.notificador = prefs.getBool(key, false);
  }

  // assigna valors guardats --> wifi i mqtt
  sta_ssid      = prefs.getString("sta_ssid", ""); sta_ssid.trim();
  sta_pass      = prefs.getString("sta_pass", ""); sta_pass.trim();
  mqtt_host      = prefs.getString("mqtt_host",      "");   //* i també per mqtt: mqtt.host
  mqtt_port      = (uint16_t)prefs.getInt("mqtt_port", 1883);   //* mqtt.port
  mqtt_user      = prefs.getString("mqtt_user",      "");       //* etc
  mqtt_pass      = prefs.getString("mqtt_pass",      "");
  mqtt_client    = prefs.getString("mqtt_client",    HC_MQTT_CLIENT);
  mqtt_topic     = prefs.getString("mqtt_topic",     HC_MQTT_TOPIC);
  mqtt_fulltopic = prefs.getString("mqtt_fulltopic", HC_MQTT_FULLTOPIC);
  device_name      = prefs.getString("devname",        WIFI_SSID);
  selectedTemplate = (int8_t)prefs.getChar("sel_tmpl", -1);
  prefs.end();

  // Logs
  LOG_D("[CFG] WiFi STA: ssid='%s' pass=%s", sta_ssid.c_str(), sta_pass.length() > 0 ? "***" : "(buit)");
  LOG_D("[CFG] MQTT: host='%s' port=%d user='%s' client='%s' topic='%s' fulltopic='%s'",
    mqtt_host.c_str(), mqtt_port, mqtt_user.c_str(),
    mqtt_client.c_str(), mqtt_topic.c_str(), mqtt_fulltopic.c_str());
}

void savePowerupMode(uint8_t mode) {
  prefs.begin("blau", false);
  prefs.putUChar("pwrup", mode);
  prefs.end();
}

void saveLastOutputState(bool on) {
  prefs.begin("blau", false);
  prefs.putUChar("lastst", on ? 1 : 0);
  prefs.end();
}

void saveConfig() {
  prefs.begin("blau", false);
  prefs.putUChar("schema", CONFIG_SCHEMA_VERSION);
  char key[4];
  for (int i = 0; i < MAX_GPIO_COUNT; i++) {
    snprintf(key, sizeof(key), "f%d", i);
    prefs.putUChar(key, (uint8_t)gpioMap[i].cfg.func);
    snprintf(key, sizeof(key), "a%d", i);
    prefs.putUInt(key, gpioMap[i].cfg.param1);
    snprintf(key, sizeof(key), "b%d", i);
    prefs.putUShort(key, gpioMap[i].cfg.param2);
    snprintf(key, sizeof(key), "c%d", i);
    prefs.putUShort(key, gpioMap[i].cfg.param3);
    snprintf(key, sizeof(key), "n%d", i);
    if (gpioMap[i].cfg.name[0]) prefs.putString(key, gpioMap[i].cfg.name);
    else prefs.remove(key);
    snprintf(key, sizeof(key), "e%d", i);
    prefs.putBool(key, gpioMap[i].cfg.notificador);
  }
  prefs.putString("sta_ssid",      sta_ssid);
  prefs.putString("sta_pass",      sta_pass);
  prefs.putString("mqtt_host",     mqtt_host);
  prefs.putInt   ("mqtt_port",     mqtt_port);
  prefs.putString("mqtt_user",     mqtt_user);
  prefs.putString("mqtt_pass",     mqtt_pass);
  prefs.putString("mqtt_client",   mqtt_client);
  prefs.putString("mqtt_topic",    mqtt_topic);
  prefs.putString("mqtt_fulltopic",mqtt_fulltopic);
  prefs.putString("devname",       device_name);
  prefs.putChar  ("sel_tmpl",      selectedTemplate);
  prefs.end();
  LOG_D("[CFG] Config guardada (schema v%d)", CONFIG_SCHEMA_VERSION);
  for (int i = 0; i < MAX_GPIO_COUNT; i++) {
    GpioFunc f = gpioMap[i].cfg.func;
    if (f == FUNC_NONE) continue;
    const char* fid = (f < FUNC_COUNT) ? FUNC_REGISTRY[f].id : "?";
    if (gpioMap[i].cfg.name[0])
      LOG_D("[CFG] GPIO%d: %s p1=%u p2=%u p3=%u nom='%s'", i, fid,
            gpioMap[i].cfg.param1, gpioMap[i].cfg.param2, gpioMap[i].cfg.param3,
            gpioMap[i].cfg.name);
    else
      LOG_D("[CFG] GPIO%d: %s p1=%u p2=%u p3=%u", i, fid,
            gpioMap[i].cfg.param1, gpioMap[i].cfg.param2, gpioMap[i].cfg.param3);
  }
}
