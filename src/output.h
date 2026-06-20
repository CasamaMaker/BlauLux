#pragma once
#include "globals.h"

// ── OutputDriver (mantingut per compat amb mqtt, espnow) ─────────
struct OutputDriver {
  void (*applyOutput)(bool on);
  void (*renderInici)();
  void (*renderAP)();
  bool        hasColor;
  bool        hasBrightness;
  bool        hasCCT;
  const char* typeName;
  const char* haDiscoveryType;
};

extern const OutputDriver* g_driver;

// ── API per-GPIO (nova) ───────────────────────────────────────────
int   findGpio(GpioFunc func);
void  driverSetup(int gpio);
void  driverSetupAll();
void  applyPowerupState();
void  driverApply(int gpio);
void  driverToggle(int gpio);
void  driverToggleAll();

// ── Cleanup i mesura ─────────────────────────────────────────────
void  cleanupTriac();
void  cleanupTriacCycle();
float getAcFreqHz();

// ── API backward-compatible (main, mqtt, espnow, webserver) ──────
void     applyGpioConfig();     // àlies → driverSetupAll()
void     configuracioLlum();    // no-op (integrat a driverSetupAll)
bool     getState();
void     applyOutput(bool on);
void     toggleOutput();
void     renderVisualFeedback(const char* mode);
void     applyBrightness(int br);
void     applyMosfetDuty(int duty);
int      getBotonPin();
bool     getButtonPullup();
int      getControlType();
int      getPin1();
int      getPin2();
int      getPin3();
int      getBrightnessForType(int ct);
void     setBrightnessForType(int ct, int v);
int      getBrightnessCW();
void     setBrightnessCW(int v);
uint32_t getCurrentColor();
void     setCurrentColor(uint32_t c);
