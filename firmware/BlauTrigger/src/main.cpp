// BlauTrigger — firmware per controlar càrregues AC (bombeta, tira PWM, CW/WW, RGB, relé, triac)
// Plataformes suportades: ESP32-C3

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>

#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"

#include <esp_sleep.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <esp_now.h>
#include <Adafruit_NeoPixel.h>
#include <blauprotocol.h>
#include <blauprotocol_trg.h>
#include "config.h"
#include <AsyncMqttClient.h>


// ════════════════════════════════════════════════════════════════
//  RESOLUCIÓ DE CONFIGURACIÓ DE HARDWARE
//  En mode HARDCODED_CONFIG els pins i el tipus de control
//  són constants de compilació. En mode web es llegeixen de NVS.
// ════════════════════════════════════════════════════════════════
#ifdef HARDCODED_CONFIG
  #define control_type  HW_CONTROL_TYPE
  #define pin1          HW_PIN1
  #define pin2          HW_PIN2
  #define pin3          HW_PIN3
  #define boto_pin      PIN_BOTO
  #define num_leds      NUM_LEDS
  #define brightness_cw BRIGHTNESS_DEF
  #define button_pullup BUTTON_PULLUP
#else
  int control_type;
  int pin1;
  int pin2;
  int pin3;
  int boto_pin;
  int num_leds      = NUM_LEDS;
  int brightness_cw = BRIGHTNESS_DEF;
  int pwm_freq      = PWM_FREQ;
  int button_pullup = BUTTON_PULLUP;
#endif

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;


// ════════════════════════════════════════════════════════════════
//  OBJECTES GLOBALS
// ════════════════════════════════════════════════════════════════
AsyncWebServer server(HTTP_PORT);   // servidor web asíncron (portal captiu)
DNSServer      dnsServer;           // servidor DNS per redirigir qualsevol domini al portal

String macAP, macAPSuffix;          // MAC del AP (completa i últims 4 caràcters)


// ════════════════════════════════════════════════════════════════
//  LED DIGITAL (NeoPixel)
// ════════════════════════════════════════════════════════════════
Adafruit_NeoPixel strip(NUM_LEDS, PIN_UNUSED, NEO_GRB + NEO_KHZ800);
int brightness[5] = {0, BRIGHTNESS_DEF, BRIGHTNESS_DEF, BRIGHTNESS_DEF, BRIGHTNESS_DEF};  // índex = control_type


// ════════════════════════════════════════════════════════════════
//  ESTAT GLOBAL
// ════════════════════════════════════════════════════════════════
unsigned long startTime;                   // instant en què es comença a prémer el botó
bool state      = false;                   // estat actual de la llum (ON/OFF)
bool webTesting = false;                   // actiu quan la web envia color/duty directament

static volatile bool _ack_pending = false; // hi ha un ACK pendent d'enviar per ESP-NOW
static uint8_t       _ack_mac[6];          // MAC destinatari de l'ACK
static BlauPacket_t  _ack_pkt;             // paquet ACK preparat

int pwmCh1 = 0;                            // canal LEDC per pin1
int pwmCh2 = 1;                            // canal LEDC per pin2 (mode WW/CW)

uint8_t mqttR = (COLOR_MQTT >> 16) & 0xFF;  // color per defecte MQTT mode 1
uint8_t mqttG = (COLOR_MQTT >> 8)  & 0xFF;
uint8_t mqttB = (COLOR_MQTT)       & 0xFF;


// ════════════════════════════════════════════════════════════════
//  CONTROL DE FASE — variables globals (mode 4)
// ════════════════════════════════════════════════════════════════
static SemaphoreHandle_t _zcdSemaphore    = NULL;
static TaskHandle_t      _triacTaskHandle = NULL;
static int               _zcdPin          = PIN_UNUSED;
static volatile uint32_t _zcdMicros       = 0;

void IRAM_ATTR zcdISR() {
  _zcdMicros = (uint32_t)micros();
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(_zcdSemaphore, &woken);
  if (woken) portYIELD_FROM_ISR();
}

void triacTask(void* pvParameters) {
  (void)pvParameters;
  while (1) {
    if (xSemaphoreTake(_zcdSemaphore, pdMS_TO_TICKS(100)) != pdTRUE) continue;
    if (!state || pin2 == PIN_UNUSED) continue;
    int power = brightness[4];
    if (power <= 0) continue;

    // Retard de fase: potència alta → retard curt; potència baixa → retard llarg
    uint32_t delay_us = (uint32_t)((100 - power) * (AC_HALF_CYCLE_US - TRIAC_MIN_DELAY_US) / 100
                                    + TRIAC_MIN_DELAY_US);
    uint32_t target = _zcdMicros + delay_us;

    // Espera gruixuda (allibera CPU)
    int32_t remaining = (int32_t)(target - (uint32_t)micros());
    if (remaining > 3000) vTaskDelay(pdMS_TO_TICKS((remaining - 3000) / 1000));

    // Espera fina (spin precís)
    while ((int32_t)(target - (uint32_t)micros()) > 0);

    if (state && pin2 != PIN_UNUSED) {
      digitalWrite(pin2, HIGH);
      delayMicroseconds(TRIAC_PULSE_US);
      digitalWrite(pin2, LOW);
    }
  }
}

// Neteja interrupció ZCD i la tasca de triac
void cleanupTriac() {
  if (_zcdPin != PIN_UNUSED) {
    detachInterrupt(digitalPinToInterrupt(_zcdPin));
    _zcdPin = PIN_UNUSED;
  }
  if (_triacTaskHandle != NULL) {
    vTaskDelete(_triacTaskHandle);
    _triacTaskHandle = NULL;
  }
  if (_zcdSemaphore != NULL) {
    vSemaphoreDelete(_zcdSemaphore);
    _zcdSemaphore = NULL;
  }
  if (pin2 != PIN_UNUSED) digitalWrite(pin2, LOW);
}


// ════════════════════════════════════════════════════════════════
//  WIFI STA + MQTT — variables globals (sempre presents)
//  En mode web: loadConfig() les omple des de NVS.
//  En mode HARDCODED: setup() les omple des de config.h (HC_*).
// ════════════════════════════════════════════════════════════════
String sta_ssid;        // SSID de la xarxa domèstica (WiFi STA)
String sta_pass;        // contrasenya de la xarxa domèstica
String         mqtt_host;
uint16_t       mqtt_port            = 1883;
String         mqtt_user;
String         mqtt_pass;
String         mqtt_client;          // plantilla client ID  (ex: "BlauTrigger_%id%")
String         mqtt_topic;           // plantilla topic curt (ex: "%id%")
String         mqtt_fulltopic;       // plantilla full topic (ex: "blautrigger/%topic%")
String         mqttClientId;         // persistent: AsyncMqttClient guarda const char*, no copia
String         mqttWillTopic;        // persistent: idem
AsyncMqttClient   mqttClient;
TimerHandle_t  mqttReconnectTimer   = nullptr;


