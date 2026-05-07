#include "channel.h"
#include "globals.h"

// ── Nom llegible per als logs ─────────────────────────────────────
static const char* chTypeName(ChannelType t) {
  switch (t) {
    case CH_ONOFF:        return "onoff";
    case CH_PWM:          return "pwm";
    case CH_PWM_CCT:      return "pwm_cct";
    case CH_NEOPIXEL:     return "neopixel";
    case CH_TRIAC_CYCLE:  return "triac_cycle";
    case CH_TRIAC_PHASE:  return "triac_phase";
    default:              return "none";
  }
}

// ════════════════════════════════════════════════════════════════
//  resolveChannels — omple g_dev.hw[] i g_dev.buttons[]
//  llegint gpioMap[] (ja carregat per loadConfig + applyGpioConfig)
//
//  Fase 2: purament informativa — cap hardware ni lògica existent
//  es modifica. g_dev.state[] es manté als valors per defecte.
// ════════════════════════════════════════════════════════════════

void resolveChannels() {

  // ── Reset complet de g_dev.hw i buttons ───────────────────────
  for (int i = 0; i < 16; i++) g_dev.hw[i]      = ChannelHW();
  for (int i = 0; i <  4; i++) g_dev.buttons[i]  = ButtonHW();
  g_dev.num_buttons = 0;
  g_dev.mosfet_gpio = -1;
  g_dev.mosfet_ledc = -1;
  g_dev.mosfet_duty = pwm_duty;  // sincronitza amb el global existent

  int ledc_next = 0;  // comptador de canals LEDC per assignar (referència futura)

  // ── Botons (qualsevol canal, inclòs 0) ────────────────────────
  for (int g = 0; g <= 21 && g_dev.num_buttons < 4; g++) {
    GpioFunc f = gpioMap[g].func;
    if (f != FUNC_BTN && f != FUNC_BTN_INV) continue;
    ButtonHW& bh = g_dev.buttons[g_dev.num_buttons++];
    bh.gpio   = g;
    bh.pullup = (f == FUNC_BTN);  // FUNC_BTN → pullup true; FUNC_BTN_INV → false
    bh.canal  = gpioMap[g].canal;
  }

  // ── MOSFET autònom (FUNC_PWM, canal 0) ────────────────────────
  // Un PWM sense canal assignat és independent del sistema de canals.
  for (int g = 0; g <= 21; g++) {
    if (gpioMap[g].func == FUNC_PWM && gpioMap[g].canal == 0) {
      g_dev.mosfet_gpio = g;
      break;
    }
  }

  // ── Canals 1–15: cada canal resolt de forma independent ───────
  for (uint8_t chan = 1; chan <= 15; chan++) {
    int relay=-1, led=-1, pwm=-1, ww=-1, cw=-1;
    int zcd=-1, triac=-1, neo=-1, mosfet=-1;

    for (int g = 0; g <= 21; g++) {
      if (gpioMap[g].canal != chan) continue;
      switch (gpioMap[g].func) {
        case FUNC_RELAY:   relay  = g; break;
        case FUNC_LED:     led    = g; break;
        case FUNC_PWM:     pwm    = g; break;
        case FUNC_PWM_WW:  ww     = g; break;
        case FUNC_PWM_CW:  cw     = g; break;
        case FUNC_ZCD:     zcd    = g; break;
        case FUNC_TRIAC:   triac  = g; break;
        case FUNC_NEOPIXEL:neo    = g; break;
        case FUNC_MOSFET:  mosfet = g; break;
        default: break;
      }
    }

    ChannelHW& hw = g_dev.hw[chan];

    if (relay >= 0 || mosfet >= 0) {
      hw.type    = CH_ONOFF;
      hw.pin_out = (relay >= 0) ? relay : mosfet;
      hw.pin_aux = led;

    } else if (zcd >= 0 && triac >= 0) {
      hw.type    = CH_TRIAC_PHASE;
      hw.pin_out = triac;
      hw.pin_aux = zcd;
      hw.pin_led = neo;

    } else if (triac >= 0) {
      hw.type    = CH_TRIAC_CYCLE;
      hw.pin_out = triac;
      hw.pin_led = neo;

    } else if (neo >= 0) {
      hw.type     = CH_NEOPIXEL;
      hw.pin_out  = neo;
      hw.num_leds = num_leds;

    } else if (ww >= 0 && cw >= 0) {
      hw.type         = CH_PWM_CCT;
      hw.pin_out      = ww;
      hw.pin_aux      = cw;
      hw.ledc_ch_main = ledc_next++;
      hw.ledc_ch_aux  = ledc_next++;

    } else if (pwm >= 0) {
      hw.type         = CH_PWM;
      hw.pin_out      = pwm;
      hw.ledc_ch_main = ledc_next++;
    }
    // else: hw.type = CH_NONE (cap GPIO rellevant en aquest canal)
  }

  // LEDC del mosfet autònom (sempre després dels canals per no solapar)
  if (g_dev.mosfet_gpio >= 0) g_dev.mosfet_ledc = ledc_next++;

  // ── Log resum ──────────────────────────────────────────────────
  for (uint8_t ch = 1; ch <= 15; ch++) {
    const ChannelHW& hw = g_dev.hw[ch];
    if (hw.type == CH_NONE) continue;
    LOG_D("[CH] canal=%-2d type=%-12s p_out=%-2d p_aux=%-2d p_led=%-2d ledc=%d/%d",
          ch, chTypeName(hw.type),
          hw.pin_out, hw.pin_aux, hw.pin_led,
          hw.ledc_ch_main, hw.ledc_ch_aux);
  }
  for (int i = 0; i < g_dev.num_buttons; i++) {
    LOG_D("[CH] boto[%d] gpio=%-2d pullup=%d canal=%d",
          i, g_dev.buttons[i].gpio, (int)g_dev.buttons[i].pullup, g_dev.buttons[i].canal);
  }
  if (g_dev.mosfet_gpio >= 0) {
    LOG_D("[CH] mosfet_auto gpio=%-2d ledc=%d duty=%d",
          g_dev.mosfet_gpio, g_dev.mosfet_ledc, g_dev.mosfet_duty);
  }

  // ── Sembra g_dev.state[] des dels globals carregats per loadConfig() ──
  // Fase 3: estat inicial coherent amb la config NVS existent (schema v2).
  // L'estat on=false és intencional per seguretat (mai restaurem ON).
  for (uint8_t ch = 1; ch <= 15; ch++) {
    if (g_dev.hw[ch].type == CH_NONE) continue;
    g_dev.state[ch].on          = false;
    g_dev.state[ch].color       = currentColor;
    g_dev.state[ch].brightness2 = (uint8_t)constrain(brightness_cw, 0, 100);
    uint8_t br = BRIGHTNESS_DEF;
    if (control_type > 0 && control_type <= 4)
      br = (uint8_t)constrain(brightness[control_type], 0, 100);
    g_dev.state[ch].brightness = br;
  }
}
