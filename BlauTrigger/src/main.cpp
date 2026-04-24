// BlauTrigger — firmware per controlar càrregues AC (bombeta, tira PWM, CW/WW, RGB, relé)
// Plataformes suportades: ESP32-C3

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>

#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"

#include <esp_sleep.h>
#include <Preferences.h>
#include <esp_now.h>
#include <Adafruit_NeoPixel.h>
#include <blauprotocol.h>
#include <blauprotocol_trg.h>
#include "config.h"


// ════════════════════════════════════════════════════════════════
//  RESOLUCIÓ DE CONFIGURACIÓ DE HARDWARE
//  En mode HARDCODED_CONFIG els pins i el tipus de control
//  són constants de compilació. En mode web es llegeixen de NVS.
// ════════════════════════════════════════════════════════════════
#ifdef HARDCODED_CONFIG
  #define control_type HW_CONTROL_TYPE
  #define pin1         HW_PIN1
  #define pin2         HW_PIN2
  #define boto_pin     Boto
#else
  int control_type;
  int pin1;
  int pin2;
  int boto_pin;
#endif

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;


// ════════════════════════════════════════════════════════════════
//  OBJECTES GLOBALS
// ════════════════════════════════════════════════════════════════
AsyncWebServer server(HTTP_PORT);   // servidor web asíncron (portal captiu)
DNSServer      dnsServer;           // servidor DNS per redirigir qualsevol domini al portal

String myAddresss, myAddresssEnd;   // MAC del AP (completa i últims 4 caràcters)


// ════════════════════════════════════════════════════════════════
//  LED DIGITAL (NeoPixel)
// ════════════════════════════════════════════════════════════════
Adafruit_NeoPixel strip(NUM_LEDS, PIN_UNUSED, NEO_GRB + NEO_KHZ800);
int brightness[4] = {0, BRIGHTNESS_DEF, BRIGHTNESS_DEF, BRIGHTNESS_DEF};  // índex = control_type


// ════════════════════════════════════════════════════════════════
//  ESTAT GLOBAL
// ════════════════════════════════════════════════════════════════
unsigned long startTime;                    // instant en què es comença a prémer el botó
bool state      = false;                    // estat actual de la llum (ON/OFF)
bool webTesting = false;                    // actiu quan la web envia color/duty directament

static volatile bool _ack_pending = false;  // hi ha un ACK pendent d'enviar per ESP-NOW
static uint8_t       _ack_mac[6];           // MAC destinatari de l'ACK
static BlauPacket_t  _ack_pkt;              // paquet ACK preparat

int pwmChannel  = 0;                        // canal LEDC per pin1
int pwmChannel2 = 1;                        // canal LEDC per pin2 (mode WW/CW)


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
  prefs.putInt("ct", PIN_UNUSED);
  prefs.putInt("p1", PIN_UNUSED);
  prefs.putInt("p2", PIN_UNUSED);
  prefs.putInt("bp", PIN_UNUSED);
  prefs.putInt("b1", BRIGHTNESS_DEF);
  prefs.putInt("b2", BRIGHTNESS_DEF);
  prefs.putInt("b3", BRIGHTNESS_DEF);
  prefs.end();
  Serial.println("Config NVS esborrada (pins reset a PIN_UNUSED)!");
}
#endif

// Llegeix la configuració des de NVS
void loadConfig() {
  prefs.begin("blau", true);
  control_type  = prefs.getInt("ct", PIN_UNUSED);
  pin1          = prefs.getInt("p1", PIN_UNUSED);
  pin2          = prefs.getInt("p2", PIN_UNUSED);
  boto_pin      = prefs.getInt("bp", PIN_UNUSED);
  brightness[1] = prefs.getInt("b1", BRIGHTNESS_DEF);
  brightness[2] = prefs.getInt("b2", BRIGHTNESS_DEF);
  brightness[3] = prefs.getInt("b3", BRIGHTNESS_DEF);
  if (pin1 == 99) pin1 = PIN_UNUSED;  // migració de valors antics
  if (pin2 == 99) pin2 = PIN_UNUSED;
  prefs.end();
  Serial.printf("Config carregada: ct=%d p1=%d p2=%d bp=%d b1=%d b2=%d b3=%d\n",
    control_type, pin1, pin2, boto_pin, brightness[1], brightness[2], brightness[3]);
}

