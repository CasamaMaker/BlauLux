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
  FUNC_BTN,         // 1  — Botó (pull-up, premut=LOW)
  FUNC_BTN_INV,     // 2  — Botó invers (pull-down, premut=HIGH)
  FUNC_ON_OFF,      // 3  — Relé/mosfet/led... sortida simple
  FUNC_DIGITAL_LED, // 4  — LED digital
  FUNC_PWM,         // 5  — Dimmer PWM single/dual
  FUNC_ZCD,         // 6  — Zero Cross Detection (interrupt entrada)
  FUNC_TRIAC_FASE,  // 7  — Porta triac (control de fase, requereix ZCD)
  FUNC_TRIAC_CYCLE, // 8  — Porta triac (control per cicle, sense ZCD)
  FUNC_COUNT
};

struct FuncDef {
  GpioFunc    func;
  const char* id;
  const char* label;
  bool        isInput;
};

static const FuncDef FUNC_REGISTRY[] = {
  { FUNC_NONE,        "none",        "",                   false },
  { FUNC_BTN,         "btn",         "Boto",               true  },
  { FUNC_BTN_INV,     "btn_inv",     "Boto (invers)",      true  },
  { FUNC_ON_OFF,      "on_off",      "On/Off",             false },
  { FUNC_DIGITAL_LED, "digital_led", "LED digital",        false },
  { FUNC_PWM,         "pwm",         "PWM",                false },
  { FUNC_ZCD,         "zcd",         "Zero Cross Det.",    true  },
  { FUNC_TRIAC_FASE,  "triac_fase",  "Triac (fase)",       false },
  { FUNC_TRIAC_CYCLE, "triac_cycle", "Triac (cicle)",      false },
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
  {  true, false, false,  false },  // 18 (USB D-)
  {  true, false, false,  false },  // 19 (USB D+)
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

struct GpioPinTemplate { uint8_t gpio; GpioFunc func; };
struct DeviceTemplate   { const char* name; GpioPinTemplate pins[8]; uint8_t count; bool select; };

static const DeviceTemplate DEVICE_TEMPLATES[] = {
  { "SONOFF_BASIC_R4", {
    { 9, FUNC_BTN        },
    { 4, FUNC_ON_OFF     },
    { 6, FUNC_ON_OFF     }
  }, 3 },
    { "PICO-CLICK", {
    { 5, FUNC_BTN_INV    },
    { 6, FUNC_DIGITAL_LED},
  }, 2 },
  { "AC_REGULATOR", {
    { 1, FUNC_BTN_INV   },
    { 0, FUNC_ZCD       },
    { 4, FUNC_TRIAC_FASE},
    { 5, FUNC_DIGITAL_LED},
  }, 4 },
  { "GL-C-309WL", {
    { 16, FUNC_DIGITAL_LED},
    { 17, FUNC_BTN   },
    { 18, FUNC_ON_OFF},
  }, 3 },
};


// ════════════════════════════════════════════════════════════════
//  TIPUS DE CONTROL (control_type)
// ════════════════════════════════════════════════════════════════
#define CTRL_TYPE_ONOFF        0  // On/Off simple (relé, mosfet, LED)
#define CTRL_TYPE_PWM          1  // PWM dimmer (1 o 2 canals WW+CW)
#define CTRL_TYPE_TRIAC_CYCLE  2  // Triac per cicle a 50 Hz (sense ZCD)
#define CTRL_TYPE_TRIAC_PHASE  3  // Triac per fase (amb ZCD)


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
#define WIFI_SSID      "BlauLux"
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
#define HC_MQTT_CLIENT    "BlauLux_%id%"     // client ID  (ex: BlauLux_A1B2)
#define HC_MQTT_TOPIC     "%id%"             // topic curt del dispositiu  (ex: A1B2)
#define HC_MQTT_FULLTOPIC "blaulux/%topic%"  // prefix complet dels topics (ex: blaulux/A1B2)
#define HC_MQTT_PORT      1883


// ════════════════════════════════════════════════════════════════
//  BRILLANTOR
// ════════════════════════════════════════════════════════════════
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
#define ZCD_FLAG_USED    FALLING    // RISING OR FALLING  


// ════════════════════════════════════════════════════════════════
//  SISTEMA
// ════════════════════════════════════════════════════════════════
#define LOG_LEVEL              3        // 0=silent 1=error 2=info 3=debug
#define CONFIG_SCHEMA_VERSION  4        // incrementa quan canvies claus NVS
#define FIRMWARE_VERSION       "1.0"
#define SERIAL_BAUD            115200  // velocitat del port sèrie
#define WIFI_AP_HOLD_MS          3000  // ms prement el botó per entrar al mode AP
#define WIFI_AP_TIMEOUT_MS     120000  // ms màxims en mode AP abans de reiniciar
#define WIFI_STA_TIMEOUT_MS    15000   // ms màxims esperant connexió STA al setup inicial
#define HTTP_PORT                  80  // port del servidor web
#define DNS_PORT                   53  // port del servidor DNS
#define DNS_POLL_MS               100  // interval de polling DNS al bucle AP
#define ESPNOW_CHANNEL              1  // canal Wi-Fi per a ESP-NOW
#define INICI_BLINK_MS            500  // durada del parpelleig d'inici
#define BUTTON_DEBOUNCE_MS        500  // debounce després de clic de botó
#define BUTTON_RELEASE_DEBOUNCE_MS 200 // debounce detecció alliberament de botó
#define LED_POWER_SETTLE_MS         5  // espera (ms) entre activar MOSFET alimentació i enviar dades NeoPixel
