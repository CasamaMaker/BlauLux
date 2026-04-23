// Blautrigger, codi que controla la càrrega AC, ja sigui una bombeta, una tira de leds pwm, CW/WW, RGB, dirigible, un relé on/off
/* ESP-32 Captive portal example
 * github.com/elliotmade/ESP32-Captive-Portal-Example
 * This isn't anything new, and doesn't do anything special
 * just an example I would have appreciated while I was searching for a solution
 */


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




// DEFINICIÓ DEL DISPOSITIU
#define PICO_CLICK
// #define SONOFF_BASIC_R4


#if defined(SONOFF_BASIC_R4)
  // Pinout
  #define Boto        9   // GPIO0 (boot button)
  #define enBoto      99
  #define rele        4   // relé que activa la càrrega AC
  #define led         6   // led que segueix el relé
  #define digitalLed  99
  // Tipus de control per defecte i pins lògics associats
  #define HW_CONTROL_TYPE  0   // 0=On/Off
  #define HW_PIN1          rele
  #define HW_PIN2          led

#elif defined(PICO_CLICK)
  // Pinout
  #define Boto        5
  #define enBoto      3
  #define digitalLed  6
  #define rele        99
  #define led         99
  // Tipus de control per defecte i pins lògics associats
  #define HW_CONTROL_TYPE  1   // 1=Digital led
  #define HW_PIN1          digitalLed
  #define HW_PIN2          99

#else
  #error "Defineix una versió del dispositiu"
#endif




// #define HARDCODED_CONFIG    // Comenta per permetre configuració via web (Preferences/NVS)

// Resolució del tipus de control: constant en mode hardcoded, variable en mode web
#ifdef HARDCODED_CONFIG
  #define control_type HW_CONTROL_TYPE
  #define pin1         HW_PIN1
  #define pin2         HW_PIN2
#else
  int control_type = HW_CONTROL_TYPE;
  int pin1         = HW_PIN1;
  int pin2         = HW_PIN2;
#endif

#define idioma  "CAT"      // CAT:català (per defecte), EN:english

const char* ssid = "BlauTrigger"; //Name of the WIFI network hosted by the device
const char* password =  "";               //Password

AsyncWebServer server(80);                //This creates a web server, required in order to host a page for connected devices

DNSServer dnsServer;                      //This creates a DNS server, required for the captive portal

#define CHANNEL 1

String myAddresss, myAddresssEnd;



//****************** DIGITAL LED ******************************
#define NUM_LEDS   1
#define BRIGHTNESS 15
Adafruit_NeoPixel strip(NUM_LEDS, HW_PIN1, NEO_GRB + NEO_KHZ800);
int brightness = BRIGHTNESS;


unsigned long startTime; // Variable per emmagatzemar el temps d'inici

bool state = false;
// static uint8_t last_seq = 0xFF;  // valor inicial impossible
static volatile bool  _ack_pending = false;
static uint8_t        _ack_mac[6];
static BlauPacket_t   _ack_pkt;


int freq = 5000;       // Freqüència del senyal PWM en Hz
int pwmChannel  = 0;   // Canal PWM principal
int pwmChannel2 = 1;   // Canal PWM secundari (WW/CW)
int resolution  = 8;   // Resolució (8 bits → 0–255)

#ifndef HARDCODED_CONFIG
Preferences prefs;

void loadConfig() {
  prefs.begin("blau", true);
  control_type = prefs.getInt("ct", HW_CONTROL_TYPE);
  pin1         = prefs.getInt("p1", HW_PIN1);
  pin2         = prefs.getInt("p2", HW_PIN2);
  prefs.end();
  Serial.printf("Config carregada: ct=%d p1=%d p2=%d\n", control_type, pin1, pin2);
}

void saveConfig() {
  prefs.begin("blau", false);
  prefs.putInt("ct", control_type);
  prefs.putInt("p1", pin1);
  prefs.putInt("p2", pin2);
  prefs.end();
  Serial.printf("Config guardada: ct=%d p1=%d p2=%d\n", control_type, pin1, pin2);
}
#endif

void OnDataSent_TRG(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "ACK TX: OK" : "ACK TX: FAIL");
}

// Init ESP Now with fallback
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

