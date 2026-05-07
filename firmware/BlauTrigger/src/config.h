#pragma once

// ════════════════════════════════════════════════════════════════
//  PINOUT I CONFIGURACIÓ DE HARDWARE
// ════════════════════════════════════════════════════════════════
#define PIN_UNUSED  -1   // pin no connectat / no utilitzat


// ════════════════════════════════════════════════════════════════
//  REGISTRE DE FUNCIONS GPIO  (estil Tasmota)
// ════════════════════════════════════════════════════════════════
enum GpioFunc : uint8_t {
  FUNC_NONE = 0,
  FUNC_BTN,        // 1  — Botó digital entrada (pull-up, premut=LOW)
  FUNC_BTN_INV,    // 2  — Botó invers (pull-down, premut=HIGH)
  FUNC_RELAY,      // 3  — Relé sortida digital
  FUNC_LED,        // 4  — LED sortida digital
  FUNC_NEOPIXEL,   // 5  — Data WS2812/NeoPixel
  FUNC_PWM,        // 6  — Dimmer PWM single
  FUNC_PWM_WW,     // 7  — PWM Warm White
  FUNC_PWM_CW,     // 8  — PWM Cold White
  FUNC_ZCD,        // 9  — Zero Cross Detection (interrupt entrada)
  FUNC_TRIAC,      // 10 — Porta triac sortida
  FUNC_MOSFET,     // 11 — MOSFET On/Off (com un relé)
  FUNC_COUNT
};

struct FuncDef {
  GpioFunc    func;
  const char* id;
  const char* label;
  bool        isInput;
  bool        needsChan;
};

static const FuncDef FUNC_REGISTRY[] = {
  { FUNC_NONE,       "none",       "",                    false, false },
  { FUNC_BTN,        "btn",        "Boto",                true,  true  },
  { FUNC_BTN_INV,    "btn_inv",    "Boto (invers)",       true,  true  },
  { FUNC_RELAY,      "relay",      "Rele",                false, true  },
  { FUNC_LED,        "led",        "LED",                 false, true  },
  { FUNC_NEOPIXEL,   "neopixel",   "NeoPixel/WS2812",     false, true  },
  { FUNC_PWM,        "pwm",        "PWM",                 false, true  },
  { FUNC_PWM_WW,     "pwm_ww",     "PWM Warm White",      false, true  },
  { FUNC_PWM_CW,     "pwm_cw",     "PWM Cold White",      false, true  },
  { FUNC_ZCD,        "zcd",        "Zero Cross Det.",     true,  true  },
  { FUNC_TRIAC,      "triac",      "Triac Gate",          false, true  },
  { FUNC_MOSFET,     "mosfet",     "MOSFET On/Off",       false, true  },
};

// Capacitats per GPIO (usades per la web per validar configuració)
struct GpioCaps { bool valid; bool hasPwm; bool hasAdc; bool inputOnly; };

static const GpioCaps ESP32C3_GPIO_CAPS[22] = {
  //       valid,  hasPwm, hasAdc, inputOnly
  {  true,  true,  true,  false },  //  0
  {  true,  true,  true,  false },  //  1
  {  true,  true,  true,  false },  //  2
  {  true,  true,  true,  false },  //  3
  {  true,  true,  true,  false },  //  4
  {  true,  true, false,  false },  //  5
  {  true,  true, false,  false },  //  6
  {  true,  true, false,  false },  //  7
  {  true,  true, false,  false },  //  8
  {  true,  true, false,  false },  //  9
  {  true,  true, false,  false },  // 10
  {  true,  true, false,  false },  // 11 (flash CS, amb precaucio)
  {  true,  true, false,  false },  // 12 (flash CLK, amb precaucio)
  {  true,  true, false,  false },  // 13 (flash DIO, amb precaucio)
  {  true,  true, false,  false },  // 14 (flash DIO, amb precaucio)
  {  true,  true, false,  false },  // 15 (flash DIO, amb precaucio)
  {  true,  true, false,  false },  // 16 (flash DIO, amb precaucio)
  {  true,  true, false,  false },  // 17 (flash DIO, amb precaucio)
  { false, false, false,  false },  // 18 (USB D-)
  { false, false, false,  false },  // 19 (USB D+)
  {  true,  true, false,  false },  // 20
  {  true,  true, false,  false },  // 21
};

