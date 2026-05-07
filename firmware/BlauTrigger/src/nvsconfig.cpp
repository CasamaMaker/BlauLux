#include "nvsconfig.h"

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
  uint8_t schema = prefs.getUChar("schema", 0);
  if (schema != CONFIG_SCHEMA_VERSION) {
    prefs.end();
    LOG_I("[CFG] Schema v%d -> esperava v%d, usant defaults", schema, CONFIG_SCHEMA_VERSION);
    brightness[1] = brightness[2] = brightness[3] = brightness[4] = BRIGHTNESS_DEF;
    brightness_cw = BRIGHTNESS_DEF;
    num_leds = NUM_LEDS; pwm_freq = PWM_FREQ;
    sta_ssid = sta_pass = mqtt_host = mqtt_user = mqtt_pass = "";
    mqtt_port = 1883;
    mqtt_client = HC_MQTT_CLIENT; mqtt_topic = HC_MQTT_TOPIC; mqtt_fulltopic = HC_MQTT_FULLTOPIC;
    device_name = WIFI_SSID;
    return;
  }
  char key[4];
  for (int i = 0; i <= 21; i++) {
    snprintf(key, sizeof(key), "g%d", i);
    uint8_t packed = prefs.getUChar(key, 0);
    gpioMap[i].func  = (GpioFunc)(packed & 0x0F);
    gpioMap[i].canal = (packed >> 4) & 0x0F;
  }
  brightness[1] = prefs.getInt("b1",  BRIGHTNESS_DEF);
  brightness[2] = prefs.getInt("b2",  BRIGHTNESS_DEF);
  brightness[3] = prefs.getInt("b3",  BRIGHTNESS_DEF);
  brightness[4] = prefs.getInt("b4",  BRIGHTNESS_DEF);
  brightness_cw = prefs.getInt("bcw", BRIGHTNESS_DEF);
  num_leds      = prefs.getInt("nl",  NUM_LEDS);
  pwm_freq      = prefs.getInt("pf",  PWM_FREQ);
  pwm_duty      = prefs.getInt("pd",  PWM_DUTY_DEF);
  currentColor  = prefs.getUInt("color", COLOR_LLUM);
  char nkey[4];
  for (int i = 0; i <= 21; i++) {
    snprintf(nkey, sizeof(nkey), "n%d", i);
    String nm = prefs.getString(nkey, "");
    strncpy(gpio_names[i], nm.c_str(), 12);
    gpio_names[i][12] = '\0';
  }
  sta_ssid      = prefs.getString("sta_ssid", "");
  sta_pass      = prefs.getString("sta_pass", "");
  mqtt_host      = prefs.getString("mqtt_host",      "");
  mqtt_port      = (uint16_t)prefs.getInt("mqtt_port", 1883);
  mqtt_user      = prefs.getString("mqtt_user",      "");
  mqtt_pass      = prefs.getString("mqtt_pass",      "");
  mqtt_client    = prefs.getString("mqtt_client",    HC_MQTT_CLIENT);
  mqtt_topic     = prefs.getString("mqtt_topic",     HC_MQTT_TOPIC);
  mqtt_fulltopic = prefs.getString("mqtt_fulltopic", HC_MQTT_FULLTOPIC);
  device_name    = prefs.getString("devname",        WIFI_SSID);
  prefs.end();
  LOG_D("[CFG] WiFi STA: ssid='%s' pass=%s", sta_ssid.c_str(), sta_pass.length() > 0 ? "***" : "(buit)");
  LOG_D("[CFG] MQTT: host='%s' port=%d user='%s' client='%s' topic='%s' fulltopic='%s'",
    mqtt_host.c_str(), mqtt_port, mqtt_user.c_str(),
    mqtt_client.c_str(), mqtt_topic.c_str(), mqtt_fulltopic.c_str());
}

void saveConfig() {
  prefs.begin("blau", false);
  prefs.putUChar("schema", CONFIG_SCHEMA_VERSION);
  char key[4];
  for (int i = 0; i <= 21; i++) {
    snprintf(key, sizeof(key), "g%d", i);
    uint8_t packed = ((gpioMap[i].canal & 0x0F) << 4) | (gpioMap[i].func & 0x0F);
    prefs.putUChar(key, packed);
  }
  prefs.putInt("b1",  brightness[1]);
  prefs.putInt("b2",  brightness[2]);
  prefs.putInt("b3",  brightness[3]);
  prefs.putInt("b4",  brightness[4]);
  prefs.putInt("bcw", brightness_cw);
  prefs.putInt("nl",  num_leds);
  prefs.putInt("pf",  pwm_freq);
  prefs.putInt("pd",  pwm_duty);
  prefs.putUInt("color", currentColor);
  char nkey[4];
  for (int i = 0; i <= 21; i++) {
    snprintf(nkey, sizeof(nkey), "n%d", i);
    if (gpio_names[i][0]) prefs.putString(nkey, gpio_names[i]);
    else prefs.remove(nkey);
  }
  prefs.end();
  LOG_D("[CFG] Config guardada (schema v2): ct=%d p1=%d p2=%d p3=%d bp=%d nl=%d pf=%d",
    control_type, pin1, pin2, pin3, boto_pin, num_leds, pwm_freq);
}
