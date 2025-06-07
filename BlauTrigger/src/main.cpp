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

// #include <wifimanager.h>
// #include "SPIFFS.h"
// #include "spiffs.h"

#include <esp_sleep.h>
#include <EEPROM.h>
#include <esp_now.h>
#include <FastLED.h>

                              //  V1  | V2  | Pico-Click
#define enVBatterySense 0     //  4   | 0   | -   [no implementat encara]
#define VbatSense 3           //  3   | 3   | 4   [no implementat encara]
#define Boto 5                //  5   | 1   | 5
#define enBoto 3              //  -   | 4   | 3   [-: deepsleep mode (variable=99), n: pin mode]
#define digitalLed 6          //  6   | 5   | 6

#define idioma  "CAT"      // CAT:català (per defecte), EN:english

const char* ssid = "BlauTrigger-AP"; //Name of the WIFI network hosted by the device
const char* password =  "";               //Password


#define CHANNEL 1
AsyncWebServer server(80);                //This creates a web server, required in order to host a page for connected devices


String myAddresss, myAddresssEnd;


//****************** DIGITAL LED ******************************
#define NUM_LEDS 1
#define DATA_PIN digitalLed //6
#define BRIGHTNESS  15
CRGB leds[NUM_LEDS];


unsigned long startTime; // Variable per emmagatzemar el temps d'inici

// Structure example to send data
typedef struct {
  char topic[50];
  char payload[50];
} struct_message;

// Create a struct_message called missatge
struct_message missatge;

bool state = false;

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

void getMyMacAddress() {
  Serial.print("MAC del microcontrolador: ");

  myAddresss = WiFi.softAPmacAddress();    //retorna la MAC de la interfície WiFi AP (access point)
  myAddresss.replace(":", "");
  Serial.print("La meva adreça MAC (ap)"); Serial.println(myAddresss);
  myAddresssEnd = myAddresss.substring(myAddresss.length() - 4);
}

// config AP SSID
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

void OnDataRecv(const uint8_t * mac, const uint8_t *data, int len) {
  
  // Missatge rebut
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);

  const struct_message* msg = (const struct_message*) data;
  Serial.print("Topic: "); Serial.println(msg->topic);
  Serial.print("Payload: "); Serial.println(msg->payload);
  Serial.println();


  // Acció al missatge rebut
  leds[0] = state ? CRGB::Red : CRGB::Black;
  FastLED.show();
  state = !state;
}


void setup() {
  startTime = millis(); // Guarda el temps actual en mil·lisegons al iniciar
  Serial.begin(115200);

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(1000);

  // configure device AP mode
  configDeviceAP();
  
  // Init ESPNow with a fallback logic
  InitESPNow();

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_recv_cb(OnDataRecv);
  // delay(1000);

  leds[0] = CRGB::Black;
  FastLED.show();
}


void loop() {
 
}