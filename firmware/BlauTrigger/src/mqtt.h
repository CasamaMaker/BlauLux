#pragma once
#include "globals.h"

String mqttResolvedTopic();
String mqttBaseTopic();
String mqttResolvedClientId();
void publishState();
void publishChannelState(uint8_t chan);
void publishHADiscovery();
void publishChannelDiscovery(uint8_t chan);
void connectMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char* topic, char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total);
