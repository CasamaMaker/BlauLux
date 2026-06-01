#pragma once
#include "output.h"

struct ButtonState {
  bool     apActive        = false;
  uint32_t apStart         = 0;
  bool     apBtnReleased   = false;
  uint32_t apBtnReleasedAt = 0;
  bool     apLastBtn       = false;
  bool     down            = false;
  uint32_t downTime        = 0;
  bool     debouncing      = false;
  uint32_t upTime          = 0;
  uint32_t lastDnsPoll     = 0;
};

extern ButtonState btn;

inline bool buttonPressed() {
  return digitalRead(getBotonPin()) == (getButtonPullup() ? LOW : HIGH);
}
