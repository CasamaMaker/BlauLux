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

// Base del tòpic per a un canal: "{base}/ch{N}"
static String mqttChanBase(uint8_t chan) {
  return mqttBaseTopic() + "/ch" + String(chan);
}

// ── Publicació d'estat ────────────────────────────────────────────

void publishState() {
  if (!mqttClient.connected()) return;
  String base = mqttBaseTopic();

  // Entitat global legacy (backward compat)
  LOG_D("[MQTT] publish %s/state = %s", base.c_str(), state ? "ON" : "OFF");
  mqttClient.publish((base + "/state").c_str(), 1, true, state ? "ON" : "OFF");
  if (g_driver && g_driver->hasBrightness) {
    LOG_D("[MQTT] publish %s/brightness = %d", base.c_str(), brightness[control_type]);
    mqttClient.publish((base + "/brightness").c_str(), 1, true, String(brightness[control_type]).c_str());
  }
  if (g_driver && g_driver->hasColor) {
    uint8_t r = (currentColor >> 16) & 0xFF;
    uint8_t g = (currentColor >> 8)  & 0xFF;
    uint8_t b =  currentColor        & 0xFF;
    String rgb = String(r) + "," + String(g) + "," + String(b);
    LOG_D("[MQTT] publish %s/rgb = %s", base.c_str(), rgb.c_str());
    mqttClient.publish((base + "/rgb").c_str(), 1, true, rgb.c_str());
  }

  // Per canal: publica l'estat de tots els canals actius
  for (uint8_t ch = 1; ch <= 15; ch++)
    publishChannelState(ch);
}

void publishChannelState(uint8_t chan) {
  if (!mqttClient.connected()) return;
  if (chan == 0 || chan > 15 || g_dev.hw[chan].type == CH_NONE) return;

  String cb = mqttChanBase(chan);
  const ChannelState& s    = g_dev.state[chan];
  ChannelCaps         caps = getChannelCaps(g_dev.hw[chan].type);

  LOG_D("[MQTT] ch%d publish %s/state = %s", chan, cb.c_str(), s.on ? "ON" : "OFF");
  mqttClient.publish((cb + "/state").c_str(), 1, true, s.on ? "ON" : "OFF");

  if (caps.hasBrightness) {
    mqttClient.publish((cb + "/brightness").c_str(), 1, true, String(s.brightness).c_str());
  }
  if (caps.hasColor) {
    uint8_t r = (s.color >> 16) & 0xFF;
    uint8_t g = (s.color >> 8)  & 0xFF;
    uint8_t b =  s.color        & 0xFF;
    mqttClient.publish((cb + "/rgb").c_str(), 1, true, (String(r)+","+String(g)+","+String(b)).c_str());
  }
}

// ── Discovery Home Assistant ──────────────────────────────────────

// Bloc de dispositiu compartit per totes les entitats del mateix BlauTrigger.
static String _deviceBlock(const String& id) {
  String uid  = "blautrigger_" + id;
  String name = "BlauTrigger " + id;
  return "\"device\":{\"identifiers\":[\"" + uid + "\"],"
         "\"name\":\"" + name + "\","
         "\"model\":\"BlauTrigger v1\","
         "\"manufacturer\":\"Blau\"}";
}