// ════════════════════════════════════════════════════════════════
//  GESTIÓ DE CONFIGURACIÓ (NVS / Preferences)
//  Només disponible quan HARDCODED_CONFIG no està definit.
// ════════════════════════════════════════════════════════════════
#ifndef HARDCODED_CONFIG
Preferences prefs;

#ifdef CLEAR_CONFIG
// Esborra tota la configuració guardada i reseteja els pins a PIN_UNUSED
void clearConfig() {
  prefs.begin("blau", false);
  prefs.putInt("ct",  PIN_UNUSED);
  prefs.putInt("p1",  PIN_UNUSED);
  prefs.putInt("p2",  PIN_UNUSED);
  prefs.putInt("p3",  PIN_UNUSED);
  prefs.putInt("bp",  PIN_UNUSED);
  prefs.putInt("bpu", BUTTON_PULLUP);
  prefs.putInt("b1",  BRIGHTNESS_DEF);
  prefs.putInt("b2",  BRIGHTNESS_DEF);
  prefs.putInt("b3",  BRIGHTNESS_DEF);
  prefs.putInt("b4",  BRIGHTNESS_DEF);
  prefs.putInt("bcw", BRIGHTNESS_DEF);
  prefs.putInt("nl",  NUM_LEDS);
  prefs.putInt("pf",  PWM_FREQ);
  prefs.putString("sta_ssid", "");
  prefs.putString("sta_pass", "");
  prefs.putString("mqtt_host",      "");
  prefs.putInt("mqtt_port",         1883);
  prefs.putString("mqtt_user",      "");
  prefs.putString("mqtt_pass",      "");
  prefs.putString("mqtt_client",    HC_MQTT_CLIENT);
  prefs.putString("mqtt_topic",     HC_MQTT_TOPIC);
  prefs.putString("mqtt_fulltopic", HC_MQTT_FULLTOPIC);
  prefs.end();
  Serial.println("Config NVS esborrada (pins reset a PIN_UNUSED)!");
}
#endif

// Llegeix la configuració des de NVS
void loadConfig() {
  prefs.begin("blau", true);
  control_type  = prefs.getInt("ct",  PIN_UNUSED);
  pin1          = prefs.getInt("p1",  PIN_UNUSED);
  pin2          = prefs.getInt("p2",  PIN_UNUSED);
  pin3          = prefs.getInt("p3",  PIN_UNUSED);
  boto_pin      = prefs.getInt("bp",  PIN_UNUSED);
  button_pullup = prefs.getInt("bpu", BUTTON_PULLUP);
  brightness[1] = prefs.getInt("b1",  BRIGHTNESS_DEF);
  brightness[2] = prefs.getInt("b2",  BRIGHTNESS_DEF);
  brightness[3] = prefs.getInt("b3",  BRIGHTNESS_DEF);
  brightness[4] = prefs.getInt("b4",  BRIGHTNESS_DEF);
  brightness_cw = prefs.getInt("bcw", BRIGHTNESS_DEF);
  num_leds      = prefs.getInt("nl",  NUM_LEDS);
  pwm_freq      = prefs.getInt("pf",  PWM_FREQ);
  sta_ssid      = prefs.getString("sta_ssid", "");
  sta_pass      = prefs.getString("sta_pass", "");
  mqtt_host      = prefs.getString("mqtt_host",      "");
  mqtt_port      = (uint16_t)prefs.getInt("mqtt_port", 1883);
  mqtt_user      = prefs.getString("mqtt_user",      "");
  mqtt_pass      = prefs.getString("mqtt_pass",      "");
  mqtt_client    = prefs.getString("mqtt_client",    HC_MQTT_CLIENT);
  mqtt_topic     = prefs.getString("mqtt_topic",     HC_MQTT_TOPIC);
  mqtt_fulltopic = prefs.getString("mqtt_fulltopic", HC_MQTT_FULLTOPIC);
  if (pin1 == 99) pin1 = PIN_UNUSED;  // migració de valors antics
  if (pin2 == 99) pin2 = PIN_UNUSED;
  prefs.end();
  Serial.printf("Config carregada: ct=%d p1=%d p2=%d p3=%d bp=%d bpu=%d b1=%d b2=%d b3=%d b4=%d bcw=%d nl=%d pf=%d\n",
    control_type, pin1, pin2, pin3, boto_pin, button_pullup, brightness[1], brightness[2], brightness[3], brightness[4], brightness_cw, num_leds, pwm_freq);
  Serial.printf("WiFi STA: ssid='%s' pass=%s\n", sta_ssid.c_str(), sta_pass.length() > 0 ? "***" : "(buit)");
  Serial.printf("MQTT: host='%s' port=%d user='%s' client='%s' topic='%s' fulltopic='%s'\n",
    mqtt_host.c_str(), mqtt_port, mqtt_user.c_str(),
    mqtt_client.c_str(), mqtt_topic.c_str(), mqtt_fulltopic.c_str());
}

// Desa la configuració actual a NVS
void saveConfig() {
  prefs.begin("blau", false);
  prefs.putInt("ct",  control_type);
  prefs.putInt("p1",  pin1);
  prefs.putInt("p2",  pin2);
  prefs.putInt("p3",  pin3);
  prefs.putInt("bp",  boto_pin);
  prefs.putInt("bpu", button_pullup);
  prefs.putInt("b1",  brightness[1]);
  prefs.putInt("b2",  brightness[2]);
  prefs.putInt("b3",  brightness[3]);
  prefs.putInt("b4",  brightness[4]);
  prefs.putInt("bcw", brightness_cw);
  prefs.putInt("nl",  num_leds);
  prefs.putInt("pf",  pwm_freq);
  prefs.end();
  Serial.printf("Config guardada: ct=%d p1=%d p2=%d p3=%d bp=%d bpu=%d b1=%d b2=%d b3=%d b4=%d bcw=%d nl=%d pf=%d\n",
    control_type, pin1, pin2, pin3, boto_pin, button_pullup, brightness[1], brightness[2], brightness[3], brightness[4], brightness_cw, num_leds, pwm_freq);
}
#endif


// ════════════════════════════════════════════════════════════════
//  ESP-NOW
// ════════════════════════════════════════════════════════════════

// Callback quan s'ha enviat un ACK per ESP-NOW
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "ACK TX: OK" : "ACK TX: FAIL");
}