// Desa la configuració actual a NVS
void saveConfig() {
  prefs.begin("blau", false);
  prefs.putInt("ct", control_type);
  prefs.putInt("p1", pin1);
  prefs.putInt("p2", pin2);
  prefs.putInt("bp", boto_pin);
  prefs.putInt("b1", brightness[1]);
  prefs.putInt("b2", brightness[2]);
  prefs.putInt("b3", brightness[3]);
  prefs.end();
  Serial.printf("Config guardada: ct=%d p1=%d p2=%d bp=%d b1=%d b2=%d b3=%d\n",
    control_type, pin1, pin2, boto_pin, brightness[1], brightness[2], brightness[3]);
}
#endif


// ════════════════════════════════════════════════════════════════
//  ESP-NOW
// ════════════════════════════════════════════════════════════════

// Callback quan s'ha enviat un ACK per ESP-NOW
void OnDataSent_TRG(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "ACK TX: OK" : "ACK TX: FAIL");
}

// Inicialitza ESP-NOW; reinicia el dispositiu si falla
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
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
  String path = "/wifimanager_" + String(IDIOMA) + ".html";
  request->send(LittleFS, path, "text/html");
}

// Declaració anticipada necessària per al POST de configuració
void configuracioLlum();

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
  server.on("/driverMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(control_type));
    Serial.println(control_type);
  });

  // Retorna "hardcoded" o "web" segons el mode de configuració compilat
  server.on("/configMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    #ifdef HARDCODED_CONFIG
      request->send(200, "text/plain", "hardcoded");
    #else
      request->send(200, "text/plain", "web");
    #endif
  });

  // Retorna la MAC del AP
  server.on("/mymac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", WiFi.softAPmacAddress());
    Serial.println(myAddresss);
  });

  // Retorna els pins configurats en format JSON
  server.on("/pins", HTTP_GET, [](AsyncWebServerRequest *request) {
    String p1   = (pin1 == PIN_UNUSED) ? "null" : String(pin1);
    String p2   = (pin2 == PIN_UNUSED) ? "null" : String(pin2);
    String json = "{\"pin1\":" + p1 + ",\"pin2\":" + p2 + "}";
    request->send(200, "application/json", json);
    Serial.println(json);
  });

  // Retorna la brillantor de cada mode en format JSON
  server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"b1\":" + String(brightness[1]) + ",\"b2\":" + String(brightness[2]) + ",\"b3\":" + String(brightness[3]) + "}";
    request->send(200, "application/json", json);
  });

  // Retorna el pin del botó en format JSON
  server.on("/boto", HTTP_GET, [](AsyncWebServerRequest *request) {
    String b = (boto_pin == PIN_UNUSED) ? "null" : String(boto_pin);
    request->send(200, "application/json", "{\"boto\":" + b + "}");
  });

  // Retorna "true" si encara no s'ha configurat el botó (first-run)
  server.on("/initialSetup", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", boto_pin == PIN_UNUSED ? "true" : "false");
  });

  // ── Endpoints d'escriptura (POST) ────────────────────────────

  // Rep i desa la configuració enviada per la web
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifndef HARDCODED_CONFIG
      int new_brightness = -1;
      int params = request->params();
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          if      (p->name() == "control_type") control_type   = p->value().toInt();
          else if (p->name() == "pin1")         pin1           = p->value().isEmpty() ? PIN_UNUSED : p->value().toInt();
          else if (p->name() == "pin2")         pin2           = p->value().isEmpty() ? PIN_UNUSED : p->value().toInt();
          else if (p->name() == "boto_pin")     boto_pin       = p->value().isEmpty() ? PIN_UNUSED : p->value().toInt();
          else if (p->name() == "brightness")   new_brightness = p->value().toInt();
        }
      }
      if (new_brightness >= 0 && control_type >= 1 && control_type <= 3)
        brightness[control_type] = new_brightness;
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
      strip.setPixelColor(0, strip.Color(r, g, b));
      strip.show();
      Serial.printf("Color: rgb(%d,%d,%d)\n", r, g, b);
    }
    request->send(200, "text/plain", "OK");
  });

  // Rep un valor de duty cycle (0–100) per previsualitzar la brillantor des de la web
  server.on("/dutty", HTTP_POST, [](AsyncWebServerRequest *request) {
    String dutyValue = "";
    if (request->hasParam("value", true)) {
      dutyValue = request->getParam("value", true)->value();
    }
    webTesting = true;
    int duty = dutyValue.toInt();
    Serial.print("Duty recibido: ");
    Serial.println(duty);
    if (control_type >= 1 && control_type <= 3) brightness[control_type] = duty;
    strip.setBrightness(map(duty, 0, 100, 0, 255));
    strip.show();
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
  myAddresss = WiFi.softAPmacAddress();
  myAddresss.replace(":", "");
  Serial.print("La meva adreça MAC (ap)"); Serial.println(myAddresss);
  myAddresssEnd = myAddresss.substring(myAddresss.length() - 4);
}

// Configura el dispositiu com a Access Point obert amb SSID = "BlauTrigger_XXXX"
void configDeviceAP() {
  WiFi.mode(WIFI_AP);
  getMyMacAddress();
  String fullSSID = String(ssid) + "_" + myAddresssEnd;
  bool result = WiFi.softAP(fullSSID, "");
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(fullSSID));
    Serial.print("AP CHANNEL "); Serial.println(WiFi.channel());
    Serial.print("AP MAC: ");    Serial.println(WiFi.softAPmacAddress());
  }
}

