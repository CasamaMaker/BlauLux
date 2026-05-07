#pragma once
#include "globals.h"

// ── OutputDriver — dispatch de sortida per control_type ──────────
struct OutputDriver {
  void (*applyOutput)(bool on);
  void (*renderInici)();
  void (*renderAP)();
  bool        hasColor;
  bool        hasBrightness;
  bool        hasCCT;
  const char* typeName;
  const char* haDiscoveryType;  // "switch" o "light"
};

extern const OutputDriver* g_driver;

// ── Funcions públiques ────────────────────────────────────────────
int   findGpio(GpioFunc func, uint8_t canal = 0);
void  applyGpioConfig();
void  cleanupTriac();
void  cleanupTriacCycle();
void  configuracioLlum();
void  applyChannelOutput(uint8_t chan);
void  applyOutput(bool on);
void  toggleOutput();
void  renderVisualFeedback(const char* mode);
void  applyBrightness(int br);
float getAcFreqHz();
void  applyMosfetDuty(int duty);