// Inicialitza ESP-NOW; reinicia el dispositiu si falla
void initEspNow() {
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}


// Returns true when button is pressed, accounting for pull-up or pull-down config
bool buttonPressed() {
  return digitalRead(boto_pin) == (button_pullup ? LOW : HIGH);
}


// ════════════════════════════════════════════════════════════════
//  SERVIDOR WEB (portal captiu de configuració)
// ════════════════════════════════════════════════════════════════

// Atura el servidor web i el sistema de fitxers (l'AP WiFi segueix actiu)
void stopWebServer() {
  server.reset();
  dnsServer.stop();
  LittleFS.end();
  Serial.println("Servidor i LittleFS aturats, però WiFi AP continua actiu");
}

// Serveix la pàgina principal del gestor WiFi des de LittleFS
void serveixWifiManager(AsyncWebServerRequest *request) {
  request->send(LittleFS, "/wifimanager.html", "text/html");
}

// Declaració anticipada necessària per al POST de configuració
void configuracioLlum();
void controlLlum(String trigger);


// ════════════════════════════════════════════════════════════════
//  MQTT
// ════════════════════════════════════════════════════════════════

// Resol la plantilla substituint %id% → últims 4 caràcters de la MAC
String mqttResolvedTopic() {
  String t = mqtt_topic;
  t.replace("%id%", macAPSuffix);
  return t;
}

// Resol el full topic substituint %id% i %topic%
String mqttBaseTopic() {
  String ft = mqtt_fulltopic;
  ft.replace("%id%",    macAPSuffix);
  ft.replace("%topic%", mqttResolvedTopic());
  return ft;
}

// Resol el client ID substituint %id% i %topic%
String mqttResolvedClientId() {
  String c = mqtt_client;
  c.replace("%id%",    macAPSuffix);
  c.replace("%topic%", mqttResolvedTopic());
  return c;
}

void publishState() {
  if (!mqttClient.connected()) return;
  String base = mqttBaseTopic();
  Serial.printf("[MQTT] publish %s/state = %s\n", base.c_str(), state ? "ON" : "OFF");
  mqttClient.publish((base + "/state").c_str(), 1, true, state ? "ON" : "OFF");
  if (control_type >= 1 && control_type <= 4) {
    Serial.printf("[MQTT] publish %s/brightness = %d\n", base.c_str(), brightness[control_type]);
    mqttClient.publish((base + "/brightness").c_str(), 1, true, String(brightness[control_type]).c_str());
  }
  if (control_type == 1) {
    String rgb = String(mqttR) + "," + String(mqttG) + "," + String(mqttB);
    Serial.printf("[MQTT] publish %s/rgb = %s\n", base.c_str(), rgb.c_str());
    mqttClient.publish((base + "/rgb").c_str(), 1, true, rgb.c_str());
  }
}

void publishHADiscovery() {
  if (!mqttClient.connected()) return;
  String id   = macAPSuffix;
  String base = mqttBaseTopic();
  String name = "BlauTrigger " + id;
  String uid  = "blautrigger_" + id;
  String dev  = "\"device\":{\"identifiers\":[\"" + uid + "\"],"
                "\"name\":\"" + name + "\","
                "\"model\":\"BlauTrigger v1\","
                "\"manufacturer\":\"Blau\"}";

  String topic, payload;
  if (control_type == 0) {
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
    if (control_type == 1)
      payload += "\"rgb_state_topic\":\"" + base + "/rgb\","
                 "\"rgb_command_topic\":\"" + base + "/rgb/set\",";
    payload += "\"availability_topic\":\"" + base + "/available\","
               + dev + "}";
  }
  Serial.printf("[MQTT] HA discovery → %s\n", topic.c_str());
  mqttClient.publish(topic.c_str(), 1, true, payload.c_str());
}

void connectMqtt() {
  if (mqtt_host.length() == 0 || !WiFi.isConnected()) {
    Serial.printf("[MQTT] connect skipped (host='%s' wifi=%d)\n",
      mqtt_host.c_str(), (int)WiFi.isConnected());
    return;
  }
  Serial.printf("[MQTT] connecting to %s:%d  (IP: %s)\n",
    mqtt_host.c_str(), mqtt_port, WiFi.localIP().toString().c_str());
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.printf("[MQTT] connected! session=%s  base=%s\n",
    sessionPresent ? "yes" : "no", mqttBaseTopic().c_str());
  String base = mqttBaseTopic();
  mqttClient.subscribe((base + "/set").c_str(), 1);
  Serial.println("[MQTT] subscribed: " + base + "/set");
  mqttClient.subscribe((base + "/brightness/set").c_str(), 1);
  Serial.println("[MQTT] subscribed: " + base + "/brightness/set");
  if (control_type == 1) {
    mqttClient.subscribe((base + "/rgb/set").c_str(), 1);
    Serial.println("[MQTT] subscribed: " + base + "/rgb/set");
  }
  mqttClient.publish((base + "/available").c_str(), 1, true, "online");
  Serial.println("[MQTT] published:  " + base + "/available = online");
  publishHADiscovery();
  publishState();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("[MQTT] disconnected (reason: %d)\n", (int)reason);
  if (WiFi.isConnected() && mqttReconnectTimer != nullptr)
    xTimerStart(mqttReconnectTimer, 0);
}


void onMqttMessage(char* topic, char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {
  String topicStr(topic);
  String payloadStr;
  for (size_t i = 0; i < len; i++) payloadStr += (char)payload[i];
  Serial.printf("[MQTT] recv [%s] = [%s]\n", topicStr.c_str(), payloadStr.c_str());

  String base = mqttBaseTopic();

  if (topicStr == base + "/set") {
    if (payloadStr == "ON"  && !state) controlLlum("mqtt");
    if (payloadStr == "OFF" &&  state) controlLlum("mqtt");
    publishState();

  } else if (topicStr == base + "/brightness/set") {
    int br = constrain(payloadStr.toInt(), 0, 100);
    if (control_type >= 1 && control_type <= 4) {
      brightness[control_type] = br;
      if (state) {
        switch (control_type) {
          case 1: strip.setBrightness(map(br, 0, 100, 0, 255)); strip.show(); break;
          case 2: ledcWrite(pwmCh1, map(br, 0, 100, 0, 255)); break;
          case 3: ledcWrite(pwmCh1, map(br, 0, 100, 0, 255)); break;
          case 4: break;
        }
      }
    }
    publishState();

  } else if (topicStr == base + "/rgb/set") {
    int c1 = payloadStr.indexOf(',');
    int c2 = payloadStr.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      mqttR = (uint8_t)payloadStr.substring(0, c1).toInt();
      mqttG = (uint8_t)payloadStr.substring(c1 + 1, c2).toInt();
      mqttB = (uint8_t)payloadStr.substring(c2 + 1).toInt();
      if (control_type == 1 && state) {
        for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, strip.Color(mqttR, mqttG, mqttB));
        strip.show();
      }
      publishState();
    }
  }
}


