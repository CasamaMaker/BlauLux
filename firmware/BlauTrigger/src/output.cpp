#include "output.h"
#include <esp_timer.h>
#include <Adafruit_NeoPixel.h>

// ── Canal LEDC per GPIO ──────────────────────────────────────────
static int8_t _ledcCh[22];
static int    _nextLedcCh = 0;

// ── NeoPixel per GPIO ────────────────────────────────────────────
static Adafruit_NeoPixel* _neoPixel[22] = {};

static int ledcChannelFor(int gpio) {
  if (gpio < 0 || gpio > 21) return -1;
  if (_ledcCh[gpio] >= 0) return _ledcCh[gpio];
  _ledcCh[gpio] = (int8_t)(_nextLedcCh++);
  return _ledcCh[gpio];
}

// ── Triac fase (ZCD + FreeRTOS task) ────────────────────────────
static SemaphoreHandle_t _zcdSemaphore    = NULL;
static TaskHandle_t      _triacTaskHandle = NULL;
static int               _zcdPin          = PIN_UNUSED;
static int               _triacPin        = PIN_UNUSED;
static volatile uint32_t _zcdMicros       = 0;
static volatile uint32_t _zcdPeriodUs     = 0;

struct TriacState { bool on; int power; };
static portMUX_TYPE _triacMux  = portMUX_INITIALIZER_UNLOCKED;
static TriacState   _triacSnap = {false, 0};

static void _triacSnapUpdate(bool on, int power) {
  portENTER_CRITICAL(&_triacMux);
  _triacSnap.on    = on;
  _triacSnap.power = power;
  portEXIT_CRITICAL(&_triacMux);
}

void IRAM_ATTR zcdISR() {
  uint32_t now    = (uint32_t)micros();
  uint32_t period = now - _zcdMicros;
  _zcdMicros = now;
  if (period > 17000 && period < 23000)
    _zcdPeriodUs = _zcdPeriodUs == 0 ? period : (_zcdPeriodUs * 7 + period) >> 3;
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(_zcdSemaphore, &woken);
  if (woken) portYIELD_FROM_ISR();
}

void triacTask(void* pvParameters) {
  (void)pvParameters;
  while (1) {
    if (xSemaphoreTake(_zcdSemaphore, pdMS_TO_TICKS(100)) != pdTRUE) continue;

    portENTER_CRITICAL(&_triacMux);
    bool on    = _triacSnap.on;
    int  power = _triacSnap.power;
    portEXIT_CRITICAL(&_triacMux);

    if (!on || _triacPin == PIN_UNUSED || power <= 0) continue;

    uint32_t zcd      = _zcdMicros;
    uint32_t delay_us = (uint32_t)((100 - power) * (AC_HALF_CYCLE_US - TRIAC_MIN_DELAY_US) / 100
                                    + TRIAC_MIN_DELAY_US);
    uint32_t t1 = zcd + delay_us;
    int32_t  r1 = (int32_t)(t1 - (uint32_t)micros());
    if (r1 > 3000) vTaskDelay(pdMS_TO_TICKS((r1 - 3000) / 1000));
    while ((int32_t)(t1 - (uint32_t)micros()) > 0);
    if (on && _triacPin != PIN_UNUSED) {
      digitalWrite(_triacPin, HIGH);
      delayMicroseconds(TRIAC_PULSE_US);
      digitalWrite(_triacPin, LOW);
    }

    uint32_t t2 = zcd + AC_HALF_CYCLE_US + delay_us;
    int32_t  r2 = (int32_t)(t2 - (uint32_t)micros());
    if (r2 > 3000) vTaskDelay(pdMS_TO_TICKS((r2 - 3000) / 1000));
    while ((int32_t)(t2 - (uint32_t)micros()) > 0);
    if (on && _triacPin != PIN_UNUSED) {
      digitalWrite(_triacPin, HIGH);
      delayMicroseconds(TRIAC_PULSE_US);
      digitalWrite(_triacPin, LOW);
    }
  }
}

// ── Triac cicle (esp_timer, sense ZCD) ──────────────────────────
static esp_timer_handle_t _cycleTimer = NULL;
static portMUX_TYPE       _cycleMux   = portMUX_INITIALIZER_UNLOCKED;
static int                _cycleGpio  = PIN_UNUSED;
static bool               _cycleOn    = false;
static int                _cyclePower = 0;