// Munta LittleFS, inicia el DNS i arrencat el servidor web del portal captiu
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
  if (pin1 == PIN_UNUSED) return;
  switch (control_type) {
    case 0:  // On/Off — relé + led indicador
      pinMode(pin1, OUTPUT);
      pinMode(pin2, OUTPUT);
      break;

    case 1:  // LED digital (NeoPixel / WS2812)
      strip = Adafruit_NeoPixel(NUM_LEDS, pin1, NEO_GRB + NEO_KHZ800);
      strip.begin();
      strip.clear();
      strip.show();
      break;

    case 2:  // LED dimmer (1× PWM)
      ledcSetup(pwmChannel, PWM_FREQ, PWM_RESOLUTION);
      ledcAttachPin(pin1, pwmChannel);
      ledcWrite(pwmChannel, map(brightness[2], 0, 100, 0, 255));
      break;

    case 3:  // LEDs WW/CW (2× PWM)
      ledcSetup(pwmChannel,  PWM_FREQ, PWM_RESOLUTION);
      ledcSetup(pwmChannel2, PWM_FREQ, PWM_RESOLUTION);
      ledcAttachPin(pin1, pwmChannel);
      ledcAttachPin(pin2, pwmChannel2);
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
      if (trigger == "boto" || trigger == "espnow") {
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
      if (trigger == "boto")   strip.setPixelColor(0, state ? 0 : COLOR_BOTO);
      if (trigger == "espnow") strip.setPixelColor(0, state ? 0 : COLOR_ESPNOW);
      if (trigger == "inici") {
        strip.setPixelColor(0, COLOR_INICI);
        strip.show();
        delay(INICI_BLINK_MS);
        strip.clear();
      }
      if (trigger == "wifiAP") {
        // pols sinusoïdal aproximat amb triangle 0→255→0 cada ~1 s
        uint16_t osc = (millis() / 2) % 510;
        uint8_t bright = (osc < 255) ? osc : 510 - osc;
        strip.setBrightness(bright);
        strip.setPixelColor(0, COLOR_WIFI_AP);
      }
      strip.show();
      break;

    case 2:  // LED dimmer (1× PWM)
      if (trigger == "boto" || trigger == "espnow") {
        ledcWrite(pwmChannel, state ? map(brightness[2], 0, 100, 0, 255) : 0);
      }
      if (trigger == "inici") {
        ledcWrite(pwmChannel, map(brightness[2], 0, 100, 0, 255));
        delay(INICI_BLINK_MS);
        ledcWrite(pwmChannel, 0);
      }
      if (trigger == "wifiAP") {
        uint16_t osc = (millis() / 2) % 510;
        uint8_t bright = (osc < 255) ? osc : 510 - osc;
        ledcWrite(pwmChannel, bright);
      }
      break;

    case 3:  // LEDs WW/CW (2× PWM)
      if (trigger == "boto" || trigger == "espnow") {
        ledcWrite(pwmChannel,  state ? map(brightness[3], 0, 100, 0, 255) : 0);
        ledcWrite(pwmChannel2, state ? map(brightness[3], 0, 100, 0, 255) : 0);
      }
      if (trigger == "inici") {
        ledcWrite(pwmChannel,  map(brightness[3], 0, 100, 0, 255));
        ledcWrite(pwmChannel2, map(brightness[3], 0, 100, 0, 255));
        delay(INICI_BLINK_MS);
        ledcWrite(pwmChannel,  0);
        ledcWrite(pwmChannel2, 0);
      }
      if (trigger == "wifiAP") {
        uint16_t osc = (millis() / 2) % 510;
        uint8_t bright = (osc < 255) ? osc : 510 - osc;
        ledcWrite(pwmChannel,  bright);
        ledcWrite(pwmChannel2, bright);
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

// Processa un paquet rebut i retorna el codi ACK corresponent
uint8_t handleAction(uint8_t pkt_type, uint8_t cmd,
                     uint8_t p1, uint8_t p2, uint8_t p3) {
  (void)p1; (void)p2; (void)p3;

  if (pkt_type == TYPE_EVENT) {
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

  if (pkt_type == TYPE_CMD) {
    switch (cmd) {
      case CMD_TOGGLE:               controlLlum("espnow"); return ACK_OK;
      case CMD_ON:   if (!state)     controlLlum("espnow"); return ACK_OK;
      case CMD_OFF:  if ( state)     controlLlum("espnow"); return ACK_OK;
      default:
        Serial.print("CMD no implementat: 0x"); Serial.println(cmd, HEX);
        return ACK_ERROR;
    }
  }

  return ACK_ERROR;
}

// Callback ESP-NOW: rep un paquet i prepara l'ACK si cal
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
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

  pinMode(boto_pin, INPUT);

  InitESPNow();
  esp_now_register_send_cb(OnDataSent_TRG);
  esp_now_register_recv_cb(OnDataRecv);
}


// ════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ════════════════════════════════════════════════════════════════
void loop() {
  // Envia l'ACK pendent si n'hi ha un de preparat per ESP-NOW
  blau_trg_process_pending(&_ack_pending, _ack_mac, &_ack_pkt);

  if (digitalRead(boto_pin)) {
    startTime = millis();
    Serial.println("Boto presionat");
    controlLlum("boto");

    // Espera que el botó s'alliberi o que es superi el temps per entrar al mode AP
    while (digitalRead(boto_pin)) {
      if (startTime + WIFI_AP_HOLD_MS < millis()) {
        // ── Mode AP de configuració (botó mantingut > WIFI_AP_HOLD_MS) ──
        configDeviceAP();
        wifiApModeServer();
        controlLlum("wifiAP");

        bool buttonReleased = false;
        static bool lastButtonState = HIGH;

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
          bool buttonState = digitalRead(boto_pin);

          if (buttonState == LOW && lastButtonState == HIGH && !buttonReleased) {
            buttonReleased = true;
            Serial.println("Botó alliberat");
            delay(BUTTON_RELEASE_DEBOUNCE_MS);
          }

          if (buttonState == HIGH && lastButtonState == LOW && buttonReleased) {
            Serial.println("Botó premut després d'alliberar");
            buttonReleased = false;
            ESP.restart();
          }

          lastButtonState = buttonState;
        }
      }
    }

    delay(BUTTON_DEBOUNCE_MS);
  }
}
