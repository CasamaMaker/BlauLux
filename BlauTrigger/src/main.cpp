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
#include <EEPROM.h>
#include <esp_now.h>
#include <FastLED.h>
#include <blauprotocol.h>
#include <blauprotocol_trg.h>




// #define BLAULINK_V1
// #define BLAULINK_V2
// #define PICO_CLICK
// #define FIRSTRELE
// #define S3ZERO
#define SONOFF_BASIC_R4

#if defined(BLAULINK_V1)
  #define enVBatterySense 4
  #define VbatSense 3
  #define Boto 5
  #define enBoto 99  // 99 per indicar no disponible o mode deepsleep
  #define digitalLed 6

#elif defined(BLAULINK_V2)
  #define enVBatterySense 0
  #define VbatSense 3
  #define Boto 1
  #define enBoto 4
  #define digitalLed 5

#elif defined(PICO_CLICK)
  #define enVBatterySense 99  // No implementat
  #define VbatSense 4
  #define Boto 5
  #define enBoto 3
  #define digitalLed 6

#elif defined(YEELIGHT_LAMP)
  #define enVBatterySense 99  // No implementat
  #define VbatSense 4
  #define Boto 5
  #define enBoto 3
  #define digitalLed 19

#elif defined(FIRSTRELE)
  #define enVBatterySense 99  // No implementat
  #define VbatSense 99
  #define Boto 0
  #define enBoto 99
  #define digitalLed 13

#elif defined(S3ZERO)
  #define enVBatterySense 99  // No implementat
  #define VbatSense 99
  #define Boto 0    // GPIO0 (boot button)
  #define enBoto 99
  #define digitalLed 21   // WS2812 integrat a l'S3-Zero
#elif defined(SONOFF_BASIC_R4)
  #define enVBatterySense 99  // No implementat
  #define VbatSense 99
  #define Boto 9    // GPIO0 (boot button)
  #define enBoto 99
  #define digitalLed 21   // WS2812 integrat a l'S3-Zero
  #define rele 4
  #define led 6

#else
  #error "Defineix una versió del dispositiu (BLAULINK_V1, BLAULINK_V2 o PICO_CLICK)"
#endif




#define idioma  "CAT"      // CAT:català (per defecte), EN:english

const char* ssid = "BlauTrigger"; //Name of the WIFI network hosted by the device
const char* password =  "";               //Password

AsyncWebServer server(80);                //This creates a web server, required in order to host a page for connected devices

DNSServer dnsServer;                      //This creates a DNS server, required for the captive portal

#define CHANNEL 1

int control_type, pin1, pin2;

String myAddresss, myAddresssEnd;



//****************** DIGITAL LED ******************************
#define NUM_LEDS 1
#define DATA_PIN digitalLed //6
#define BRIGHTNESS  15
CRGB leds[NUM_LEDS];
int brightness = BRIGHTNESS;


unsigned long startTime; // Variable per emmagatzemar el temps d'inici

// // Structure example to send data
// typedef struct {
//   char topic[50];
//   char payload[50];
// } struct_message;

// // Create a struct_message called missatge
// struct_message missatge;

bool state = false;
// static uint8_t last_seq = 0xFF;  // valor inicial impossible
static volatile bool  _ack_pending = false;
static uint8_t        _ack_mac[6];
static BlauPacket_t   _ack_pkt;


int freq = 5000;      // Freqüència del senyal PWM en Hz
int pwmChannel = 0;   // Canal PWM (0–7 per ESP32-C3)
int resolution = 8;   // Resolució (8 bits → valors de 0 a 255) 

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
      request->send(200, "text/plain", String(control_type)); //String(mac).c_str());
      Serial.println(control_type);
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

  // reb les variables des de la web
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0;i<params;i++){
      const AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        if (p->name() == "control_type") {
          String buf = p->value().c_str();
          Serial.print("control_type: ");
          Serial.println(buf);
          control_type=buf.toInt();
          // guardar EEPROM
        }
        if (p->name() == "pin1") {
          String buf = p->value().c_str();
          Serial.print("Pin1: ");
          Serial.println(buf);
          pin1=buf.toInt();
          // guardar EEPROM
        }
        if (p->name() == "pin2") {
          String buf = p->value().c_str();
          Serial.print("Pin2: ");
          Serial.println(buf);
          pin2=buf.toInt();
          // guardar EEPROM
        }
      }
    }
    request->send(200, "text/plain", "Configurat! Ja pots prova");
    // delay(1000);
    // stopWebServer();
    // delay(200);
    // leds[0] = CRGB::Black;
    // FastLED.show();
    ESP.restart();

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
    FastLED.setBrightness(map(brightness, 0, 100, 0, 255));
    FastLED.show();
    
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