static void _cycleSnapUpdate(bool on, int power) {
  portENTER_CRITICAL(&_cycleMux);
  _cycleOn    = on;
  _cyclePower = power;
  portEXIT_CRITICAL(&_cycleMux);
}

static void cycleTimerCallback(void*) {
  static uint32_t counter = 0;
  portENTER_CRITICAL(&_cycleMux);
  bool on    = _cycleOn;
  int  power = _cyclePower;
  portEXIT_CRITICAL(&_cycleMux);
  if (!on || _cycleGpio == PIN_UNUSED || power <= 0) { counter = 0; return; }
  if (++counter >= 100) counter = 0;
  if ((int)counter < power) {
    digitalWrite(_cycleGpio, HIGH);
    delayMicroseconds(TRIAC_PULSE_US);
    digitalWrite(_cycleGpio, LOW);
  }
}

static void _initTriacCycle(int gpio) {
  _cycleGpio = gpio;
  esp_timer_create_args_t args = {};
  args.callback        = cycleTimerCallback;
  args.dispatch_method = ESP_TIMER_TASK;
  args.name            = "triac_cycle";
  esp_timer_create(&args, &_cycleTimer);
  esp_timer_start_periodic(_cycleTimer, 10000);
}

// ── Cleanup ──────────────────────────────────────────────────────
void cleanupTriac() {
  if (_zcdPin != PIN_UNUSED) {
    detachInterrupt(digitalPinToInterrupt(_zcdPin));
    _zcdPin = PIN_UNUSED;
  }
  if (_triacTaskHandle != NULL) { vTaskDelete(_triacTaskHandle); _triacTaskHandle = NULL; }
  if (_zcdSemaphore    != NULL) { vSemaphoreDelete(_zcdSemaphore); _zcdSemaphore = NULL; }
  if (_triacPin != PIN_UNUSED)  { digitalWrite(_triacPin, LOW); _triacPin = PIN_UNUSED; }
  _zcdPeriodUs = 0;
  _triacSnapUpdate(false, 0);
}

void cleanupTriacCycle() {
  if (_cycleTimer != NULL) {
    esp_timer_stop(_cycleTimer);
    esp_timer_delete(_cycleTimer);
    _cycleTimer = NULL;
  }
  _cycleSnapUpdate(false, 0);
  _cycleGpio = PIN_UNUSED;
}

// ── ZCD setup (cridat des de driverSetup del GPIO triac_fase) ────
static void driverSetupZcd(int triacGpio, int zcdGpio) {
  _triacPin     = triacGpio;
  _zcdPin       = zcdGpio;
  _zcdSemaphore = xSemaphoreCreateBinary();
  pinMode(zcdGpio, INPUT);
  attachInterrupt(digitalPinToInterrupt(zcdGpio), zcdISR, RISING);
  xTaskCreatePinnedToCore(triacTask, "triac", 3072, NULL, 5, &_triacTaskHandle, 0);
}

// ── Helper ──────────────────────────────────────────────────────
int findGpio(GpioFunc func) {
  for (int i = 0; i <= 21; i++)
    if (gpioMap[i].cfg.func == func) return i;
  return PIN_UNUSED;
}

// ── g_driver (backward-compat per mqtt/espnow) ───────────────────
static void _noop_applyOutput(bool) {}
static void _noop_renderInici()     {}
static void _noop_renderAP()        {}

static OutputDriver  _activeDriver = { _noop_applyOutput, _noop_renderInici, _noop_renderAP,
                                        false, false, false, "none", "switch" };
const OutputDriver*  g_driver = nullptr;

static void _updateGDriver() {
  bool        hasColor = false, hasBr = false, hasCCT = false;
  const char* typeName = "none";
  const char* discType = "switch";
  int         pwmCount = 0;
  bool        hasOutput = false;

  for (int i = 0; i <= 21; i++) {
    switch (gpioMap[i].cfg.func) {
      case FUNC_ON_OFF:
        hasOutput = true; typeName = "switch"; break;
      case FUNC_DIGITAL_LED:
        hasOutput = true; hasColor = true; hasBr = true; typeName = "light"; discType = "light"; break;
      case FUNC_PWM:
        hasOutput = true; hasBr = true; discType = "light"; typeName = "pwm"; pwmCount++; break;
      case FUNC_TRIAC_CYCLE:
        hasOutput = true; hasBr = true; discType = "light"; typeName = "triac_cycle"; break;
      case FUNC_TRIAC_FASE:
        hasOutput = true; hasBr = true; discType = "light"; typeName = "triac_phase"; break;
      default: break;
    }
  }
  if (pwmCount >= 2) { hasCCT = true; typeName = "pwm_cct"; }

  _activeDriver.hasColor       = hasColor;
  _activeDriver.hasBrightness  = hasBr;
  _activeDriver.hasCCT         = hasCCT;
  _activeDriver.typeName       = typeName;
  _activeDriver.haDiscoveryType = discType;
  g_driver = hasOutput ? &_activeDriver : nullptr;
}

