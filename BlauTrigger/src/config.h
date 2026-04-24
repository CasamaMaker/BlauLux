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
  #define PIN_EN_BOTO     PIN_UNUSED
  #define PIN_RELE        4   // relé que activa la càrrega AC
  #define PIN_LED         6   // led que segueix el relé
  #define PIN_DIGITAL_LED PIN_UNUSED
  #define HW_CONTROL_TYPE 0   // 0=On/Off
  #define HW_PIN1         PIN_RELE
  #define HW_PIN2         PIN_LED

#elif defined(PICO_CLICK)
  #define PIN_BOTO        5
  #define PIN_EN_BOTO     3
  #define PIN_DIGITAL_LED 6
  #define PIN_RELE        PIN_UNUSED
  #define PIN_LED         PIN_UNUSED
  #define HW_CONTROL_TYPE 1   // 1=Digital led
  #define HW_PIN1         PIN_DIGITAL_LED
  #define HW_PIN2         PIN_UNUSED

#else
  #error "Defineix una versió del dispositiu a config.h"
#endif


// ════════════════════════════════════════════════════════════════
//  MODE DE CONFIGURACIÓ
//  · Comentat  → configuració via web (es guarda a NVS/Preferences)
//  · Descomentat → tots els paràmetres de hardware estan fixats
//                  al codi i la web és de només lectura
// ════════════════════════════════════════════════════════════════
// #define HARDCODED_CONFIG

// ════════════════════════════════════════════════════════════════
//  ESBORRA CONFIG  (descomenta per esborrar les Preferences de NVS)
//  · Comentat    → comportament normal
//  · Descomentat → esborra tota la config guardada a l'inici
//                  (torna a comentar i repuja el firmware després)
// ════════════════════════════════════════════════════════════════
// #define CLEAR_CONFIG


// ════════════════════════════════════════════════════════════════
//  IDIOMA DE LA WEB  (CAT | EN)
// ════════════════════════════════════════════════════════════════
#define IDIOMA  "CAT"


// ════════════════════════════════════════════════════════════════
//  WIFI  (Access Point del portal de configuració)
// ════════════════════════════════════════════════════════════════
#define WIFI_SSID      "BlauTrigger"
#define WIFI_PASSWORD  ""


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


// ════════════════════════════════════════════════════════════════
//  PWM (Led Dimmer i WW/CW)
// ════════════════════════════════════════════════════════════════
#define PWM_FREQ        5000   // freqüència Hz
#define PWM_RESOLUTION  8      // bits (8 → rang 0–255)


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