void configuracioLlum(){
  // llegir EEPROM: control_type, pin1 i pin2
  switch(control_type){
    case 0:     // on/off                     ----> topic: light  &  payload: "toogle"
      // digitalWrite(19, state);
      pinMode(rele, OUTPUT);  //rele
      pinMode(led, OUTPUT);  //led
      break;
    
    case 1:     // Digital led                ----> topic: digled  &  payload: color
      FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
      leds[0] = CRGB::Black;
      FastLED.show();
      break;

    case 2:{     // Led Dimmer [PWM]           ----> topic: dimmer  &  payload: %255
      // Defineix els paràmetres PWM
      // int pwmPin = 4;       // El pin GPIO on vols treure el PWM
      // int freq = 5000;      // Freqüència del senyal PWM en Hz
      // int pwmChannel = 0;   // Canal PWM (0–7 per ESP32-C3)
      // int resolution = 8;   // Resolució (8 bits → valors de 0 a 255) 
      // Configura el canal PWM
      ledcSetup(pwmChannel, freq, resolution);
      
      // Assigna el canal al pin
      ledcAttachPin(pin1, pwmChannel);

      // Escriu un valor PWM (0 a 255 si resolution = 8)
      int valor = (int)pow(2, 8)*50/100;
      ledcWrite(pwmChannel, valor);  // 128:50% de cicle de treball
      break;
    }

    case 3:     // Leds WW/CW  [2*PWM]        ----> topic: wwcw  &  payload: %255 & %255
      Serial.println("3333");
      break;

    default:
      Serial.println("Mode desconegut");
      break;
  }
}

void controlLlum(String trigger){
  switch(control_type){
    case 0:     // on/off
      // digitalWrite(pin1, state);
      // analogWrite(19, map(100, 0, 100, 0, 255));
      // delay(1000);
      // analogWrite(19, map(50, 0, 100, 0, 255));
      digitalWrite(rele, state ? HIGH : LOW);
      digitalWrite(led, state ? HIGH : LOW);
      break;

    case 1:     // Digital led
      FastLED.setBrightness(map(brightness, 0, 100, 0, 255));
      if(trigger == "boto") leds[0] = state ? CRGB::Black : CRGB::Blue;
      if(trigger == "espnow") leds[0] = state ? CRGB::Black : CRGB::Red;
      if(trigger == "inici"){
        leds[0] = CRGB::Yellow;
        delay(500);
        leds[0] = CRGB::Black;
      }
      FastLED.show();
      break;

    case 2:     // Led Dimmer [PWM]
      // analogWrite(pin1, state ? map(brightness, 0, 100, 0, 255) : 0);
      break;

    case 3:     // Leds WW/CW  [2*PWM]
      break;

    default:
      Serial.println("Mode desconegut");
      break;
  }
  state = !state;
}

// void OnDataRecv(const uint8_t * mac, const uint8_t *data, int len) {
  
//   // Missatge rebut
//   char macStr[18];
//   snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
//            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
//   Serial.print("Last Packet Recv from: "); Serial.println(macStr);

//   const struct_message* msg = (const struct_message*) data;
//   Serial.print("Topic: "); Serial.println(msg->topic);
//   Serial.print("Payload: "); Serial.println(msg->payload);
//   Serial.println();


//   // Acció al missatge rebut
//   controlLlum("espnow");
//   // leds[0] = state ? CRGB::Black : CRGB::Red;
//   // FastLED.show();
//   // state = !state;
// }

