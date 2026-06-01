#pragma once
#include <esp_task_wdt.h>
#include <esp_idf_version.h>

#define WDT_TIMEOUT_S  30   // reinicia si loop() no s'executa en 30 s

inline void wdtSetup() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_task_wdt_config_t cfg = {
    .timeout_ms     = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true,
  };
  esp_task_wdt_reconfigure(&cfg);
  esp_task_wdt_add(NULL);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
#endif
}

inline void wdtReset() { esp_task_wdt_reset(); }
