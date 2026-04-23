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
  #define Boto        9   // GPIO0 (boot button)
  #define enBoto      PIN_UNUSED
  #define rele        4   // relé que activa la càrrega AC
  #define led         6   // led que segueix el relé
  #define digitalLed  PIN_UNUSED
  #define HW_CONTROL_TYPE  0   // 0=On/Off
  #define HW_PIN1          rele
  #define HW_PIN2          led

#elif defined(PICO_CLICK)
  #define Boto        5
  #define enBoto      3
  #define digitalLed  6
  #define rele        PIN_UNUSED
  #define led         PIN_UNUSED
  #define HW_CONTROL_TYPE  1   // 1=Digital led
  #define HW_PIN1          digitalLed
  #define HW_PIN2          PIN_UNUSED

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
#define COLOR_INICI     groc
#define COLOR_WIFI_AP   verd


// ════════════════════════════════════════════════════════════════
//  PWM (Led Dimmer i WW/CW)
// ════════════════════════════════════════════════════════════════
#define PWM_FREQ        5000   // freqüència Hz
#define PWM_RESOLUTION  8      // bits (8 → rang 0–255)
