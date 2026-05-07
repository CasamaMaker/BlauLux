#pragma once
#include <Arduino.h>

// ════════════════════════════════════════════════════════════════
//  STATE — API centralitzada per modificar l'estat dels canals
//  Fase 3: punt d'entrada únic per a tot canvi d'estat.
//  Aplica hardware via applyChannelOutput() i sincronitza els
//  globals legacy (state, brightness[], currentColor) per al canal
//  primari, de manera que MQTT i webserver segueixen funcionant.
// ════════════════════════════════════════════════════════════════

void setChannelOn(uint8_t chan, bool on);
void setChannelBrightness(uint8_t chan, uint8_t br);
void setChannelBrightness2(uint8_t chan, uint8_t br2);
void setChannelColor(uint8_t chan, uint32_t color);
void toggleChannel(uint8_t chan);