// ── driverSetup ──────────────────────────────────────────────────
void driverSetup(int gpio) {
  gpioMap[gpio].rt.param1       = gpioMap[gpio].cfg.param1;
  gpioMap[gpio].rt.param2       = gpioMap[gpio].cfg.param2;
  gpioMap[gpio].rt.param3       = gpioMap[gpio].cfg.param3;
  gpioMap[gpio].rt.state        = false;
  gpioMap[gpio].rt.dirty        = false;
  gpioMap[gpio].rt.lastChangeMs = 0;

  switch (gpioMap[gpio].cfg.func) {
    case FUNC_BTN:
    case FUNC_BTN_INV:
      pinMode(gpio, INPUT);
      break;
    case FUNC_ON_OFF:
      pinMode(gpio, OUTPUT);
      digitalWrite(gpio, LOW);
      break;
    case FUNC_DIGITAL_LED: {
      if (_neoPixel[gpio]) { delete _neoPixel[gpio]; _neoPixel[gpio] = nullptr; }
      uint16_t numLeds = gpioMap[gpio].rt.param3 ? gpioMap[gpio].rt.param3 : 1;
      _neoPixel[gpio] = new Adafruit_NeoPixel(numLeds, (uint16_t)gpio, NEO_GRB + NEO_KHZ800);
      _neoPixel[gpio]->begin();
      _neoPixel[gpio]->clear();
      _neoPixel[gpio]->show();
      break;
    }
    case FUNC_PWM: {
      int      ch   = ledcChannelFor(gpio);
      uint32_t freq = gpioMap[gpio].rt.param2 ? (uint32_t)gpioMap[gpio].rt.param2 : PWM_FREQ;
      if (ch >= 0) {
        ledcSetup(ch, freq, PWM_RESOLUTION);
        ledcAttachPin(gpio, ch);
        ledcWrite(ch, 0);
      }
      break;
    }
    case FUNC_TRIAC_CYCLE:
      pinMode(gpio, OUTPUT);
      _initTriacCycle(gpio);
      break;
    case FUNC_TRIAC_FASE: {
      pinMode(gpio, OUTPUT);
      int zcdGpio = (int)gpioMap[gpio].rt.param2;
      if (zcdGpio >= 0 && zcdGpio <= 21) driverSetupZcd(gpio, zcdGpio);
      break;
    }
    case FUNC_ZCD:
      // inicialitzat per driverSetupZcd del GPIO triac_fase corresponent
      break;
    default:
      break;
  }
  gpioMap[gpio].rt.initialized = true;
}

// ── driverSetupAll ───────────────────────────────────────────────
void driverSetupAll() {
  memset(_ledcCh, -1, sizeof(_ledcCh));   //omplir tota la memòria de _ledcCh amb el valor 0xFF
  _nextLedcCh = 0;
  cleanupTriac();
  cleanupTriacCycle();
  for (int i = 0; i <= 21; i++) driverSetup(i);
  _updateGDriver();
  LOG_D("[DRV] driverSetupAll complet");
}

