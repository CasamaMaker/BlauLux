#include "mqtt.h"
#include "output.h"
#include "nvsconfig.h"

// ── Helpers de tòpic ─────────────────────────────────────────────

String mqttResolvedTopic() {
  String t = mqtt_topic;
  t.replace("%id%", macAPSuffix);
  return t;
}

String mqttBaseTopic() {
  String ft = mqtt_fulltopic;
  ft.replace("%id%",    macAPSuffix);
  ft.replace("%topic%", mqttResolvedTopic());
  return ft;
}

String mqttResolvedClientId() {
  String c = mqtt_client;
  c.replace("%id%",    macAPSuffix);
  c.replace("%topic%", mqttResolvedTopic());
  return c;
}

// ── Publicació d'estat ────────────────────────────────────────────

void publishState() {
  if (!mqttClient.connected()) return;
  String base = mqttBaseTopic();

  LOG_D("[MQTT] publish %s/state = %s", base.c_str(), getState() ? "ON" : "OFF");
  mqttClient.publish((base + "/state").c_str(), 1, true, getState() ? "ON" : "OFF");
  if (g_driver && g_driver->hasBrightness) {
    LOG_D("[MQTT] publish %s/brightness = %d", base.c_str(), getBrightnessForType(getControlType()));
    mqttClient.publish((base + "/brightness").c_str(), 1, true, String(getBrightnessForType(getControlType())).c_str());
  }
  if (g_driver && g_driver->hasColor) {
    uint32_t cc = getCurrentColor();
    uint8_t r = (cc >> 16) & 0xFF;
    uint8_t g = (cc >> 8)  & 0xFF;
    uint8_t b =  cc        & 0xFF;
    String rgb = String(r) + "," + String(g) + "," + String(b);
    LOG_D("[MQTT] publish %s/rgb = %s", base.c_str(), rgb.c_str());
    mqttClient.publish((base + "/rgb").c_str(), 1, true, rgb.c_str());
  }
}

// ── Discovery Home Assistant ──────────────────────────────────────

// Bloc de dispositiu compartit per totes les entitats del mateix BlauLux.
static String _deviceBlock(const String& id) {
  String uid  = "blaulux_" + id;
  String name = "BlauLux " + id;
  return "\"device\":{\"identifiers\":[\"" + uid + "\"],"
         "\"name\":\"" + name + "\","
         "\"model\":\"BlauLux v1\","
         "\"manufacturer\":\"Blau\"}";
}

void publishHADiscovery() {
  if (!mqttClient.connected()) return;
  String id   = macAPSuffix;
  String base = mqttBaseTopic();
  String uid  = "blaulux_" + id;
  String name = "BlauLux " + id;
  String dev  = _deviceBlock(id);

  String topic, payload;
  if (!g_driver || !g_driver->hasBrightness) {
    topic   = "homeassistant/switch/" + uid + "/config";
    payload = "{\"name\":\"" + name + "\","
              "\"unique_id\":\"" + uid + "\","
              "\"state_topic\":\"" + base + "/state\","
              "\"command_topic\":\"" + base + "/set\","
              "\"payload_on\":\"ON\","
              "\"payload_off\":\"OFF\","
              "\"availability_topic\":\"" + base + "/available\","
              + dev + "}";
  } else {
    topic   = "homeassistant/light/" + uid + "/config";
    payload = "{\"name\":\"" + name + "\","
              "\"unique_id\":\"" + uid + "\","
              "\"state_topic\":\"" + base + "/state\","
              "\"command_topic\":\"" + base + "/set\","
              "\"brightness_state_topic\":\"" + base + "/brightness\","
              "\"brightness_command_topic\":\"" + base + "/brightness/set\","
              "\"brightness_scale\":100,";
    if (g_driver && g_driver->hasColor)
      payload += "\"rgb_state_topic\":\"" + base + "/rgb\","
                 "\"rgb_command_topic\":\"" + base + "/rgb/set\",";
    payload += "\"availability_topic\":\"" + base + "/available\","
               + dev + "}";
  }
  LOG_I("[MQTT] HA discovery -> %s", topic.c_str());
  mqttClient.publish(topic.c_str(), 1, true, payload.c_str());
}

// ── Connexió i desconnexió ────────────────────────────────────────

void connectMqtt() {
  if (mqtt_host.length() == 0 || !WiFi.isConnected()) {
    LOG_I("[MQTT] connect skipped (host='%s' wifi=%d)", mqtt_host.c_str(), (int)WiFi.isConnected());
    return;
  }
  LOG_I("[MQTT] connecting to %s:%d", mqtt_host.c_str(), mqtt_port);
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  LOG_I("[MQTT] connected! session=%s base=%s", sessionPresent ? "yes" : "no", mqttBaseTopic().c_str());
  String base = mqttBaseTopic();

  mqttClient.subscribe((base + "/set").c_str(), 1);
  mqttClient.subscribe((base + "/brightness/set").c_str(), 1);
  if (g_driver && g_driver->hasColor) mqttClient.subscribe((base + "/rgb/set").c_str(), 1);

  // Disponibilitat, discovery i estat inicial
  mqttClient.publish((base + "/available").c_str(), 1, true, "online");
  publishHADiscovery();
  publishState();

  {
    prefs.begin("blau", true);
    uint8_t lastReset = prefs.getUChar("lastReset", 0xFF);
    prefs.end();
    if (lastReset != 0xFF) {
      mqttClient.publish((base + "/stat/reset").c_str(), 1, false,
                         resetReasonStr((esp_reset_reason_t)lastReset));
      prefs.begin("blau", false);
      prefs.remove("lastReset");
      prefs.end();
    }
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  LOG_I("[MQTT] disconnected (reason: %d)", (int)reason);
  if (WiFi.isConnected() && mqttReconnectTimer != nullptr)
    xTimerStart(mqttReconnectTimer, 0);
}

// ── Recepció de missatges ─────────────────────────────────────────

void onMqttMessage(char* topic, char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {
  String topicStr(topic);
  String payloadStr;
  for (size_t i = 0; i < len; i++) payloadStr += (char)payload[i];
  LOG_I("[MQTT] recv [%s] = [%s]", topicStr.c_str(), payloadStr.c_str());

  String base = mqttBaseTopic();

  if (topicStr == base + "/set") {
    if (payloadStr == "ON"  && !getState()) applyOutput(true);
    if (payloadStr == "OFF" &&  getState()) applyOutput(false);
    publishState();

  } else if (topicStr == base + "/brightness/set") {
    int br = constrain(payloadStr.toInt(), 0, 100);
    if (g_driver && g_driver->hasBrightness) applyBrightness(br);
    publishState();

  } else if (topicStr == base + "/rgb/set") {
    int c1 = payloadStr.indexOf(',');
    int c2 = payloadStr.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      uint8_t r = (uint8_t)payloadStr.substring(0, c1).toInt();
      uint8_t g = (uint8_t)payloadStr.substring(c1 + 1, c2).toInt();
      uint8_t b = (uint8_t)payloadStr.substring(c2 + 1).toInt();
      setCurrentColor(((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
      if (g_driver && g_driver->hasColor && getState()) applyOutput(true);
      publishState();
    }
  }
}