void webServerSetup() {
  // Pàgina principal (ordinador)
  server.on("/", HTTP_GET, serveixWifiManager);

  // Detecció de portal captiu — Windows 11
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });
  server.on("/wpad.dat",        [](AsyncWebServerRequest *request) { request->send(404); });

  // Detecció de portal captiu — Android
  server.on("/generate_204", HTTP_GET, serveixWifiManager);

  // Detecció de portal captiu — Windows (addicionals)
  server.on("/ncsi.txt",                    HTTP_GET, serveixWifiManager);
  server.on("/hotspot-detect.html",         HTTP_GET, serveixWifiManager);
  server.on("/library/test/success.html",   HTTP_GET, serveixWifiManager);
  server.on("/redirect",                    HTTP_GET, serveixWifiManager);
  server.on("/fwlink",                      HTTP_GET, serveixWifiManager);
  server.on("/cdn-cgi/",                    HTTP_GET, serveixWifiManager);
  server.on("/canonical.html",              HTTP_GET, serveixWifiManager);

  // Detecció de portal captiu — Firefox
  server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });

  // Icona del navegador (no servida)
  server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });

  // Full d'estils
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
    Serial.println("Served CSS");
  });

  // ── Endpoints d'estat (GET) ──────────────────────────────────

  // Retorna el mode de control actiu (0=On/Off, 1=NeoPixel, 2=PWM, 3=WW/CW)
  server.on("/driverMode", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(control_type));
    Serial.println(control_type);
  });

  // Retorna "hardcoded" o "web" segons el mode de configuració compilat
  server.on("/configMode", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifdef HARDCODED_CONFIG
      request->send(200, "text/plain", "hardcoded");
    #else
      request->send(200, "text/plain", "web");
    #endif
  });

  // Retorna la MAC del AP
  server.on("/mymac", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", WiFi.softAPmacAddress());
    Serial.println(macAP);
  });

  // Retorna els pins configurats en format JSON
  server.on("/pins", HTTP_POST, [](AsyncWebServerRequest *request) {
    String sPin1 = (pin1 == PIN_UNUSED) ? "null" : String(pin1);
    String sPin2 = (pin2 == PIN_UNUSED) ? "null" : String(pin2);
    String sPin3 = (pin3 == PIN_UNUSED) ? "null" : String(pin3);
    String json  = "{\"pin1\":" + sPin1 + ",\"pin2\":" + sPin2 + ",\"pin3\":" + sPin3 + "}";
    request->send(200, "application/json", json);
    Serial.println(json);
  });

  // Retorna la brillantor de cada mode en format JSON
  server.on("/brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    String json = "{\"b1\":" + String(brightness[1]) + ",\"b2\":" + String(brightness[2]) + ",\"b3\":" + String(brightness[3]) + ",\"b4\":" + String(brightness[4]) + ",\"bcw\":" + String(brightness_cw) + ",\"pf\":" + String(pwm_freq) + "}";
    request->send(200, "application/json", json);
  });

  // Retorna el nombre de LEDs configurats
  server.on("/numLeds", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(num_leds));
  });

  // Retorna el pin del botó i el mode pull-up/pull-down en format JSON
  server.on("/boto", HTTP_POST, [](AsyncWebServerRequest *request) {
    String b = (boto_pin == PIN_UNUSED) ? "null" : String(boto_pin);
    request->send(200, "application/json", "{\"boto\":" + b + ",\"bpu\":" + String(button_pullup) + "}");
  });

  // Retorna "true" si encara no s'ha configurat el botó (first-run)
  server.on("/initialSetup", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", boto_pin == PIN_UNUSED ? "true" : "false");
  });

  // Retorna la versió del firmware
  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", FIRMWARE_VERSION);
  });

  // ── Endpoints d'escriptura (POST) ────────────────────────────

  // Rep i desa la configuració enviada per la web
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifndef HARDCODED_CONFIG
      int newBrightness = -1;
      int params = request->params();
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          if      (p->name() == "control_type") control_type  = p->value().toInt();
          else if (p->name() == "pin1")         pin1          = p->value().isEmpty() ? PIN_UNUSED : p->value().toInt();
          else if (p->name() == "pin2")         pin2          = p->value().isEmpty() ? PIN_UNUSED : p->value().toInt();
          else if (p->name() == "pin3")         pin3          = p->value().isEmpty() ? PIN_UNUSED : p->value().toInt();
          else if (p->name() == "boto_pin")     boto_pin      = p->value().isEmpty() ? PIN_UNUSED : p->value().toInt();
          else if (p->name() == "button_pullup") button_pullup = p->value().toInt();
          else if (p->name() == "brightness")   newBrightness = p->value().toInt();
          else if (p->name() == "brightness_cw") brightness_cw = p->value().toInt();
          else if (p->name() == "num_leds")     { int v = p->value().toInt(); num_leds = v > 0 ? v : 1; }
          else if (p->name() == "pwm_freq")     { int v = p->value().toInt(); if (v >= 100) pwm_freq = v; }
        }
      }
      if (newBrightness >= 0 && control_type >= 1 && control_type <= 4)
        brightness[control_type] = newBrightness;
      saveConfig();
      configuracioLlum();
    #endif
    request->send(200, "text/plain", "OK");
  });

  // Rep un color RGB per previsualitzar el LED digital des de la web
  server.on("/color", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("r", true) && request->hasParam("g", true) && request->hasParam("b", true)) {
      webTesting = true;
      int r = request->getParam("r", true)->value().toInt();
      int g = request->getParam("g", true)->value().toInt();
      int b = request->getParam("b", true)->value().toInt();
      if (pin2 != PIN_UNUSED) digitalWrite(pin2, (r == 0 && g == 0 && b == 0) ? LOW : HIGH);
      for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, strip.Color(r, g, b));
      strip.show();
      Serial.printf("Color: rgb(%d,%d,%d)\n", r, g, b);
    }
    request->send(200, "text/plain", "OK");
  });

  // Rep un valor de brillantor WW (0–100) per previsualitzar des de la web
  server.on("/dutty", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
      webTesting = true;
      int duty = request->getParam("value", true)->value().toInt();
      if (control_type >= 1 && control_type <= 4) brightness[control_type] = duty;
      switch (control_type) {
        case 1: strip.setBrightness(map(duty, 0, 100, 0, 255)); strip.show(); break;
        case 2: ledcWrite(pwmCh1, map(duty, 0, 100, 0, 255)); break;
        case 3: ledcWrite(pwmCh1, map(duty, 0, 100, 0, 255)); break;
        case 4: break;  // la tasca triac llegeix brightness[4] automàticament
      }
    }
    request->send(200, "text/plain", "OK");
  });

  // Rep un valor de brillantor CW (0–100) per al canal 2 (mode WW/CW)
  server.on("/duttyCW", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
      webTesting = true;
      int duty = request->getParam("value", true)->value().toInt();
      brightness_cw = duty;
      if (control_type == 3 && pin2 != PIN_UNUSED) {
        ledcWrite(pwmCh2, map(duty, 0, 100, 0, 255));
      } else if (control_type == 4 && pin3 != PIN_UNUSED) {
        uint8_t ledBr = (uint8_t)map((long)duty * brightness[4] / 100, 0, 100, 0, 255);
        strip.setBrightness(max((uint8_t)5, ledBr));
        strip.show();
      }
    }
    request->send(200, "text/plain", "OK");
  });

  // Retorna l'estat de la connexió WiFi STA en format JSON
  server.on("/wifiStatus", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool connected = WiFi.status() == WL_CONNECTED;
    String ip   = connected ? WiFi.localIP().toString() : "";
    int    rssi = connected ? WiFi.RSSI() : 0;
    String json = "{\"connected\":" + String(connected ? "true" : "false") +
                  ",\"ip\":\"" + ip + "\"" +
                  ",\"ssid\":\"" + sta_ssid + "\"" +
                  ",\"pass\":\"" + sta_pass + "\"" +
                  ",\"rssi\":" + String(rssi) + "}";
    request->send(200, "application/json", json);
  });

  // Desa les credencials WiFi STA a NVS i reconnecta
  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifndef HARDCODED_CONFIG
      if (request->hasParam("sta_ssid", true)) {
        sta_ssid = request->getParam("sta_ssid", true)->value();
        sta_pass = request->hasParam("sta_pass", true) ? request->getParam("sta_pass", true)->value() : "";
        prefs.begin("blau", false);
        prefs.putString("sta_ssid", sta_ssid);
        prefs.putString("sta_pass", sta_pass);
        prefs.end();
        Serial.println("WiFi STA credentials saved: " + sta_ssid);
        WiFi.disconnect();
        if (sta_ssid.length() > 0) WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
      }
    #endif
    request->send(200, "text/plain", "OK");
  });

  // Retorna l'estat de la connexió MQTT en format JSON
  server.on("/mqttStatus", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool mqConnected = mqttClient.connected();
    String json = "{\"connected\":" + String(mqConnected ? "true" : "false") +
                  ",\"broker\":\"" + mqtt_host + "\"" +
                  ",\"port\":" + String(mqtt_port) +
                  ",\"user\":\"" + mqtt_user + "\"" +
                  ",\"pass\":\"" + mqtt_pass + "\"" +
                  ",\"client\":\"" + mqtt_client + "\"" +
                  ",\"mqtt_topic\":\"" + mqtt_topic + "\"" +
                  ",\"fulltopic\":\"" + mqtt_fulltopic + "\"" +
                  ",\"topic\":\"" + (mqConnected ? mqttBaseTopic() : "") + "\"}";
    request->send(200, "application/json", json);
  });

  // Desa la configuració MQTT a NVS i reconnecta
  server.on("/mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifndef HARDCODED_CONFIG
      if (request->hasParam("mqtt_host", true)) {
        mqtt_host      = request->getParam("mqtt_host", true)->value();
        mqtt_port      = request->hasParam("mqtt_port", true) ?
                         (uint16_t)request->getParam("mqtt_port", true)->value().toInt() : 1883;
        mqtt_user      = request->hasParam("mqtt_user", true)      ? request->getParam("mqtt_user",      true)->value() : "";
        mqtt_pass      = request->hasParam("mqtt_pass", true)      ? request->getParam("mqtt_pass",      true)->value() : "";
        mqtt_client    = request->hasParam("mqtt_client", true)    ? request->getParam("mqtt_client",    true)->value() : HC_MQTT_CLIENT;
        mqtt_topic     = request->hasParam("mqtt_topic", true)     ? request->getParam("mqtt_topic",     true)->value() : HC_MQTT_TOPIC;
        mqtt_fulltopic = request->hasParam("mqtt_fulltopic", true) ? request->getParam("mqtt_fulltopic", true)->value() : HC_MQTT_FULLTOPIC;
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
        Serial.printf("MQTT config saved: %s:%d client='%s' topic='%s' fulltopic='%s'\n",
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
    #endif
    request->send(200, "text/plain", "OK");
  });

  // Qualsevol URL no reconeguda → portal captiu
  server.onNotFound(serveixWifiManager);

  server.begin();
  Serial.println("Web server started");
}


