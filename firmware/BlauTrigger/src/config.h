#pragma once

// ════════════════════════════════════════════════════════════════
//  SELECCIÓ DEL DISPOSITIU  (descomenta un sol)
// ════════════════════════════════════════════════════════════════
#define PICO_CLICK
// #define SONOFF_BASIC_R4


// ════════════════════════════════════════════════════════════════
//  PINOUT I CONFIGURACIÓ DE HARDWARE
// ════════════════════════════════════════════════════════════════
#define PIN_UNUSED  -1   // pin no connectat / no utilitzat

#if defined(SONOFF_BASIC_R4)
  #define PIN_BOTO        9   // GPIO0 (boot button)
  #define PIN_RELE        4   // relé que activa la càrrega AC
  #define PIN_LED         6   // led que segueix el relé
  #define PIN_DIGITAL_LED PIN_UNUSED
  #define HW_CONTROL_TYPE 0   // 0=On/Off
  #define HW_PIN1         PIN_RELE
  #define HW_PIN2         PIN_LED
  #define HW_PIN3         PIN_UNUSED
  #define BUTTON_PULLUP   1   // 1=pull-up (premut=LOW), 0=pull-down (premut=HIGH)

#elif defined(PICO_CLICK)
  #define PIN_BOTO        5
  #define PIN_DIGITAL_LED 6
  #define PIN_RELE        PIN_UNUSED
  #define PIN_LED         PIN_UNUSED
  #define HW_CONTROL_TYPE 1   // 1=Digital led
  #define HW_PIN1         PIN_DIGITAL_LED
  #define HW_PIN2         PIN_UNUSED
  #define HW_PIN3         PIN_UNUSED
  #define BUTTON_PULLUP   0   // 1=pull-up (premut=LOW), 0=pull-down (premut=HIGH)

#else
  #error "Defineix una versió del dispositiu a config.h"
#endif


// ════════════════════════════════════════════════════════════════
//  MODE DE CONFIGURACIÓ
//  · Comentat  → configuració via web (es guarda a NVS/Preferences)
//  · Descomentat → tots els paràmetres de hardware estan fixats
//                  al codi i la web és de només lectura
// ════════════════════════════════════════════════════════════════
// #define HARDCODED_CONFIG   // comenta per desactivar

// ════════════════════════════════════════════════════════════════
//  ESBORRA CONFIG  (descomenta per esborrar les Preferences de NVS)
//  · Comentat    → comportament normal
//  · Descomentat → esborra tota la config guardada a l'inici
//                  (torna a comentar i repuja el firmware després)
// ════════════════════════════════════════════════════════════════
// #define CLEAR_CONFIG       // comenta per desactivar


// ════════════════════════════════════════════════════════════════
//  WIFI  (Access Point del portal de configuració)
// ════════════════════════════════════════════════════════════════
#define WIFI_SSID      "BlauTrigger"
#define WIFI_PASSWORD  ""


// ════════════════════════════════════════════════════════════════
//  FUNCIONALITATS OPCIONALS
//  · Descomentat (per defecte) → funcionalitat activa
//  · Comentat                  → funcionalitat desactivada
// ════════════════════════════════════════════════════════════════
#define ENABLE_WIFI_STA   // connexió WiFi a xarxa domèstica; comenta per desactivar
#define ENABLE_MQTT       // client MQTT; comenta per desactivar (requereix ENABLE_WIFI_STA)


// ════════════════════════════════════════════════════════════════
//  WIFI STA  (xarxa domèstica — actiu quan HARDCODED_CONFIG)
//  En mode web, les credencials es guarden via el portal (NVS).
// ════════════════════════════════════════════════════════════════
#define HC_STA_SSID   ""     // SSID del router
#define HC_STA_PASS   ""     // contrasenya del router


// ════════════════════════════════════════════════════════════════
//  MQTT  (broker — actiu quan HARDCODED_CONFIG)
//  En mode web, la configuració es guarda via el portal (NVS).
//  Wildcards: %id% → últims 4 caràcters de la MAC  |  %topic% → valor resolt de HC_MQTT_TOPIC
// ════════════════════════════════════════════════════════════════
#define HC_MQTT_HOST      ""                     // adreça del broker (ex: "192.168.1.100")
#define HC_MQTT_PORT      1883                   // port MQTT
#define HC_MQTT_USER      ""                     // usuari (buit = sense autenticació)
#define HC_MQTT_PASS      ""                     // contrasenya
#define HC_MQTT_CLIENT    "BlauTrigger_%id%"     // client ID  (ex: BlauTrigger_A1B2)
#define HC_MQTT_TOPIC     "%id%"                 // topic curt del dispositiu  (ex: A1B2)
#define HC_MQTT_FULLTOPIC "blautrigger/%topic%"  // prefix complet dels topics (ex: blautrigger/A1B2)


// ════════════════════════════════════════════════════════════════
//  LED DIGITAL (NeoPixel / WS2812)
// ════════════════════════════════════════════════════════════════
#define NUM_LEDS        1
#define BRIGHTNESS_DEF  15    // brillantor per defecte (0–100)

//  Paleta de colors  — format 0xRRGGBB
//  Escriu el nom directament a les assignacions de sota
#define vermell   0xFF0000
#define verd      0x00FF00
#define blau      0x0000FF
#define groc      0xFFFF00
#define taronja   0xFF8000
#define blanc     0xFFFFFF
#define negre     0x000000
#define lila      0x800080
#define rosa      0xFF00FF
#define cian      0x00FFFF

//  Colors de cada acció
#define COLOR_BOTO      blau
#define COLOR_ESPNOW    vermell
#define COLOR_INICI     verd
#define COLOR_WIFI_AP   lila
#define COLOR_MQTT      groc


// ════════════════════════════════════════════════════════════════
//  PWM (Led Dimmer i WW/CW)
// ════════════════════════════════════════════════════════════════
#define PWM_FREQ        5000   // freqüència Hz
#define PWM_RESOLUTION  8      // bits (8 → rang 0–255)


// ════════════════════════════════════════════════════════════════
//  CONTROL DE FASE (Triac + ZCD)
// ════════════════════════════════════════════════════════════════
#define TRIAC_PULSE_US       100   // durada del pols de dispar del triac (µs)
#define AC_HALF_CYCLE_US   10000   // semiperíode de 50 Hz (µs)
#define TRIAC_MIN_DELAY_US   500   // retard mínim de dispar després del ZCD (µs)


// ════════════════════════════════════════════════════════════════
//  SISTEMA
// ════════════════════════════════════════════════════════════════
#define SERIAL_BAUD            115200  // velocitat del port sèrie
#define WIFI_AP_HOLD_MS          3000  // ms prement el botó per entrar al mode AP
#define WIFI_AP_TIMEOUT_MS     120000  // ms màxims en mode AP abans de reiniciar
#define HTTP_PORT                  80  // port del servidor web
#define DNS_PORT                   53  // port del servidor DNS
#define DNS_POLL_MS               100  // interval de polling DNS al bucle AP
#define ESPNOW_CHANNEL              1  // canal Wi-Fi per a ESP-NOW
#define INICI_BLINK_MS            500  // durada del parpelleig d'inici
#define BUTTON_DEBOUNCE_MS        500  // debounce després de clic de botó
#define BUTTON_RELEASE_DEBOUNCE_MS 200 // debounce detecció alliberament de botó
