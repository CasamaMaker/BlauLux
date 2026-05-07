#include "output.h"
#include <esp_timer.h>

// ── Variables privades del mòdul ─────────────────────────────────
static int _triacPin    = PIN_UNUSED;
static int _mosfetGpio  = PIN_UNUSED;
static int _pwmChMosfet = 2;

// ── Triac fase (ZCD + FreeRTOS task) ────────────────────────────
static SemaphoreHandle_t _zcdSemaphore    = NULL;
static TaskHandle_t      _triacTaskHandle = NULL;
static int               _zcdPin          = PIN_UNUSED;
static volatile uint32_t _zcdMicros       = 0;
static volatile uint32_t _zcdPeriodUs     = 0;
static volatile uint32_t _zcdCount        = 0;

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
  uint32_t now = (uint32_t)micros();
  uint32_t period = now - _zcdMicros;
  _zcdMicros = now;
  if (period > 17000 && period < 23000)
    _zcdPeriodUs = _zcdPeriodUs == 0 ? period : (_zcdPeriodUs * 7 + period) >> 3;
  _zcdCount++;
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

    uint32_t zcd = _zcdMicros;
    uint32_t delay_us = (uint32_t)((100 - power) * (AC_HALF_CYCLE_US - TRIAC_MIN_DELAY_US) / 100
                                    + TRIAC_MIN_DELAY_US);
    uint32_t target = zcd + delay_us;
    int32_t remaining = (int32_t)(target - (uint32_t)micros());
    if (remaining > 3000) vTaskDelay(pdMS_TO_TICKS((remaining - 3000) / 1000));
    while ((int32_t)(target - (uint32_t)micros()) > 0);
    if (on && _triacPin != PIN_UNUSED) {
      digitalWrite(_triacPin, HIGH);
      delayMicroseconds(TRIAC_PULSE_US);
      digitalWrite(_triacPin, LOW);
    }

    uint32_t target2 = zcd + AC_HALF_CYCLE_US + delay_us;
    int32_t remaining2 = (int32_t)(target2 - (uint32_t)micros());
    if (remaining2 > 3000) vTaskDelay(pdMS_TO_TICKS((remaining2 - 3000) / 1000));
    while ((int32_t)(target2 - (uint32_t)micros()) > 0);
    if (on && _triacPin != PIN_UNUSED) {
      digitalWrite(_triacPin, HIGH);
      delayMicroseconds(TRIAC_PULSE_US);
      digitalWrite(_triacPin, LOW);
    }
  }
}

// ── Triac cicle (esp_timer, sense ZCD) ──────────────────────────
// Disparos a 100 Hz (cada semiperíode de 50 Hz).
// El comptador avança 100 cops per segon; els primers 'power' ticks disparen.
static esp_timer_handle_t _cycleTimer = NULL;
static portMUX_TYPE       _cycleMux   = portMUX_INITIALIZER_UNLOCKED;
static bool               _cycleOn    = false;
static int                _cyclePower = 0;

static void _cycleSnapUpdate(bool on, int power) {
  portENTER_CRITICAL(&_cycleMux);
  _cycleOn    = on;
  _cyclePower = power;
  portEXIT_CRITICAL(&_cycleMux);
}

static void cycleTimerCallback(void* arg) {
  static uint32_t counter = 0;
  portENTER_CRITICAL(&_cycleMux);
  bool on    = _cycleOn;
  int  power = _cyclePower;
  portEXIT_CRITICAL(&_cycleMux);
  if (!on || _triacPin == PIN_UNUSED || power <= 0) { counter = 0; return; }
  counter++;
  if (counter >= 100) counter = 0;
  if ((int)counter < power) {
    digitalWrite(_triacPin, HIGH);
    delayMicroseconds(TRIAC_PULSE_US);
    digitalWrite(_triacPin, LOW);
  }
}