static const GpioCaps ESP32S3_GPIO_CAPS[22] = {
  //       valid,  hasPwm, hasAdc, inputOnly
  {  true,  true, false,  false },  //  0 (strapping)
  {  true,  true,  true,  false },  //  1
  {  true,  true,  true,  false },  //  2
  {  true,  true,  true,  false },  //  3
  {  true,  true,  true,  false },  //  4
  {  true,  true,  true,  false },  //  5
  {  true,  true,  true,  false },  //  6
  {  true,  true,  true,  false },  //  7
  {  true,  true,  true,  false },  //  8
  {  true,  true,  true,  false },  //  9
  {  true,  true,  true,  false },  // 10
  {  true,  true, false,  false },  // 11
  {  true,  true, false,  false },  // 12
  {  true,  true, false,  false },  // 13
  {  true,  true, false,  false },  // 14
  {  true,  true, false,  false },  // 15
  {  true,  true, false,  false },  // 16
  {  true,  true, false,  false },  // 17
  {  true, false, false,  false },  // 18 (USB D-, usable)
  {  true, false, false,  false },  // 19 (USB D+, usable)
  {  true,  true, false,  false },  // 20
  {  true,  true, false,  false },  // 21
};

struct GpioPinTemplate { uint8_t gpio; GpioFunc func; uint8_t canal; };
struct DeviceTemplate   { const char* name; GpioPinTemplate pins[8]; uint8_t count; };

static const DeviceTemplate DEVICE_TEMPLATES[] = {
  { "SONOFF_BASIC_R4", {
    { 9, FUNC_BTN,      0 },
    { 4, FUNC_RELAY,    1 },
    { 6, FUNC_LED,      1 }
  }, 3 },
  { "PICO_CLICK", {
    { 5, FUNC_BTN_INV,  0 },
    { 6, FUNC_NEOPIXEL, 0 }
  }, 2 },
  { "ESP32_S3_ZERO", {
    { 0, FUNC_BTN,      0 },
    {21, FUNC_NEOPIXEL, 0 }
  }, 2 },
  { "AC_REGULATOR", {
    { 1, FUNC_BTN_INV,  0 },
    { 0, FUNC_ZCD,      1 },
    { 4, FUNC_TRIAC,    1 },
    { 5, FUNC_NEOPIXEL, 0 }
  }, 4 },
  { "AC_CYCLE", {
    { 1, FUNC_BTN_INV,  0 },
    { 4, FUNC_TRIAC,    1 },
    { 5, FUNC_NEOPIXEL, 0 }
  }, 3 },
};


// ════════════════════════════════════════════════════════════════
//  TIPUS DE CONTROL (control_type)
// ════════════════════════════════════════════════════════════════
#define CTRL_TYPE_ONOFF        0  // Relé / LED / MOSFET (digital)
#define CTRL_TYPE_NEOPIXEL     1  // LED digital WS2812/NeoPixel
#define CTRL_TYPE_PWM          2  // PWM dimmer (1 canal o 2 canals WW+CW)
#define CTRL_TYPE_TRIAC_CYCLE  3  // Triac per cicle a 50 Hz (sense ZCD)
#define CTRL_TYPE_TRIAC_PHASE  4  // Triac per fase (amb ZCD)


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
//  MQTT  (valors per defecte — configurables via web i guardats a NVS)
//  Wildcards: %id% → últims 4 caràcters de la MAC  |  %topic% → valor resolt de HC_MQTT_TOPIC
// ════════════════════════════════════════════════════════════════
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
#define COLOR_LLUM      blanc   // color per defecte del LED (configurable via web)
#define COLOR_INICI     verd
#define COLOR_WIFI_AP   lila
#define COLOR_MQTT      groc


// ════════════════════════════════════════════════════════════════
//  PWM (Led Dimmer i WW/CW)
// ════════════════════════════════════════════════════════════════
#define PWM_FREQ        5000   // freqüència Hz
#define PWM_RESOLUTION  8      // bits (8 → rang 0–255)
#define PWM_DUTY_DEF    50     // duty cycle per defecte (0–100 %)


// ════════════════════════════════════════════════════════════════
//  CONTROL DE FASE (Triac + ZCD)
// ════════════════════════════════════════════════════════════════
#define TRIAC_PULSE_US       100   // durada del pols de dispar del triac (µs)
#define AC_HALF_CYCLE_US   10000   // semiperíode de 50 Hz (µs)
#define TRIAC_MIN_DELAY_US   500   // retard mínim de dispar després del ZCD (µs)


// ════════════════════════════════════════════════════════════════
//  SISTEMA
// ════════════════════════════════════════════════════════════════
#define LOG_LEVEL              3        // 0=silent 1=error 2=info 3=debug
#define CONFIG_SCHEMA_VERSION  2        // incrementa quan canvies claus NVS
#define FIRMWARE_VERSION       "1.0"
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