void printPacket(const BlauPacket_t* pkt) {
  Serial.println("---- PACKET ----");
  Serial.printf("version: 0x%02X\n", pkt->version);
  Serial.printf("type:    0x%02X\n", pkt->type);
  Serial.printf("seq:     %d\n", pkt->seq);
  Serial.printf("cmd:     0x%02X\n", pkt->cmd);
  Serial.printf("p1:      0x%02X\n", pkt->p1);
  Serial.printf("p2:      0x%02X\n", pkt->p2);
  Serial.printf("p3:      0x%02X\n", pkt->p3);
  Serial.printf("src_id:  0x%04X\n", pkt->src_id);
  Serial.printf("crc:     0x%02X\n", pkt->crc8);
  Serial.println("----------------");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  // Serial.print("len: ");
  // Serial.println(len);

  // Serial.print("mac: ");
  // for (int i = 0; i < 6; i++) {
  //   Serial.printf("%02X", mac[i]);
  //   if (i < 5) Serial.print(":");
  // }
  // Serial.println();

  // printPacket((BlauPacket_t*)data);

  // Serial.println();
  BlauPacket_t pkt;
  if (!blau_parse_packet(data, len, &pkt)) {
    Serial.println("Paquet invàlid (mida, CRC o versió)");
    return;
  }

  Serial.print("Paquet rebut: type=0x"); Serial.print(pkt.type, HEX);
  Serial.print(" cmd=0x");  Serial.print(pkt.cmd, HEX);
  Serial.print(" seq=");    Serial.println(pkt.seq);

  if (pkt.type == TYPE_EVENT && pkt.cmd == EVT_CLICK_1) {

    bool dup = blau_is_duplicate(pkt.src_id, pkt.seq);

    if (!dup) {
      controlLlum("espnow");                         // acció: 1 sola vegada
    } else {
      Serial.println("Duplicat ignorat");
    }
    // Serial.println(mac);
    // Peer: registrar sempre (necessari per respondre)
    // if (!esp_now_is_peer_exist(mac)) {
    //   esp_now_peer_info_t peerInfo = {};
    //   memcpy(peerInfo.peer_addr, mac, 6);
    //   peerInfo.channel = 0;
    //   peerInfo.encrypt = false;
    //   esp_now_add_peer(&peerInfo);
    // }

    // ACK sempre — BlauLink necessita saber que ha arribat, sigui duplicat o no
    // BlauPacket_t ack;
    // blau_build_ack(&ack, pkt.seq, dup ? ACK_DUPLICATE : ACK_OK);
    // esp_now_send(mac, (uint8_t*)&ack, sizeof(ack));
    // Serial.print("ACK enviat (");
    // Serial.print(dup ? "DUPLICATE" : "OK");
    // Serial.print(") seq="); Serial.println(pkt.seq);
    memcpy(_ack_mac, mac, 6);
    blau_build_ack(&_ack_pkt, pkt.seq, dup ? ACK_DUPLICATE : ACK_OK);
    _ack_pending = true;
    Serial.print("ACK pendent seq="); Serial.println(pkt.seq);
  }
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

  Serial.begin(115200);   // Inicialització port sèrie

  control_type=0;
  configuracioLlum();     // configuració el control de la llum
  controlLlum("inici");

  // delay(1000);

  configDeviceAP();       // Configuració de l'equip en mode Wifi Acces Point
  // noWifi();

  //Pinout configuration
  pinMode(Boto, INPUT);

  // Init ESPNow with a fallback logic
  InitESPNow();

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  // esp_now_register_recv_cb(OnDataRecv);

  esp_now_register_send_cb(OnDataSent_TRG);
  esp_now_register_recv_cb(OnDataRecv);
  // delay(1000);

  // leds[0] = CRGB::Black;
  // FastLED.show();
  

  
  // pin1=23;
  // pin2=44;
}

void loop() {

  // static unsigned long lastTick = 0;
  // if (millis() - lastTick > 3000) {
  //   Serial.printf("[TRG] loop viu, _ack_pending=%d\n", (int)_ack_pending);
  //   lastTick = millis();
  // }

  if (_ack_pending) {
    _ack_pending = false;
    if (!esp_now_is_peer_exist(_ack_mac)) {
      esp_now_peer_info_t p = {};
      memcpy(p.peer_addr, _ack_mac, 6);
      p.channel = 0;
      p.encrypt = false;
      p.ifidx = WIFI_IF_AP;     // ← BlauTrigger és en WIFI_AP mode
      esp_err_t addResult = esp_now_add_peer(&p);
      Serial.print("add_peer: 0x"); Serial.println(addResult, HEX);
    }
    esp_err_t r = esp_now_send(_ack_mac, (uint8_t*)&_ack_pkt, sizeof(_ack_pkt));
    Serial.print("ACK esp_now_send: 0x"); Serial.println(r, HEX);
  }

  if(!digitalRead(Boto)){
    startTime = millis(); // Guarda el temps actual en mil·lisegons al iniciar

    // Acció al missatge rebut
    Serial.println("Boto presionat");
    controlLlum("boto");
    // leds[0] = state ? CRGB::Black : CRGB::Blue;
    // FastLED.show();
    // state = !state;


    while(!digitalRead(Boto)){
      if(startTime + 3000 < millis()){
        leds[0] = CRGB::Green;
        FastLED.show();

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