static void _initTriacCycle() {
  esp_timer_create_args_t args = {};
  args.callback        = cycleTimerCallback;
  args.dispatch_method = ESP_TIMER_TASK;
  args.name            = "triac_cycle";
  esp_timer_create(&args, &_cycleTimer);
  esp_timer_start_periodic(_cycleTimer, 10000); // 10 ms = 100 Hz
}

// ── Helpers GPIO ─────────────────────────────────────────────────

int findGpio(GpioFunc func, uint8_t canal) {
  for (int i = 0; i <= 21; i++)
    if (gpioMap[i].func == func && (canal == 0 || gpioMap[i].canal == canal))
      return i;
  return PIN_UNUSED;
}

// ── Implementacions dels drivers ─────────────────────────────────

// ·· On/Off ··
static void onoff_applyOutput(bool on) {
  digitalWrite(pin1, on ? HIGH : LOW);
  if (pin2 != PIN_UNUSED) digitalWrite(pin2, on ? HIGH : LOW);
}
static void onoff_renderInici() {
  if (pin2 != PIN_UNUSED) { digitalWrite(pin2, HIGH); delay(INICI_BLINK_MS); digitalWrite(pin2, LOW); }
}
static void onoff_renderAP() {
  if (pin2 != PIN_UNUSED) digitalWrite(pin2, HIGH);
}

// ·· NeoPixel ··
static void neopixel_applyOutput(bool on) {
  strip.setBrightness(map(brightness[CTRL_TYPE_NEOPIXEL], 0, 100, 0, 255));
  for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, on ? currentColor : 0);
  strip.show();
}
static void neopixel_renderInici() {
  for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, COLOR_INICI);
  strip.show();
  delay(INICI_BLINK_MS);
  strip.clear();
  strip.show();
}
static void neopixel_renderAP() {
  uint16_t osc   = (millis() / 2) % 510;
  uint8_t  bright = (osc < 255) ? (uint8_t)osc : (uint8_t)(510 - osc);
  strip.setBrightness(bright);
  for (int i = 0; i < num_leds; i++) strip.setPixelColor(i, COLOR_WIFI_AP);
  strip.show();
}

// ·· PWM (1 o 2 canals) ··
static void pwm_applyOutput(bool on) {
  ledcWrite(pwmCh1, on ? map(brightness[CTRL_TYPE_PWM], 0, 100, 0, 255) : 0);
  if (pin2 != PIN_UNUSED) ledcWrite(pwmCh2, on ? map(brightness_cw, 0, 100, 0, 255) : 0);
}
static void pwm_renderInici() {
  ledcWrite(pwmCh1, map(brightness[CTRL_TYPE_PWM], 0, 100, 0, 255));
  if (pin2 != PIN_UNUSED) ledcWrite(pwmCh2, map(brightness_cw, 0, 100, 0, 255));
  delay(INICI_BLINK_MS);
  ledcWrite(pwmCh1, 0);
  if (pin2 != PIN_UNUSED) ledcWrite(pwmCh2, 0);
}
static void pwm_renderAP() {
  uint16_t osc   = (millis() / 2) % 510;
  uint8_t  bright = (osc < 255) ? (uint8_t)osc : (uint8_t)(510 - osc);
  ledcWrite(pwmCh1, bright);
  if (pin2 != PIN_UNUSED) ledcWrite(pwmCh2, bright);
}

