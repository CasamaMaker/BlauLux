#pragma once
#include <Arduino.h>   // uint8_t, uint32_t, bool — necessari per ser auto-contingut
#include "config.h"

// ════════════════════════════════════════════════════════════════
//  CHANNEL — Model de dades per canal (Fase 1 de la nova arquitectura)
//  Solament declaracions de tipus i la variable g_dev.
//  Cap lògica existent no es modifica en aquesta fase.
// ════════════════════════════════════════════════════════════════


// ── Tipus de canal (determinat pels GPIOs assignats) ─────────────
enum ChannelType : uint8_t {
  CH_NONE         = 0,
  CH_ONOFF,          // relay / mosfet / LED digital
  CH_PWM,            // PWM single (1 GPIO)
  CH_PWM_CCT,        // PWM dual WW+CW (2 GPIOs)
  CH_NEOPIXEL,       // WS2812/NeoPixel (1 GPIO)
  CH_TRIAC_CYCLE,    // triac sense ZCD (esp_timer 100 Hz)
  CH_TRIAC_PHASE,    // triac amb ZCD (FreeRTOS task + ISR)
};


// ── Recursos hardware resolts per canal ──────────────────────────
struct ChannelHW {
  ChannelType type      = CH_NONE;
  int  pin_out          = -1;  // relay / triac / pwm_ww / neopixel / mosfet
  int  pin_aux          = -1;  // led-feedback / pwm_cw / zcd
  int  pin_led          = -1;  // neopixel opcional de feedback (triac)
  int  ledc_ch_main     = -1;  // canal LEDC per pin_out (PWM)
  int  ledc_ch_aux      = -1;  // canal LEDC per pin_aux (CCT)
  int  num_leds         = 1;   // nombre de LEDs (CH_NEOPIXEL)
};


// ── Estat runtime per canal ───────────────────────────────────────
struct ChannelState {
  bool     on          = false;
  uint8_t  brightness  = 15;        // 0–100
  uint8_t  brightness2 = 15;        // CCT: canal fred; triac: LED feedback
  uint32_t color       = 0xFFFFFF;  // 0xRRGGBB (CH_NEOPIXEL)
};


// ── Capacitats derivades del ChannelType (sense estat, sols lectura) ─
struct ChannelCaps {
  bool        hasOnOff;
  bool        hasBrightness;
  bool        hasCCT;
  bool        hasColor;
  const char* haDiscoveryType;  // "switch" o "light"
};

inline ChannelCaps getChannelCaps(ChannelType t) {
  switch (t) {
    case CH_ONOFF:        return { true,  false, false, false, "switch" };
    case CH_PWM:          return { true,  true,  false, false, "light"  };
    case CH_PWM_CCT:      return { true,  true,  true,  false, "light"  };
    case CH_NEOPIXEL:     return { true,  true,  false, true,  "light"  };
    case CH_TRIAC_CYCLE:  return { true,  true,  false, false, "light"  };
    case CH_TRIAC_PHASE:  return { true,  true,  false, false, "light"  };
    default:              return { false, false, false, false, nullptr  };
  }
}


// ── Configuració hardware d'un botó físic ────────────────────────
// Separat de ButtonState (button.h) que gestiona l'estat de detecció.
struct ButtonHW {
  int     gpio   = -1;
  bool    pullup = true;
  uint8_t canal  = 0;  // canal que controla (0 = no assignat)
};


// ── Estat complet del dispositiu ─────────────────────────────────
struct DeviceRuntime {
  ChannelHW    hw[16];      // índex = canal (0 reservat, 1–15 actius)
  ChannelState state[16];   // índex = canal

  ButtonHW     buttons[4];  // fins a 4 botons físics
  int          num_buttons  = 0;

  int  mosfet_gpio  = -1;   // MOSFET autònom (FUNC_PWM canal 0)
  int  mosfet_ledc  = -1;   // canal LEDC assignat
  int  mosfet_duty  = 50;   // duty cycle % (0–100)
};

extern DeviceRuntime g_dev;

// ── Resolució de canals des de gpioMap ───────────────────────────
// Omple g_dev.hw[] i g_dev.buttons[]. Crida-la al final d'applyGpioConfig().
// No modifica cap variable existent — és purament additiva.
void resolveChannels();