// ── driverApply ──────────────────────────────────────────────────
void driverApply(int gpio) {
  if (!gpioMap[gpio].rt.initialized) return;
  bool     on = gpioMap[gpio].rt.state;
  uint32_t p1 = gpioMap[gpio].rt.param1;
  uint16_t p2 = gpioMap[gpio].rt.param2;
  uint16_t p3 = gpioMap[gpio].rt.param3;

  switch (gpioMap[gpio].cfg.func) {
    case FUNC_ON_OFF:
      digitalWrite(gpio, on ? HIGH : LOW);
      break;
    case FUNC_PWM: {
      int ch = ledcChannelFor(gpio);
      if (ch >= 0) ledcWrite(ch, on ? (uint32_t)map(p1, 0, 100, 0, 255) : 0);
      break;
    }
    case FUNC_TRIAC_CYCLE:
      _cycleSnapUpdate(on, (int)p1);
      break;
    case FUNC_TRIAC_FASE:
      _triacSnapUpdate(on, (int)p1);
      break;
    case FUNC_DIGITAL_LED: {
      Adafruit_NeoPixel* px = _neoPixel[gpio];
      if (!px) break;
      uint32_t col = 0;
      if (on) {
        uint8_t r  = (uint8_t)((p1 >> 16) & 0xFF);
        uint8_t g  = (uint8_t)((p1 >>  8) & 0xFF);
        uint8_t b  = (uint8_t)( p1        & 0xFF);
        uint8_t br = (uint8_t)constrain((int)p2, 0, 100);
        col = px->Color((uint8_t)((uint16_t)r * br / 100),
                        (uint8_t)((uint16_t)g * br / 100),
                        (uint8_t)((uint16_t)b * br / 100));
      }
      px->fill(col);
      px->show();
      break;
    }
    default:
      break;
  }
  gpioMap[gpio].rt.dirty = true;
}

// ── driverToggle / driverToggleAll ───────────────────────────────
void driverToggle(int gpio) {
  gpioMap[gpio].rt.state        = !gpioMap[gpio].rt.state;
  gpioMap[gpio].rt.lastChangeMs = (uint32_t)millis();
  driverApply(gpio);
}

void driverToggleAll() {
  for (int i = 0; i <= 21; i++) {
    GpioFunc f = gpioMap[i].cfg.func;
    if (f == FUNC_NONE || f == FUNC_BTN || f == FUNC_BTN_INV || f == FUNC_ZCD) continue;    // descarta si és una d'aquestes funcions
    driverToggle(i);
  }
}

// ── getAcFreqHz ──────────────────────────────────────────────────
float getAcFreqHz() {
  uint32_t p      = _zcdPeriodUs;
  bool     active = (p > 0) && ((uint32_t)(micros() - _zcdMicros) < 200000UL);
  return active ? 1000000.0f / (float)p : 0.0f;
}

// ── Backward-compat ──────────────────────────────────────────────

void applyGpioConfig()  { driverSetupAll(); }
void configuracioLlum() {}

static int _firstOutputGpio() {
  for (int i = 0; i <= 21; i++) {
    GpioFunc f = gpioMap[i].cfg.func;
    if (f == FUNC_ON_OFF || f == FUNC_PWM || f == FUNC_TRIAC_CYCLE ||
        f == FUNC_TRIAC_FASE || f == FUNC_DIGITAL_LED) return i;
  }
  return PIN_UNUSED;
}

bool getState() {
  int g = _firstOutputGpio();
  return g != PIN_UNUSED && gpioMap[g].rt.state;
}

void applyOutput(bool on) {
  for (int i = 0; i <= 21; i++) {
    GpioFunc f = gpioMap[i].cfg.func;
    if (f == FUNC_NONE || f == FUNC_BTN || f == FUNC_BTN_INV || f == FUNC_ZCD) continue;
    gpioMap[i].rt.state        = on;
    gpioMap[i].rt.lastChangeMs = (uint32_t)millis();
    driverApply(i);
  }
}

void toggleOutput() { driverToggleAll(); }

void renderVisualFeedback(const char* mode) {
  for (int i = 0; i <= 21; i++) {
    GpioFunc f = gpioMap[i].cfg.func;
    if (f == FUNC_PWM) {
      int ch = ledcChannelFor(i);
      if (ch < 0) break;
      if (strcmp(mode, "inici") == 0) {
        ledcWrite(ch, map(gpioMap[i].rt.param1, 0, 100, 0, 255));
        delay(INICI_BLINK_MS);
        ledcWrite(ch, 0);
      } else if (strcmp(mode, "wifiAP") == 0) {
        uint16_t osc    = (uint16_t)((millis() / 2) % 510);
        uint8_t  bright = osc < 255 ? (uint8_t)osc : (uint8_t)(510 - osc);
        ledcWrite(ch, bright);
      }
      return;
    }
    if (f == FUNC_ON_OFF) {
      if (strcmp(mode, "inici") == 0) {
        digitalWrite(i, HIGH); delay(INICI_BLINK_MS); digitalWrite(i, LOW);
      } else if (strcmp(mode, "wifiAP") == 0) {
        digitalWrite(i, (millis() / 500) % 2 ? HIGH : LOW);
      }
      return;
    }
  }
}