// ·· Triac cicle ··
static void triac_cycle_applyOutput(bool on) {
  _cycleSnapUpdate(on, brightness[CTRL_TYPE_TRIAC_CYCLE]);
  if (pin3 == PIN_UNUSED) return;
  if (on) {
    uint8_t ledBr = (uint8_t)map((long)brightness_cw * brightness[CTRL_TYPE_TRIAC_CYCLE] / 100, 0, 100, 0, 255);
    strip.setBrightness(max((uint8_t)5, ledBr));
    strip.setPixelColor(0, currentColor);
  } else {
    strip.clear();
  }
  strip.show();
}
static void triac_cycle_renderInici() {
  if (pin3 == PIN_UNUSED) return;
  strip.setBrightness(map(brightness_cw, 0, 100, 0, 255));
  strip.setPixelColor(0, COLOR_INICI);
  strip.show();
  delay(INICI_BLINK_MS);
  strip.clear();
  strip.show();
}
static void triac_cycle_renderAP() {
  if (pin3 == PIN_UNUSED) return;
  uint16_t osc   = (millis() / 2) % 510;
  uint8_t  bright = (osc < 255) ? (uint8_t)osc : (uint8_t)(510 - osc);
  strip.setBrightness(bright);
  strip.setPixelColor(0, COLOR_WIFI_AP);
  strip.show();
}

// ·· Triac fase ··
static void triac_phase_applyOutput(bool on) {
  _triacSnapUpdate(on, brightness[CTRL_TYPE_TRIAC_PHASE]);
  if (pin3 == PIN_UNUSED) return;
  if (on) {
    uint8_t ledBr = (uint8_t)map((long)brightness_cw * brightness[CTRL_TYPE_TRIAC_PHASE] / 100, 0, 100, 0, 255);
    strip.setBrightness(max((uint8_t)5, ledBr));
    strip.setPixelColor(0, currentColor);
  } else {
    strip.clear();
  }
  strip.show();
}
static void triac_phase_renderInici() {
  if (pin3 == PIN_UNUSED) return;
  strip.setBrightness(map(brightness_cw, 0, 100, 0, 255));
  strip.setPixelColor(0, COLOR_INICI);
  strip.show();
  delay(INICI_BLINK_MS);
  strip.clear();
  strip.show();
}
static void triac_phase_renderAP() {
  if (pin3 == PIN_UNUSED) return;
  uint16_t osc   = (millis() / 2) % 510;
  uint8_t  bright = (osc < 255) ? (uint8_t)osc : (uint8_t)(510 - osc);
  strip.setBrightness(bright);
  strip.setPixelColor(0, COLOR_WIFI_AP);
  strip.show();
}

// ── Registre de drivers (base — hasCCT pot ser ajustat en runtime) ──

static const OutputDriver _onoff_base        = { onoff_applyOutput,       onoff_renderInici,       onoff_renderAP,       false, false, false, "onoff",       "switch" };
static const OutputDriver _neopixel_base     = { neopixel_applyOutput,    neopixel_renderInici,    neopixel_renderAP,    true,  true,  false, "neopixel",    "light"  };
static const OutputDriver _pwm_base          = { pwm_applyOutput,         pwm_renderInici,         pwm_renderAP,         false, true,  false, "pwm",         "light"  };
static const OutputDriver _triac_cycle_base  = { triac_cycle_applyOutput, triac_cycle_renderInici, triac_cycle_renderAP, false, true,  false, "triac_cycle", "light"  };
static const OutputDriver _triac_phase_base  = { triac_phase_applyOutput, triac_phase_renderInici, triac_phase_renderAP, false, true,  false, "triac_phase", "light"  };

// Instància mutable: permet ajustar hasCCT/typeName en applyGpioConfig()
static OutputDriver       _activeDriver;
const  OutputDriver*      g_driver = nullptr;

// ── applyGpioConfig ──────────────────────────────────────────────

