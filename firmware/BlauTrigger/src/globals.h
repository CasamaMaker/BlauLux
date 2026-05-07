#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <AsyncMqttClient.h>
#include "config.h"
#include "channel.h"
#include "state.h"
#include "log.h"

// ── Tipus GPIO (runtime) ─────────────────────────────────────────
struct GpioAssign { GpioFunc func; uint8_t canal; };

// ── Mapa GPIO i noms ─────────────────────────────────────────────
extern GpioAssign gpioMap[22];
extern char       gpio_names[22][13];

// ── Pins derivats i mode de control ─────────────────────────────
extern int control_type;
extern int pin1, pin2, pin3;
extern int boto_pin, button_pullup, boto_canal;
extern int num_leds, brightness_cw, pwm_freq, pwm_duty;
extern String device_name;

// ── Hardware ─────────────────────────────────────────────────────
extern Adafruit_NeoPixel strip;
extern int      brightness[5];
extern int      pwmCh1, pwmCh2;
extern uint32_t currentColor;

// ── Estat ────────────────────────────────────────────────────────
extern bool state;
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

// ── NVS ──────────────────────────────────────────────────────────
extern Preferences prefs;

// ── Servidor Web / DNS ───────────────────────────────────────────
extern AsyncWebServer server;
extern DNSServer      dnsServer;