// ════════════════════════════════════════════════════════════════
//  WIFI / ACCESS POINT
// ════════════════════════════════════════════════════════════════

// Llegeix i formata la MAC del AP; guarda els últims 4 caràcters per al SSID
void getMyMacAddress() {
  Serial.print("MAC del microcontrolador: ");
  macAP = WiFi.softAPmacAddress();
  macAP.replace(":", "");
  Serial.print("La meva adreça MAC (ap)"); Serial.println(macAP);
  macAPSuffix = macAP.substring(macAP.length() - 4);
}

// Configura el dispositiu en mode AP+STA. L'AP obre al canal ESPNOW_CHANNEL
// per mantenir la compatibilitat amb ESP-NOW (BlauLink). La connexió STA
// es gestiona separadament des de setup().
void configDeviceAP() {
  WiFi.mode(WIFI_AP_STA);
  getMyMacAddress();
  String apSsid = String(ssid) + "_" + macAPSuffix;
  bool apOk = WiFi.softAP(apSsid, "", ESPNOW_CHANNEL);
  if (!apOk) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(apSsid));
    Serial.print("AP CHANNEL "); Serial.println(WiFi.channel());
    Serial.print("AP MAC: ");    Serial.println(WiFi.softAPmacAddress());
  }
}

// Munta LittleFS, inicia el DNS i arrenca el servidor web del portal captiu
void wifiApModeServer() {
  if (!LittleFS.begin()) return Serial.println("Error muntant LittleFS"), void();
  Serial.println("Wifi initialized");
  Serial.println(WiFi.softAPIP());
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());  // redirigeix qualsevol domini al portal
  webServerSetup();
  Serial.println("Setup complete");
}