void publishHADiscovery() {
  if (!mqttClient.connected()) return;
  String id   = macAPSuffix;
  String base = mqttBaseTopic();
  String uid  = "blautrigger_" + id;
  String name = "BlauTrigger " + id;
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

void publishChannelDiscovery(uint8_t chan) {
  if (!mqttClient.connected()) return;
  if (chan == 0 || chan > 15 || g_dev.hw[chan].type == CH_NONE) return;

  String id  = macAPSuffix;
  String cb  = mqttChanBase(chan);
  String uid = "blautrigger_" + id + "_ch" + String(chan);
  String name = "BlauTrigger " + id + " CH" + String(chan);
  String dev  = _deviceBlock(id);
  String base = mqttBaseTopic();

  ChannelCaps caps = getChannelCaps(g_dev.hw[chan].type);
  String topic, payload;

  if (!caps.hasBrightness) {
    topic   = "homeassistant/switch/" + uid + "/config";
    payload = "{\"name\":\"" + name + "\","
              "\"unique_id\":\"" + uid + "\","
              "\"state_topic\":\"" + cb + "/state\","
              "\"command_topic\":\"" + cb + "/set\","
              "\"payload_on\":\"ON\","
              "\"payload_off\":\"OFF\","
              "\"availability_topic\":\"" + base + "/available\","
              + dev + "}";
  } else {
    topic   = "homeassistant/light/" + uid + "/config";
    payload = "{\"name\":\"" + name + "\","
              "\"unique_id\":\"" + uid + "\","
              "\"state_topic\":\"" + cb + "/state\","
              "\"command_topic\":\"" + cb + "/set\","
              "\"brightness_state_topic\":\"" + cb + "/brightness\","
              "\"brightness_command_topic\":\"" + cb + "/brightness/set\","
              "\"brightness_scale\":100,";
    if (caps.hasColor)
      payload += "\"rgb_state_topic\":\"" + cb + "/rgb\","
                 "\"rgb_command_topic\":\"" + cb + "/rgb/set\",";
    payload += "\"availability_topic\":\"" + base + "/available\","
               + dev + "}";
  }
  LOG_I("[MQTT] HA ch discovery ch%d -> %s", chan, topic.c_str());
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

  // Subscripcions globals legacy
  mqttClient.subscribe((base + "/set").c_str(), 1);
  mqttClient.subscribe((base + "/brightness/set").c_str(), 1);
  if (g_driver && g_driver->hasColor) mqttClient.subscribe((base + "/rgb/set").c_str(), 1);

  // Subscripcions i discovery per canal
  for (uint8_t ch = 1; ch <= 15; ch++) {
    if (g_dev.hw[ch].type == CH_NONE) continue;
    String cb = mqttChanBase(ch);
    mqttClient.subscribe((cb + "/set").c_str(), 1);
    ChannelCaps caps = getChannelCaps(g_dev.hw[ch].type);
    if (caps.hasBrightness) mqttClient.subscribe((cb + "/brightness/set").c_str(), 1);
    if (caps.hasColor)      mqttClient.subscribe((cb + "/rgb/set").c_str(), 1);
    publishChannelDiscovery(ch);
  }

  // Disponibilitat, discovery global i estat inicial
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

  String base      = mqttBaseTopic();
  String chanPrefix = base + "/ch";

  // ── Comandes per canal: {base}/ch{N}/{cmd} ────────────────────
  if (topicStr.startsWith(chanPrefix)) {
    String rest     = topicStr.substring(chanPrefix.length());
    int    slashPos = rest.indexOf('/');
    if (slashPos > 0) {
      uint8_t chan = (uint8_t)rest.substring(0, slashPos).toInt();
      String  cmd  = rest.substring(slashPos + 1);

      if (chan >= 1 && chan <= 15 && g_dev.hw[chan].type != CH_NONE) {
        if (cmd == "set") {
          if (payloadStr == "ON")  setChannelOn(chan, true);
          if (payloadStr == "OFF") setChannelOn(chan, false);
          publishChannelState(chan);

        } else if (cmd == "brightness/set") {
          setChannelBrightness(chan, (uint8_t)constrain(payloadStr.toInt(), 0, 100));
          publishChannelState(chan);

        } else if (cmd == "rgb/set") {
          int c1 = payloadStr.indexOf(',');
          int c2 = payloadStr.indexOf(',', c1 + 1);
          if (c1 > 0 && c2 > c1) {
            uint8_t r = (uint8_t)payloadStr.substring(0, c1).toInt();
            uint8_t g = (uint8_t)payloadStr.substring(c1 + 1, c2).toInt();
            uint8_t b = (uint8_t)payloadStr.substring(c2 + 1).toInt();
            setChannelColor(chan, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
            publishChannelState(chan);
          }
        }
      }
    }
    return;
  }

  // ── Comandes globals legacy ───────────────────────────────────
  if (topicStr == base + "/set") {
    if (payloadStr == "ON"  && !state) { state = true;  applyOutput(true);  }
    if (payloadStr == "OFF" &&  state) { state = false; applyOutput(false); }
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
      currentColor = strip.Color(r, g, b);
      if (g_driver && g_driver->hasColor && state) applyOutput(true);
      publishState();
    }
  }
}