void applyGpioConfig() {
  pin1 = pin2 = pin3 = boto_pin = PIN_UNUSED;
  button_pullup = 1;
  boto_canal    = 0;
  control_type  = -1;
  _mosfetGpio   = PIN_UNUSED;
  g_driver      = nullptr;

  int bg = findGpio(FUNC_BTN);
  if (bg != PIN_UNUSED) { boto_pin = bg; button_pullup = 1; boto_canal = gpioMap[bg].canal; }
  else {
    bg = findGpio(FUNC_BTN_INV);
    if (bg != PIN_UNUSED) { boto_pin = bg; button_pullup = 0; boto_canal = gpioMap[bg].canal; }
  }

  // On/Off (relé/mosfet amb canal) — prioritat 1
  for (int i = 0; i <= 21 && control_type < 0; i++) {
    if (gpioMap[i].func != FUNC_RELAY && gpioMap[i].func != FUNC_MOSFET) continue;
    uint8_t ch = gpioMap[i].canal;
    if (ch == 0) continue;
    control_type = CTRL_TYPE_ONOFF; pin1 = i;
    pin2 = findGpio(FUNC_LED, ch);
  }

  // Triac fase (amb ZCD) — prioritat 2
  if (control_type < 0) {
    for (int i = 0; i <= 21; i++) {
      if (gpioMap[i].func != FUNC_TRIAC) continue;
      uint8_t ch = gpioMap[i].canal;
      int zcd = (ch > 0) ? findGpio(FUNC_ZCD, ch) : findGpio(FUNC_ZCD);
      if (zcd != PIN_UNUSED) {
        control_type = CTRL_TYPE_TRIAC_PHASE; pin1 = zcd; pin2 = i;
        pin3 = (ch > 0) ? findGpio(FUNC_NEOPIXEL, ch) : PIN_UNUSED;
        if (pin3 == PIN_UNUSED) pin3 = findGpio(FUNC_NEOPIXEL);
        break;
      }
    }
  }

  // Triac cicle (sense ZCD) — prioritat 3
  if (control_type < 0) {
    for (int i = 0; i <= 21; i++) {
      if (gpioMap[i].func != FUNC_TRIAC) continue;
      uint8_t ch = gpioMap[i].canal;
      control_type = CTRL_TYPE_TRIAC_CYCLE; pin1 = i;
      pin3 = (ch > 0) ? findGpio(FUNC_NEOPIXEL, ch) : PIN_UNUSED;
      if (pin3 == PIN_UNUSED) pin3 = findGpio(FUNC_NEOPIXEL);
      break;
    }
  }

  // NeoPixel — prioritat 4
  if (control_type < 0) {
    int np = PIN_UNUSED;
    for (int i = 0; i <= 21; i++) {
      if (gpioMap[i].func != FUNC_NEOPIXEL) continue;
      if (gpioMap[i].canal > 0 && np == PIN_UNUSED) np = i;
      else if (np == PIN_UNUSED) np = i;
    }
    if (np != PIN_UNUSED) { control_type = CTRL_TYPE_NEOPIXEL; pin1 = np; }
  }

  // PWM dual canal (WW+CW) — prioritat 5
  if (control_type < 0) {
    for (int i = 0; i <= 21; i++) {
      if (gpioMap[i].func != FUNC_PWM_WW) continue;
      uint8_t ch = gpioMap[i].canal;
      int cw = (ch > 0) ? findGpio(FUNC_PWM_CW, ch) : findGpio(FUNC_PWM_CW);
      if (cw != PIN_UNUSED) { control_type = CTRL_TYPE_PWM; pin1 = i; pin2 = cw; break; }
    }
  }

  // PWM single canal — prioritat 6
  if (control_type < 0) {
    for (int i = 0; i <= 21; i++) {
      if (gpioMap[i].func != FUNC_PWM) continue;
      if (gpioMap[i].canal == 0) continue;
      control_type = CTRL_TYPE_PWM; pin1 = i; break;
    }
  }

  // On/Off fallback (relé/mosfet sense canal) — prioritat 7
  if (control_type < 0) {
    for (int i = 0; i <= 21; i++) {
      if (gpioMap[i].func != FUNC_RELAY && gpioMap[i].func != FUNC_MOSFET) continue;
      control_type = CTRL_TYPE_ONOFF; pin1 = i; break;
    }
  }

  // PWM fallback (qualsevol canal) — prioritat 8
  if (control_type < 0) {
    for (int i = 0; i <= 21; i++) {
      if (gpioMap[i].func != FUNC_PWM) continue;
      control_type = CTRL_TYPE_PWM; pin1 = i; break;
    }
  }

  // MOSFET autònom (PWM canal 0, no és el pin1 principal)
  for (int i = 0; i <= 21; i++) {
    if (gpioMap[i].func != FUNC_PWM || gpioMap[i].canal != 0) continue;
    if (pin1 == i) continue;
    _mosfetGpio = i; break;
  }

  // Assigna el driver actiu
  const OutputDriver* base = nullptr;
  switch (control_type) {
    case CTRL_TYPE_ONOFF:       base = &_onoff_base;       break;
    case CTRL_TYPE_NEOPIXEL:    base = &_neopixel_base;    break;
    case CTRL_TYPE_PWM:         base = &_pwm_base;         break;
    case CTRL_TYPE_TRIAC_CYCLE: base = &_triac_cycle_base; break;
    case CTRL_TYPE_TRIAC_PHASE: base = &_triac_phase_base; break;
    default: break;
  }
  if (base) {
    _activeDriver = *base;
    if (control_type == CTRL_TYPE_PWM && pin2 != PIN_UNUSED) {
      _activeDriver.hasCCT   = true;
      _activeDriver.typeName = "pwm_cct";
    }
    g_driver = &_activeDriver;
  }

  LOG_D("[CFG] gpio_config -> ct=%d drv=%s p1=%d p2=%d p3=%d bp=%d bpu=%d bchan=%d mosfet=%d",
        control_type, g_driver ? g_driver->typeName : "none",
        pin1, pin2, pin3, boto_pin, button_pullup, boto_canal, _mosfetGpio);

  resolveChannels();  // Fase 2: omple g_dev.hw[] sense modificar cap variable existent
}

