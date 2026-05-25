#pragma once
// Canvia LOG_LEVEL a config.h: 0=silent  1=error  2=info  3=debug

#ifndef LOG_LEVEL
  #define LOG_LEVEL 2
#endif

#if LOG_LEVEL >= 1
  #define LOG_E(fmt, ...) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_E(fmt, ...) do {} while(0)
#endif

#if LOG_LEVEL >= 2
  #define LOG_I(fmt, ...) Serial.printf("[I] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_I(fmt, ...) do {} while(0)
#endif

#if LOG_LEVEL >= 3
  #define LOG_D(fmt, ...) Serial.printf("[D] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_D(fmt, ...) do {} while(0)
#endif