// ════════════════════════════════════════════════════════════════
//  CONTROL DE LA LLUM
// ════════════════════════════════════════════════════════════════

// Inicialitza els perifèrics (GPIO, LEDC, NeoPixel) segons el tipus de control
void configuracioLlum() {
  cleanupTriac();  // atura ZCD ISR i tasca de triac si estaven actius
  if (pin1 == PIN_UNUSED) return;
  switch (control_type) {
    case 0:  // On/Off — relé + led indicador
      pinMode(pin1, OUTPUT);
      pinMode(pin2, OUTPUT);
      break;

    case 1:  // LED digital (NeoPixel / WS2812)
      strip.updateLength(num_leds);
      strip.setPin(pin1);
      strip.begin();
      strip.clear();
      strip.show();
      if (pin2 != PIN_UNUSED) {
        pinMode(pin2, OUTPUT);
        digitalWrite(pin2, LOW);
      }
      break;

    case 2:  // LED dimmer (1× PWM)
      ledcSetup(pwmCh1, pwm_freq, PWM_RESOLUTION);
      ledcAttachPin(pin1, pwmCh1);
      ledcWrite(pwmCh1, map(brightness[2], 0, 100, 0, 255));
      break;

    case 3:  // LEDs WW/CW (2× PWM)
      ledcSetup(pwmCh1, pwm_freq, PWM_RESOLUTION);
      ledcSetup(pwmCh2, pwm_freq, PWM_RESOLUTION);
      ledcAttachPin(pin1, pwmCh1);
      ledcAttachPin(pin2, pwmCh2);
      break;

    case 4:  // Triac control de fase (ZCD + MOC3021S) + WS2812 indicador
      // pin1 = ZCD entrada (H11AA4), pin2 = sortida triac (MOC3021S), pin3 = WS2812
      pinMode(pin2, OUTPUT);
      digitalWrite(pin2, LOW);
      if (pin3 != PIN_UNUSED) {
        strip.updateLength(1);
        strip.setPin(pin3);
        strip.begin();
        strip.clear();
        strip.show();
      }
      _zcdSemaphore = xSemaphoreCreateBinary();
      _zcdPin = pin1;
      pinMode(pin1, INPUT);
      attachInterrupt(digitalPinToInterrupt(pin1), zcdISR, RISING);
      xTaskCreate(triacTask, "triac", 3072, NULL, 5, &_triacTaskHandle);
      break;

    default:
      Serial.println("Mode desconegut");
      break;
  }
}

// Actua sobre la llum segons el trigger i el mode de control actiu.
// Triggers: "boto" | "espnow" → commuta; "inici" → parpelleig d'arrencada; "wifiAP" → pols suau
void controlLlum(String trigger) {
  if (pin1 == PIN_UNUSED) return;
  switch (control_type) {
    case 0:  // On/Off
      if (trigger == "boto" || trigger == "espnow" || trigger == "mqtt") {
        digitalWrite(pin1, state ? HIGH : LOW);                           // relé
        if (pin2 != PIN_UNUSED) digitalWrite(pin2, state ? HIGH : LOW);  // led indicador
      }
      if (trigger == "inici") {
        if (pin2 != PIN_UNUSED) { digitalWrite(pin2, HIGH); delay(INICI_BLINK_MS); digitalWrite(pin2, LOW); }
      }
      if (trigger == "wifiAP") {
        if (pin2 != PIN_UNUSED) digitalWrite(pin2, HIGH);
      }
      break;

    case 1:  // LED digital (NeoPixel)
      strip.setBrightness(map(brightness[1], 0, 100, 0, 255));
      if (trigger == "boto")   for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, state ? 0 : COLOR_BOTO);
      if (trigger == "espnow") for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, state ? 0 : COLOR_ESPNOW);
      if (trigger == "mqtt")   for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, state ? 0 : strip.Color(mqttR, mqttG, mqttB));
      if (trigger == "boto" || trigger == "espnow" || trigger == "mqtt") {
        if (pin2 != PIN_UNUSED) digitalWrite(pin2, state ? LOW : HIGH);
      }
      if (trigger == "inici") {
        if (pin2 != PIN_UNUSED) digitalWrite(pin2, HIGH);
        for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, COLOR_INICI);
        strip.show();
        delay(INICI_BLINK_MS);
        strip.clear();
        if (pin2 != PIN_UNUSED) digitalWrite(pin2, LOW);
      }
      if (trigger == "wifiAP") {
        if (pin2 != PIN_UNUSED) digitalWrite(pin2, HIGH);
        uint16_t osc   = (millis() / 2) % 510;
        uint8_t  bright = (osc < 255) ? osc : 510 - osc;
        strip.setBrightness(bright);
        for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, COLOR_WIFI_AP);
      }
      strip.show();
      break;

    case 2:  // LED dimmer (1× PWM)
      if (trigger == "boto" || trigger == "espnow" || trigger == "mqtt") {
        ledcWrite(pwmCh1, state ? map(brightness[2], 0, 100, 0, 255) : 0);
      }
      if (trigger == "inici") {
        ledcWrite(pwmCh1, map(brightness[2], 0, 100, 0, 255));
        delay(INICI_BLINK_MS);
        ledcWrite(pwmCh1, 0);
      }
      if (trigger == "wifiAP") {
        uint16_t osc   = (millis() / 2) % 510;
        uint8_t  bright = (osc < 255) ? osc : 510 - osc;
        ledcWrite(pwmCh1, bright);
      }
      break;

    case 3:  // LEDs WW/CW (2× PWM)
      if (trigger == "boto" || trigger == "espnow" || trigger == "mqtt") {
        ledcWrite(pwmCh1, state ? map(brightness[3],   0, 100, 0, 255) : 0);
        ledcWrite(pwmCh2, state ? map(brightness_cw,   0, 100, 0, 255) : 0);
      }
      if (trigger == "inici") {
        ledcWrite(pwmCh1, map(brightness[3],   0, 100, 0, 255));
        ledcWrite(pwmCh2, map(brightness_cw,   0, 100, 0, 255));
        delay(INICI_BLINK_MS);
        ledcWrite(pwmCh1, 0);
        ledcWrite(pwmCh2, 0);
      }
      if (trigger == "wifiAP") {
        uint16_t osc   = (millis() / 2) % 510;
        uint8_t  bright = (osc < 255) ? osc : 510 - osc;
        ledcWrite(pwmCh1, bright);
        ledcWrite(pwmCh2, bright);
      }
      break;

    case 4:  // Triac control de fase — WS2812 actua com mode 1, brillantor escalada per potència
      if (pin3 == PIN_UNUSED) break;
      if (trigger == "boto" || trigger == "espnow" || trigger == "mqtt") {
        if (!state) {
          // brillantor = brightness_cw (LED) × potència triac / 100
          uint8_t ledBr = (uint8_t)map((long)brightness_cw * brightness[4] / 100, 0, 100, 0, 255);
          strip.setBrightness(max((uint8_t)5, ledBr));
          strip.setPixelColor(0, trigger == "boto" ? COLOR_BOTO :
                                         trigger == "mqtt" ? COLOR_MQTT : COLOR_ESPNOW);
        } else {
          strip.clear();
        }
        strip.show();
      }
      if (trigger == "inici") {
        strip.setBrightness(map(brightness_cw, 0, 100, 0, 255));
        strip.setPixelColor(0, COLOR_INICI);
        strip.show();
        delay(INICI_BLINK_MS);
        strip.clear();
        strip.show();
      }
      if (trigger == "wifiAP") {
        uint16_t osc   = (millis() / 2) % 510;
        uint8_t  bright = (osc < 255) ? osc : 510 - osc;
        strip.setBrightness(bright);
        strip.setPixelColor(0, COLOR_WIFI_AP);
        strip.show();
      }
      break;

    default:
      Serial.println("Mode desconegut");
      break;
  }

  // Actualitza l'estat lògic excepte per als triggers d'indicació visual
  if (trigger != "inici" && trigger != "wifiAP") state = !state;
}


