#include "state.h"
#include "globals.h"
#include "output.h"

// ── Helpers ───────────────────────────────────────────────────────

// Retorna el canal del pin primari de sortida (0 si no assignat).
static inline uint8_t _primaryChan() {
  return (pin1 != PIN_UNUSED) ? (uint8_t)gpioMap[pin1].canal : 0;
}

// Sincronitza els globals legacy (state, brightness[], currentColor)
// quan el canal modificat és el canal primari (l'únic que MQTT/web observen).
static void _syncLegacy(uint8_t chan) {
  if (chan != _primaryChan() || chan == 0) return;
  state                  = g_dev.state[chan].on;
  brightness[control_type] = g_dev.state[chan].brightness;
  brightness_cw          = g_dev.state[chan].brightness2;
  currentColor           = g_dev.state[chan].color;
}

// ── API pública ───────────────────────────────────────────────────

void setChannelOn(uint8_t chan, bool on) {
  if (chan == 0 || chan > 15 || g_dev.hw[chan].type == CH_NONE) return;
  g_dev.state[chan].on = on;
  applyChannelOutput(chan);
  _syncLegacy(chan);
}

void setChannelBrightness(uint8_t chan, uint8_t br) {
  if (chan == 0 || chan > 15 || g_dev.hw[chan].type == CH_NONE) return;
  g_dev.state[chan].brightness = constrain(br, 0, 100);
  if (g_dev.state[chan].on) applyChannelOutput(chan);
  _syncLegacy(chan);
}

void setChannelBrightness2(uint8_t chan, uint8_t br2) {
  if (chan == 0 || chan > 15 || g_dev.hw[chan].type == CH_NONE) return;
  g_dev.state[chan].brightness2 = constrain(br2, 0, 100);
  if (g_dev.state[chan].on) applyChannelOutput(chan);
  _syncLegacy(chan);
}

void setChannelColor(uint8_t chan, uint32_t color) {
  if (chan == 0 || chan > 15 || g_dev.hw[chan].type == CH_NONE) return;
  g_dev.state[chan].color = color;
  if (g_dev.state[chan].on) applyChannelOutput(chan);
  _syncLegacy(chan);
}

void toggleChannel(uint8_t chan) {
  if (chan == 0 || chan > 15 || g_dev.hw[chan].type == CH_NONE) return;
  setChannelOn(chan, !g_dev.state[chan].on);
}