// ── Cleanup ──────────────────────────────────────────────────────

void cleanupTriac() {
  if (_zcdPin != PIN_UNUSED) {
    detachInterrupt(digitalPinToInterrupt(_zcdPin));
    _zcdPin = PIN_UNUSED;
  }
  if (_triacTaskHandle != NULL) {
    vTaskDelete(_triacTaskHandle);
    _triacTaskHandle = NULL;
  }
  if (_zcdSemaphore != NULL) {
    vSemaphoreDelete(_zcdSemaphore);
    _zcdSemaphore = NULL;
  }
  if (_triacPin != PIN_UNUSED) { digitalWrite(_triacPin, LOW); _triacPin = PIN_UNUSED; }
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
  // _triacPin el gestiona cleanupTriac(); no resetegem aquí per evitar conflictes
}

// ── Inicialització perifèrics de sortida ─────────────────────────

void configuracioLlum() {
  cleanupTriac();
  cleanupTriacCycle();

  for (int gpio = 0; gpio <= 21; gpio++) {
    if (gpioMap[gpio].canal != 0) continue;
    switch (gpioMap[gpio].func) {
      case FUNC_NEOPIXEL:
        strip.updateLength(num_leds);
        strip.setPin(gpio);
        strip.begin();
        strip.clear();
        strip.show();
        break;
      case FUNC_LED:
        pinMode(gpio, OUTPUT);
        digitalWrite(gpio, LOW);
        break;
      default: break;
    }
  }

  if (_mosfetGpio != PIN_UNUSED) {
    ledcSetup(_pwmChMosfet, pwm_freq, PWM_RESOLUTION);
    ledcAttachPin(_mosfetGpio, _pwmChMosfet);
    ledcWrite(_pwmChMosfet, (uint32_t)map(pwm_duty, 0, 100, 0, 255));
    LOG_D("[CFG] MOSFET_PWM standalone gpio=%d ledc_ch=%d duty=%d%%", _mosfetGpio, _pwmChMosfet, pwm_duty);
  }

  for (int gpio = 0; gpio <= 21; gpio++) {
    if (gpioMap[gpio].func != FUNC_MOSFET || gpioMap[gpio].canal != 0) continue;
    if (pin1 == gpio) continue;
    pinMode(gpio, OUTPUT); digitalWrite(gpio, LOW);
  }

  for (uint8_t chan = 1; chan <= 15; chan++) {
    int relayPin = PIN_UNUSED, ledPin    = PIN_UNUSED;
    int pwmPin   = PIN_UNUSED, pwmWwPin  = PIN_UNUSED, pwmCwPin = PIN_UNUSED;
    int zcdPin   = PIN_UNUSED, triacPin  = PIN_UNUSED;

    for (int gpio = 0; gpio <= 21; gpio++) {
      if (gpioMap[gpio].canal != chan) continue;
      switch (gpioMap[gpio].func) {
        case FUNC_RELAY:  relayPin  = gpio; break;
        case FUNC_LED:    ledPin    = gpio; break;
        case FUNC_PWM:    pwmPin    = gpio; break;
        case FUNC_PWM_WW: pwmWwPin  = gpio; break;
        case FUNC_PWM_CW: pwmCwPin  = gpio; break;
        case FUNC_ZCD:    zcdPin    = gpio; break;
        case FUNC_TRIAC:  triacPin  = gpio; break;
        case FUNC_MOSFET: pinMode(gpio, OUTPUT); digitalWrite(gpio, LOW); break;
        default: break;
      }
    }

    if (relayPin != PIN_UNUSED) {
      pinMode(relayPin, OUTPUT);
      if (ledPin != PIN_UNUSED) { pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW); }
    }

    if (pwmPin != PIN_UNUSED) {
      ledcSetup(pwmCh1, pwm_freq, PWM_RESOLUTION);
      ledcAttachPin(pwmPin, pwmCh1);
      ledcWrite(pwmCh1, 0);
    } else if (pwmWwPin != PIN_UNUSED && pwmCwPin != PIN_UNUSED) {
      ledcSetup(pwmCh1, pwm_freq, PWM_RESOLUTION);
      ledcSetup(pwmCh2, pwm_freq, PWM_RESOLUTION);
      ledcAttachPin(pwmWwPin, pwmCh1);
      ledcAttachPin(pwmCwPin, pwmCh2);
    }

    if (zcdPin != PIN_UNUSED && triacPin != PIN_UNUSED) {
      // Triac fase
      _triacPin = triacPin;
      pinMode(triacPin, OUTPUT);
      digitalWrite(triacPin, LOW);
      if (pin3 != PIN_UNUSED) {
        strip.updateLength(1);
        strip.setPin(pin3);
        strip.begin();
        strip.clear();
        strip.show();
      }
      _zcdSemaphore = xSemaphoreCreateBinary();
      _zcdPin = zcdPin;
      pinMode(zcdPin, INPUT);
      attachInterrupt(digitalPinToInterrupt(zcdPin), zcdISR, RISING);
      xTaskCreatePinnedToCore(triacTask, "triac", 3072, NULL, 5, &_triacTaskHandle, 0);
    } else if (triacPin != PIN_UNUSED && control_type == CTRL_TYPE_TRIAC_CYCLE) {
      // Triac cicle (sense ZCD)
      _triacPin = triacPin;
      pinMode(triacPin, OUTPUT);
      digitalWrite(triacPin, LOW);
      if (pin3 != PIN_UNUSED) {
        strip.updateLength(1);
        strip.setPin(pin3);
        strip.begin();
        strip.clear();
        strip.show();
      }
      _initTriacCycle();
    }
  }
}