void stopWebServer() {
  server.reset();          // Elimina els handlers de l’AsyncWebServer
  dnsServer.stop();        // Si també vols aturar el DNS
  LittleFS.end();          // Si està suportat
  Serial.println("Servidor i LittleFS aturats, però WiFi AP continua actiu");
}

void serveixWifiManager(AsyncWebServerRequest *request) {
  String path = "/wifimanager_" + String(idioma) + ".html";
  request->send(LittleFS, path, "text/html");
}

void webServerSetup(){
  // accedeix aquí just conectar-se a la wifi des de l'ordinador
  server.on("/", HTTP_GET, serveixWifiManager);
  // Required
	server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	// windows 11 captive portal workaround
	server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });

  // accedeix aquí just conectar-se a la wifi des del mobil android
  server.on("/generate_204", HTTP_GET, serveixWifiManager);

// .  server.on("/connecttest.txt", HTTP_GET, serveixWifiManager);
  server.on("/ncsi.txt", HTTP_GET, serveixWifiManager);
  
  // Rutas adicionales para engañar a Windows
  server.on("/hotspot-detect.html", HTTP_GET, serveixWifiManager);
  server.on("/library/test/success.html", HTTP_GET, serveixWifiManager);
  server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox captive portal call home
  server.on("/redirect", HTTP_GET, serveixWifiManager);
  server.on("/fwlink", HTTP_GET, serveixWifiManager);
  server.on("/cdn-cgi/", HTTP_GET, serveixWifiManager);
  server.on("/canonical.html", HTTP_GET, serveixWifiManager);

  // return 404 to webpage icon
	server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });	// webpage icon

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
    Serial.println("Served CSS");
  });

  server.on("/driverMode", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", String(control_type));
      Serial.println(control_type);
  });

  server.on("/configMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    #ifdef HARDCODED_CONFIG
      request->send(200, "text/plain", "hardcoded");
    #else
      request->send(200, "text/plain", "web");
    #endif
  });

  server.on("/mymac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", WiFi.softAPmacAddress()); //String(mac).c_str());
    Serial.println(myAddresss);
  });

  server.on("/pins", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"pin1\":" + String(pin1) + ",\"pin2\":" + String(pin2) + "}";
    request->send(200, "application/json", json);
    Serial.println(json);
  });

  // reb i guarda la configuració des de la web
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    #ifndef HARDCODED_CONFIG
      int params = request->params();
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          if      (p->name() == "control_type") control_type = p->value().toInt();
          else if (p->name() == "pin1")         pin1         = p->value().toInt();
          else if (p->name() == "pin2")         pin2         = p->value().toInt();
        }
      }
      saveConfig();
    #endif
    request->send(200, "text/plain", "Configurat!");
    ESP.restart();
  });

  server.on("/color", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("r", true) && request->hasParam("g", true) && request->hasParam("b", true)) {
      int r = request->getParam("r", true)->value().toInt();
      int g = request->getParam("g", true)->value().toInt();
      int b = request->getParam("b", true)->value().toInt();
      strip.setPixelColor(0, strip.Color(r, g, b));
      strip.show();
      Serial.printf("Color: rgb(%d,%d,%d)\n", r, g, b);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/dutty", HTTP_POST, [](AsyncWebServerRequest *request){
    String dutyValue = "";
    
    if (request->hasParam("value", true)) {
      dutyValue = request->getParam("value", true)->value();
    }
    
    int duty = dutyValue.toInt();
    
    // Aquí aplicas el duty cycle a tu PWM
    // Por ejemplo: ledcWrite(channel, map(duty, 0, 100, 0, 255));
    
    Serial.print("Duty recibido: ");
    Serial.println(duty);

    brightness = duty;
    strip.setBrightness(map(brightness, 0, 100, 0, 255));
    strip.show();
    
    request->send(200, "text/plain", "OK");
  });


  // accedeix aquí quan busques qualsevol web al navegador
  server.onNotFound(serveixWifiManager);
  // // IMPORTANTE: Configurar el manejador para solicitudes no encontradas
  // server.onNotFound([](AsyncWebServerRequest *request){
  //   // Esto es crucial para el portal cautivo
  //   request->redirect("http://" + WiFi.softAPIP().toString());
  // });

  server.begin();                         //Starts the server process
  Serial.println("Web server started");
}

