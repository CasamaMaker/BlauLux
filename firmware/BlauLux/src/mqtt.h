#pragma once
#include "globals.h"

String mqttResolvedTopic();
String mqttBaseTopic();
String mqttResolvedClientId();
void publishState();
void publishHADiscovery();
void connectMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char* topic, char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total);
