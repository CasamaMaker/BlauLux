#pragma once
#include "globals.h"
#include <esp_system.h>

const char* resetReasonStr(esp_reset_reason_t r);
void logResetReason();
void clearConfig();
void loadConfig();
void saveConfig();