void getMyMacAddress() {
  Serial.print("MAC del microcontrolador: ");

  myAddresss = WiFi.softAPmacAddress();    //retorna la MAC de la interfície WiFi AP (access point)
  myAddresss.replace(":", "");
  Serial.print("La meva adreça MAC (ap)"); Serial.println(myAddresss);
  myAddresssEnd = myAddresss.substring(myAddresss.length() - 4);
}

/**
 * @brief Configura l'equip en mode Wifi Access Point
 */

void noWifi(){
  // ✅ Solo WiFi para ESP-NOW, sin AP
  WiFi.mode(WIFI_STA);  // Modo Station sin conectar
  // WiFi.disconnect();
}
void configDeviceAP() {
  WiFi.mode(WIFI_AP);

  getMyMacAddress();

  // const char *SSID = "Slave_1";
  String fullSSID = String(ssid) + "_" + myAddresssEnd;
  bool result = WiFi.softAP(fullSSID, "");//, CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(fullSSID));
    Serial.print("AP CHANNEL "); Serial.println(WiFi.channel());
    Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  }
}

void configuracioLlum() {
  switch (control_type) {
    case 0:  // On/Off
      pinMode(pin1, OUTPUT);    // rele
      pinMode(pin2, OUTPUT);    // led
      break;

    case 1:  // Digital led
      strip = Adafruit_NeoPixel(NUM_LEDS, pin1, NEO_GRB + NEO_KHZ800);
      strip.begin();
      strip.clear();
      strip.show();
      break;

    case 2:  // Led Dimmer [PWM]
      ledcSetup(pwmChannel, freq, resolution);
      ledcAttachPin(pin1, pwmChannel);
      ledcWrite(pwmChannel, map(brightness, 0, 100, 0, 255));
      break;

    case 3:  // Leds WW/CW [2×PWM]
      ledcSetup(pwmChannel,  freq, resolution);
      ledcSetup(pwmChannel2, freq, resolution);
      ledcAttachPin(pin1, pwmChannel);
      ledcAttachPin(pin2, pwmChannel2);
      break;

    default:
      Serial.println("Mode desconegut");
      break;
  }
}

void controlLlum(String trigger) {
  switch (control_type) {
    case 0:  // On/Off
      if (trigger == "boto" || trigger == "espnow"){
        digitalWrite(pin1, state ? HIGH : LOW);   // rele
        digitalWrite(pin2, state ? HIGH : LOW);   // led
      }
      if (trigger == "inici") {
        digitalWrite(pin2, HIGH);   // led
        delay(500);
        digitalWrite(pin2, LOW);   // led
      }
      break;

    case 1:  // Digital led
      strip.setBrightness(map(brightness, 0, 100, 0, 255));
      if (trigger == "boto")   strip.setPixelColor(0, state ? strip.Color(0,0,0) : strip.Color(0,0,255));
      if (trigger == "espnow") strip.setPixelColor(0, state ? strip.Color(0,0,0) : strip.Color(255,0,0));
      if (trigger == "inici") {
        strip.setPixelColor(0, strip.Color(255,255,0));
        strip.show();
        delay(500);
        strip.clear();
      }
      strip.show();
      break;

    case 2:  // Led Dimmer [PWM]
      if (trigger == "boto" || trigger == "espnow"){
        ledcWrite(pwmChannel, state ? map(brightness, 0, 100, 0, 255) : 0);
      }
      if (trigger == "inici") {
        ledcWrite(pwmChannel, map(brightness, 0, 100, 0, 255));
        delay(500);
        ledcWrite(pwmChannel, 0);
      }
      break;

    case 3:  // Leds WW/CW [2×PWM]
      if (trigger == "boto" || trigger == "espnow"){
        ledcWrite(pwmChannel,  state ? map(brightness, 0, 100, 0, 255) : 0);
        ledcWrite(pwmChannel2, state ? map(brightness, 0, 100, 0, 255) : 0);
      }
      if (trigger == "inici") {
        ledcWrite(pwmChannel,  map(brightness, 0, 100, 0, 255));
        ledcWrite(pwmChannel2, map(brightness, 0, 100, 0, 255));
        delay(500);
        ledcWrite(pwmChannel,  0);
        ledcWrite(pwmChannel2, 0);
      }
      break;

    default:
      Serial.println("Mode desconegut");
      break;
  }
  state = !state;
}

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
      case CMD_TOGGLE:                       controlLlum("espnow"); return ACK_OK;
      case CMD_ON:   if (!state)             controlLlum("espnow"); return ACK_OK;
      case CMD_OFF:  if ( state)             controlLlum("espnow"); return ACK_OK;
      default:
        Serial.print("CMD no implementat: 0x"); Serial.println(cmd, HEX);
        return ACK_ERROR;
    }
  }
  return ACK_ERROR;
}