// ════════════════════════════════════════════════════════════════
//  PROTOCOL BLAU (ESP-NOW)
// ════════════════════════════════════════════════════════════════

// Aplica la brillantor actual al hardware sense canviar l'estat on/off
static void applyBrightness(int br) {
  switch (control_type) {
    case 1: strip.setBrightness(map(br, 0, 100, 0, 255)); strip.show(); break;
    case 2: ledcWrite(pwmCh1, map(br, 0, 100, 0, 255)); break;
    case 3: ledcWrite(pwmCh1, map(br, 0, 100, 0, 255)); break;
    case 4: break;  // la tasca triac llegeix brightness[4] automàticament
  }
}

// Processa un paquet rebut i retorna el codi ACK corresponent
uint8_t handleAction(uint8_t pktType, uint8_t cmd,
                     uint8_t p1, uint8_t p2, uint8_t p3) {
  if (pktType == TYPE_EVENT) {
    switch (cmd) {
      case EVT_CLICK_1:
      case EVT_CLICK_2:
      case EVT_CLICK_3:    controlLlum("espnow"); return ACK_OK;
      case EVT_LONG_START:
      case EVT_LONG_END:                          return ACK_OK;
      default:
        Serial.print("EVT desconegut: 0x"); Serial.println(cmd, HEX);
        return ACK_ERROR;
    }
  }

  if (pktType == TYPE_CMD) {
    switch (cmd) {
      case CMD_TOGGLE:           controlLlum("espnow"); return ACK_OK;
      case CMD_ON:   if (!state) controlLlum("espnow"); return ACK_OK;
      case CMD_OFF:  if ( state) controlLlum("espnow"); return ACK_OK;

      case CMD_SET_BRIGHTNESS: {
        if (control_type < 1 || control_type > 4) return ACK_OK;
        int br = constrain((int)p1, 0, 100);
        brightness[control_type] = br;
        if (state) applyBrightness(br);
        return ACK_OK;
      }

      case CMD_SET_RGB: {
        mqttR = p1; mqttG = p2; mqttB = p3;
        if (control_type == 1 && state) {
          for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, strip.Color(mqttR, mqttG, mqttB));
          strip.show();
        }
        return ACK_OK;
      }

      case CMD_SET_CCT: {
        if (control_type == 3) {
          brightness[3] = constrain((int)p1, 0, 100);
          brightness_cw = constrain((int)p2, 0, 100);
          if (state) {
            ledcWrite(pwmCh1, map(brightness[3], 0, 100, 0, 255));
            if (pin2 != PIN_UNUSED) ledcWrite(pwmCh2, map(brightness_cw, 0, 100, 0, 255));
          }
        }
        return ACK_OK;
      }

      case CMD_DIM_UP: {
        if (control_type < 1 || control_type > 4) return ACK_OK;
        int step = (p1 >= 1 && p1 <= 10) ? (int)p1 : 5;
        int br = constrain(brightness[control_type] + step, 0, 100);
        brightness[control_type] = br;
        if (state) applyBrightness(br);
        return ACK_OK;
      }

      case CMD_DIM_DOWN: {
        if (control_type < 1 || control_type > 4) return ACK_OK;
        int step = (p1 >= 1 && p1 <= 10) ? (int)p1 : 5;
        int br = constrain(brightness[control_type] - step, 0, 100);
        brightness[control_type] = br;
        if (br == 0 && state)  controlLlum("espnow");  // apaga si arriba a 0
        else if (state)        applyBrightness(br);
        return ACK_OK;
      }

      case CMD_SET_SCENE: {
        // Escenes predefinides (p1 = id):
        //   0 → Apagat
        //   1 → Ple  (100%, blanc càlid per RGB)
        //   2 → Lectura (30%, blanc càlid tènue per RGB)
        //   3 → Nit   (5%, taronja nit per RGB)
        struct { uint8_t br; uint8_t r, g, b; uint8_t cw; } scenes[] = {
          { 0,   0,   0,   0,   0 },  // 0: off
          { 100, 255, 200, 100, 100 },  // 1: ple
          { 30,  255, 150,  50,  15 },  // 2: lectura
          { 5,   255,  60,   0,   0 },  // 3: nit
        };
        if (p1 >= sizeof(scenes) / sizeof(scenes[0])) return ACK_ERROR;

        auto& sc = scenes[p1];
        if (p1 == 0) {
          if (state) controlLlum("espnow");
          return ACK_OK;
        }
        // Escenes 1-3: actualitza brillantor i color, encén si apagat
        if (control_type >= 1 && control_type <= 4) brightness[control_type] = sc.br;
        if (control_type == 1) { mqttR = sc.r; mqttG = sc.g; mqttB = sc.b; }
        if (control_type == 3) brightness_cw = sc.cw;
        if (!state) controlLlum("espnow");  // state → true
        // Aplica el color/brillantor correctes (sobreescriu l'indicador d'espnow)
        switch (control_type) {
          case 1: strip.setBrightness(map(sc.br, 0, 100, 0, 255));
                  for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, strip.Color(mqttR, mqttG, mqttB));
                  strip.show(); break;
          case 2: ledcWrite(pwmCh1, map(sc.br, 0, 100, 0, 255)); break;
          case 3: ledcWrite(pwmCh1, map(sc.br, 0, 100, 0, 255));
                  if (pin2 != PIN_UNUSED) ledcWrite(pwmCh2, map(sc.cw, 0, 100, 0, 255)); break;
          case 4: break;
        }
        return ACK_OK;
      }

      default:
        Serial.print("CMD no implementat: 0x"); Serial.println(cmd, HEX);
        return ACK_ERROR;
    }
  }

  return ACK_ERROR;
}

