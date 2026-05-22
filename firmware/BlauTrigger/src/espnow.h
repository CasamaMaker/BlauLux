#pragma once
#include "globals.h"
#include <esp_now.h>

void initEspNow();
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status);
void onDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int len);
uint8_t handleAction(uint8_t pktType, uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3);
void processEspNowPending();