void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  blau_trg_on_data_recv(mac, data, len,
                        &_ack_pending, _ack_mac, &_ack_pkt,
                        handleAction,
                        state, (uint8_t)brightness, (uint8_t)control_type);
}

void wifiApModeServer(){
  
  if (!LittleFS.begin()) return Serial.println("Error muntant LittleFS"), void();   // Init. file system
  
  Serial.println("Wifi initialized");
  Serial.println(WiFi.softAPIP());                          //Print out the IP address
  
  dnsServer.start(53, "*", WiFi.softAPIP());                //This starts the DNS server.  The "*" sends any request for port 53 straight to the IP address of the device
  
  webServerSetup();                                         //Configures the behavior of the web server
  Serial.println("Setup complete");
}

void setup() {

  Serial.begin(115200);

  #ifndef HARDCODED_CONFIG
  loadConfig();
  #endif

  configuracioLlum();     // configuració el control de la llum
  controlLlum("inici");

  // delay(1000);

  configDeviceAP();       // Configuració de l'equip en mode Wifi Acces Point
  // noWifi();

  //Pinout configuration
  pinMode(Boto, INPUT);

  // Init ESPNow with a fallback logic
  InitESPNow();

  esp_now_register_send_cb(OnDataSent_TRG);
  esp_now_register_recv_cb(OnDataRecv);

}

void loop() {

  blau_trg_process_pending(&_ack_pending, _ack_mac, &_ack_pkt);

  if(digitalRead(Boto)){
    startTime = millis(); // Guarda el temps actual en mil·lisegons al iniciar

    // Acció al missatge rebut
    Serial.println("Boto presionat");
    controlLlum("boto");
    // leds[0] = state ? CRGB::Black : CRGB::Blue;
    // FastLED.show();
    // state = !state;


    while(digitalRead(Boto)){
      if(startTime + 3000 < millis()){
        strip.setPixelColor(0, strip.Color(0,255,0));
        strip.show();

        configDeviceAP();       // Configuració de l'equip en mode Wifi Acces Point
        
        wifiApModeServer();
        bool buttonStateLow1=false;
        bool buttonStateHigh2=false;
        bool buttonReleased = false;
        while(1){
          dnsServer.processNextRequest();     //requisit dns constant
          delay(100);

          if(startTime + 60000 < millis()){         // s'apaga l'equip després de 60 segons
            Serial.println("Temps excedit");

            // stopWebServer();
            // delay(200);
            // leds[0] = CRGB::Black;
            // FastLED.show();
            // break;
            ESP.restart();
          }

          // seqüència per detectar que el boto es deixa de presionar i es torna a presionar, per apagar l'equip
          static bool lastButtonState = HIGH;       // Estat anterior del botó
          bool buttonState = digitalRead(Boto);     // Llegeix el botó

          if (buttonState == LOW && lastButtonState == HIGH && !buttonReleased) {
              buttonReleased = true;                // Marquem que s'ha alliberat el botó
              Serial.println("Botó alliberat");
              delay(200);
          }

          if (buttonState == HIGH && lastButtonState == LOW && buttonReleased) {
              Serial.println("Botó premut després d'alliberar");
              buttonReleased = false;                 // Reiniciem per detectar una nova seqüència
              
              // stopWebServer();
              // delay(200);
              // leds[0] = CRGB::Black;
              // FastLED.show();
              // break;
              ESP.restart();

          }
          lastButtonState = buttonState;              // Guardem l'estat per a la següent iteració
        }
      }
      // noWifi();
    }
    delay(500);
  }
}