// Callback ESP-NOW: rep un paquet i prepara l'ACK si cal
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  blau_trg_on_data_recv(mac, data, len,
                        &_ack_pending, _ack_mac, &_ack_pkt,
                        handleAction,
                        state, (uint8_t)brightness[control_type], (uint8_t)control_type);
}


// ════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);

  #ifndef HARDCODED_CONFIG
    #ifdef CLEAR_CONFIG
      clearConfig();
    #endif
    loadConfig();

    // Si el botó no està configurat → mode de setup inicial (AP + web)
    if (boto_pin == PIN_UNUSED) {
      Serial.println("Boto no configurat, entrant en mode AP inicial...");
      configDeviceAP();
      wifiApModeServer();

      unsigned long apStart = millis();
      while (boto_pin == PIN_UNUSED) {
        dnsServer.processNextRequest();
        if (pin1 != PIN_UNUSED && control_type >= 0) controlLlum("wifiAP");
        delay(DNS_POLL_MS);
        if (millis() - apStart > WIFI_AP_TIMEOUT_MS) {
          Serial.println("Temps excedit en setup inicial");
          ESP.restart();
        }
      }
      delay(500);
      ESP.restart();  // reinicia amb la configuració nova ja desada a NVS
    }
  #endif

  configuracioLlum();   // inicialitza els perifèrics de la llum
  controlLlum("inici"); // parpelleig d'arrencada

  configDeviceAP();     // activa el WiFi en mode Access Point

  pinMode(boto_pin, button_pullup ? INPUT_PULLUP : INPUT_PULLDOWN);

  initEspNow();
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  #ifdef HARDCODED_CONFIG
    sta_ssid       = HC_STA_SSID;
    sta_pass       = HC_STA_PASS;
    mqtt_host      = HC_MQTT_HOST;
    mqtt_port      = HC_MQTT_PORT;
    mqtt_user      = HC_MQTT_USER;
    mqtt_pass      = HC_MQTT_PASS;
    mqtt_client    = HC_MQTT_CLIENT;
    mqtt_topic     = HC_MQTT_TOPIC;
    mqtt_fulltopic = HC_MQTT_FULLTOPIC;
  #endif

#ifdef ENABLE_WIFI_STA
  if (sta_ssid.length() > 0) {
    WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
    Serial.println("[WiFi] STA connecting to: " + sta_ssid);
  }
#endif

#ifdef ENABLE_MQTT
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)0,
                        [](TimerHandle_t){ Serial.println("[MQTT] timer → reconnect"); connectMqtt(); });
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
    Serial.printf("[MQTT] configured: broker=%s:%d user=%s\n",
      mqtt_host.c_str(), mqtt_port, mqtt_user.length() > 0 ? mqtt_user.c_str() : "(none)");
  }
#endif
}


// ════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ════════════════════════════════════════════════════════════════
void loop() {
  // Envia l'ACK pendent si n'hi ha un de preparat per ESP-NOW
  blau_trg_process_pending(&_ack_pending, _ack_mac, &_ack_pkt);

  // Detecta canvis de connexió WiFi STA i dispara connectMqtt quan toca
#if defined(ENABLE_WIFI_STA) && defined(ENABLE_MQTT)
  {
    static bool lastWifiConnected = false;
    bool wifiNow = WiFi.isConnected();
    if (wifiNow && !lastWifiConnected) {
      int staCh = WiFi.channel();
      Serial.printf("[WiFi] STA connected, IP: %s, canal STA: %d\n",
        WiFi.localIP().toString().c_str(), staCh);
      // El hardware mou l'AP al canal del STA. En ESP32-C3 (ràdio única) no es pot
      // canviar el canal quan el STA ja és connectat — BlauLink ha de fer scan per trobar-nos.
      esp_err_t chErr = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
      Serial.printf("[WiFi] esp_wifi_set_channel(%d) → %s (canal actual: %d)\n",
        ESPNOW_CHANNEL, chErr == ESP_OK ? "OK" : "FAIL (ràdio bloquejada al canal STA)", staCh);
      if (mqtt_host.length() > 0 && !mqttClient.connected()) connectMqtt();
    }
    if (!wifiNow && lastWifiConnected) {
      Serial.println("[WiFi] STA disconnected");
      if (mqttReconnectTimer != nullptr) xTimerStop(mqttReconnectTimer, 0);
    }
    lastWifiConnected = wifiNow;
  }
#endif

  // Publica l'estat per MQTT si ha canviat (per ESP-NOW o botó)
#ifdef ENABLE_MQTT
  {
    static bool lastState = false;
    if (lastState != state) { lastState = state; publishState(); }
  }
#endif

  if (buttonPressed()) {
    startTime = millis();
    Serial.println("Boto presionat");
    controlLlum("boto");

    // Espera que el botó s'alliberi o que es superi el temps per entrar al mode AP
    while (buttonPressed()) {
      if (startTime + WIFI_AP_HOLD_MS < millis()) {
        // ── Mode AP de configuració (botó mantingut > WIFI_AP_HOLD_MS) ──
        configDeviceAP();
        wifiApModeServer();
        controlLlum("wifiAP");

        bool buttonReleased = false;
        static bool lastButtonPressed = false;

        while (1) {
          dnsServer.processNextRequest();
          if (!webTesting) controlLlum("wifiAP");
          delay(DNS_POLL_MS);

          // Reinici per timeout
          if (startTime + WIFI_AP_TIMEOUT_MS < millis()) {
            Serial.println("Temps excedit");
            ESP.restart();
          }

          // Detecció de la seqüència: alliberar i tornar a prémer → reinicia el dispositiu
          bool pressed = buttonPressed();

          if (!pressed && lastButtonPressed && !buttonReleased) {
            buttonReleased = true;
            Serial.println("Botó alliberat");
            delay(BUTTON_RELEASE_DEBOUNCE_MS);
          }

          if (pressed && !lastButtonPressed && buttonReleased) {
            Serial.println("Botó premut després d'alliberar");
            buttonReleased = false;
            ESP.restart();
          }

          lastButtonPressed = pressed;
        }
      }
    }

    delay(BUTTON_DEBOUNCE_MS);
  }
}
