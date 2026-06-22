#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"
#include <Preferences.h>
#include <AsyncMqttClient.h>
#include "config.h"
#include "log.h"

// ── Tipus GPIO ───────────────────────────────────────────────────

struct GpioConfig {       // persistent — guardat a NVS
  char     name[13];
  GpioFunc func;
  uint32_t param1;
  uint16_t param2;
  uint16_t param3;
  bool     notificador;
};

struct GpioRuntime {      // volatile — només RAM
  bool     initialized;   // si driverSetup() s'ha executat
  bool     state;         // estat actual (on/off)
  uint32_t lastChangeMs;  // ms de l'últim canvi
  uint32_t param1;        // valors live (poden diferir del cfg fins que es guarda)
  uint16_t param2;
  uint16_t param3;
  bool     dirty;         // true = pendent d'enviar a MQTT i web
};

struct Gpio {
  GpioConfig  cfg;
  GpioRuntime rt;
};

// ── Mapa GPIO ────────────────────────────────────────────────────
extern Gpio gpioMap[MAX_GPIO_COUNT];

// ── Configuració runtime ─────────────────────────────────────────
extern String device_name;
extern uint8_t powerupMode;    // 0=off, 1=on, 2=last_state
extern uint8_t lastSavedState; // 0=off, 1=on

// ── Estat ────────────────────────────────────────────────────────
extern bool webTesting;

// ── WiFi / AP ────────────────────────────────────────────────────
extern String macAP, macAPSuffix;
extern String sta_ssid, sta_pass;

// ── MQTT ─────────────────────────────────────────────────────────
extern String          mqtt_host, mqtt_user, mqtt_pass;
extern String          mqtt_client, mqtt_topic, mqtt_fulltopic;
extern uint16_t        mqtt_port;
extern String          mqttClientId, mqttWillTopic;
extern AsyncMqttClient mqttClient;
extern TimerHandle_t   mqttReconnectTimer;

// ── Template seleccionat ─────────────────────────────────────────
extern int8_t selectedTemplate;  // -1 = cap, 0..N = índex del template actiu

// ── NVS ──────────────────────────────────────────────────────────
extern Preferences prefs;

// ── Servidor Web / DNS ───────────────────────────────────────────
extern AsyncWebServer server;
extern DNSServer      dnsServer;