// ── applyChannelOutput — aplica hardware per canal des de g_dev ──

void applyChannelOutput(uint8_t chan) {
  const ChannelHW&    hw = g_dev.hw[chan];
  const ChannelState& s  = g_dev.state[chan];
  bool    on  = s.on;
  uint8_t br  = s.brightness;
  uint8_t br2 = s.brightness2;

  switch (hw.type) {
    case CH_ONOFF:
      if (hw.pin_out >= 0) digitalWrite(hw.pin_out, on ? HIGH : LOW);
      if (hw.pin_aux >= 0) digitalWrite(hw.pin_aux, on ? HIGH : LOW);
      break;

    case CH_PWM:
      if (hw.ledc_ch_main >= 0)
        ledcWrite(hw.ledc_ch_main, on ? (uint32_t)map(br, 0, 100, 0, 255) : 0);
      break;

    case CH_PWM_CCT:
      if (hw.ledc_ch_main >= 0)
        ledcWrite(hw.ledc_ch_main, on ? (uint32_t)map(br,  0, 100, 0, 255) : 0);
      if (hw.ledc_ch_aux >= 0)
        ledcWrite(hw.ledc_ch_aux,  on ? (uint32_t)map(br2, 0, 100, 0, 255) : 0);
      break;

    case CH_NEOPIXEL:
      strip.setBrightness(map(br, 0, 100, 0, 255));
      for (int i = 0; i < hw.num_leds; i++)
        strip.setPixelColor(i, on ? s.color : 0);
      strip.show();
      break;

    case CH_TRIAC_CYCLE:
      _cycleSnapUpdate(on, br);
      if (hw.pin_led >= 0) {
        if (on) {
          uint8_t ledBr = (uint8_t)map((long)br2 * br / 100, 0, 100, 0, 255);
          strip.setBrightness(max((uint8_t)5, ledBr));
          strip.setPixelColor(0, s.color);
        } else { strip.clear(); }
        strip.show();
      }
      break;

    case CH_TRIAC_PHASE:
      _triacSnapUpdate(on, br);
      if (hw.pin_led >= 0) {
        if (on) {
          uint8_t ledBr = (uint8_t)map((long)br2 * br / 100, 0, 100, 0, 255);
          strip.setBrightness(max((uint8_t)5, ledBr));
          strip.setPixelColor(0, s.color);
        } else { strip.clear(); }
        strip.show();
      }
      break;

    default: break;
  }
}