int  getBotonPin()    { int g = findGpio(FUNC_BTN); return g != PIN_UNUSED ? g : findGpio(FUNC_BTN_INV); }
bool getButtonPullup(){ return findGpio(FUNC_BTN) != PIN_UNUSED; }

int getControlType() {
  int g = _firstOutputGpio();
  if (g == PIN_UNUSED) return -1;
  switch (gpioMap[g].cfg.func) {
    case FUNC_ON_OFF:      return CTRL_TYPE_ONOFF;
    case FUNC_PWM:         return CTRL_TYPE_PWM;
    case FUNC_TRIAC_CYCLE: return CTRL_TYPE_TRIAC_CYCLE;
    case FUNC_TRIAC_FASE:  return CTRL_TYPE_TRIAC_PHASE;
    default:               return -1;
  }
}

int getPin1() { return _firstOutputGpio(); }
int getPin2() { return PIN_UNUSED; }
int getPin3() { return PIN_UNUSED; }

int getBrightnessForType(int ct) {
  for (int i = 0; i <= 21; i++) {
    GpioFunc f = gpioMap[i].cfg.func;
    if (ct == CTRL_TYPE_PWM         && f == FUNC_PWM)         return (int)gpioMap[i].rt.param1;
    if (ct == CTRL_TYPE_TRIAC_CYCLE && f == FUNC_TRIAC_CYCLE) return (int)gpioMap[i].rt.param1;
    if (ct == CTRL_TYPE_TRIAC_PHASE && f == FUNC_TRIAC_FASE)  return (int)gpioMap[i].rt.param1;
    if (ct == CTRL_TYPE_ONOFF       && f == FUNC_DIGITAL_LED) return (int)gpioMap[i].rt.param2;
  }
  return BRIGHTNESS_DEF;
}

void setBrightnessForType(int ct, int v) {
  v = constrain(v, 0, 100);
  for (int i = 0; i <= 21; i++) {
    GpioFunc f = gpioMap[i].cfg.func;
    if ((ct == CTRL_TYPE_PWM         && f == FUNC_PWM)         ||
        (ct == CTRL_TYPE_TRIAC_CYCLE && f == FUNC_TRIAC_CYCLE) ||
        (ct == CTRL_TYPE_TRIAC_PHASE && f == FUNC_TRIAC_FASE))
      gpioMap[i].rt.param1 = (uint32_t)v;
    else if (ct == CTRL_TYPE_ONOFF   && f == FUNC_DIGITAL_LED)
      gpioMap[i].rt.param2 = (uint16_t)v;
  }
}

int getBrightnessCW() {
  bool first = true;
  for (int i = 0; i <= 21; i++) {
    if (gpioMap[i].cfg.func != FUNC_PWM) continue;
    if (first) { first = false; continue; }
    return (int)gpioMap[i].rt.param1;
  }
  return BRIGHTNESS_DEF;
}

void setBrightnessCW(int v) {
  v = constrain(v, 0, 100);
  bool first = true;
  for (int i = 0; i <= 21; i++) {
    if (gpioMap[i].cfg.func != FUNC_PWM) continue;
    if (first) { first = false; continue; }
    gpioMap[i].rt.param1 = (uint32_t)v;
    return;
  }
}

uint32_t getCurrentColor() {
  int g = findGpio(FUNC_DIGITAL_LED);
  return g != PIN_UNUSED ? gpioMap[g].rt.param1 : COLOR_LLUM;
}

void setCurrentColor(uint32_t c) {
  int g = findGpio(FUNC_DIGITAL_LED);
  if (g != PIN_UNUSED) gpioMap[g].rt.param1 = c;
}

void applyBrightness(int br) {
  int ct = getControlType();
  if (ct >= 0) setBrightnessForType(ct, br);
  if (getState()) applyOutput(true);
}

void applyMosfetDuty(int duty) {
  int main = _firstOutputGpio();
  for (int i = 0; i <= 21; i++) {
    if (gpioMap[i].cfg.func != FUNC_PWM || i == main) continue;
    gpioMap[i].rt.param1 = (uint32_t)constrain(duty, 0, 100);
    driverApply(i);
    return;
  }
}