// ── Funcions de control de sortida ──────────────────────────────

void applyOutput(bool on) {
  if (pin1 == PIN_UNUSED || !g_driver) return;
  state = on;
  uint8_t ch = (uint8_t)gpioMap[pin1].canal;
  if (ch > 0) g_dev.state[ch].on = on;
  g_driver->applyOutput(on);
}

void toggleOutput() {
  if (pin1 == PIN_UNUSED) return;
  uint8_t ch = (uint8_t)gpioMap[pin1].canal;
  if (ch == 0) return;
  if (boto_canal > 0 && boto_canal != ch) return;
  toggleChannel(ch);
}

void renderVisualFeedback(const char* mode) {
  if (pin1 == PIN_UNUSED || !g_driver) return;
  if (strcmp(mode, "inici") == 0)  g_driver->renderInici();
  else if (strcmp(mode, "wifiAP") == 0) g_driver->renderAP();
}

void applyBrightness(int br) {
  brightness[control_type] = br;
  uint8_t ch = (pin1 != PIN_UNUSED) ? (uint8_t)gpioMap[pin1].canal : 0;
  if (ch > 0) g_dev.state[ch].brightness = (uint8_t)constrain(br, 0, 100);
  if (state) applyOutput(true);
}

float getAcFreqHz() {
  uint32_t p = _zcdPeriodUs;
  bool active = (p > 0) && ((uint32_t)(micros() - _zcdMicros) < 200000UL);
  return active ? (1000000.0f / (float)p) : 0.0f;
}

void applyMosfetDuty(int duty) {
  pwm_duty = duty;
  if (_mosfetGpio != PIN_UNUSED)
    ledcWrite(_pwmChMosfet, map(duty, 0, 100, 0, 255));
}
