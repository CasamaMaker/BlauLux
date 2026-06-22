#pragma once
#include "globals.h"
#include <esp_now.h>

void initEspNow();
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status);
void onDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int len);
uint8_t handleAction(uint8_t pktType, uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3);
void processEspNowPending();

// ── BlauProtocol v2 — seguretat (NVS namespace "blau_rx") ──
// Definides a espnow.cpp perquè l'estat v2 (blauprotocol_trg2.h)
// només pot viure en aquest mòdul.
bool loadSecurityConfig();                       // clau de NVS + whitelist/nonces (al setup)
bool saveSecurityPassword(const char *pwd);      // PBKDF2 -> aes_key a NVS (password no es guarda)
void clearSecurity();                            // esborra clau + whitelist + nonces -> mode v1
void securityStartLearning();                    // activa mode "afegir emissor" 60 s
bool securityConfigured();
int  securityWhitelistCount();
bool securityWhitelistMac(int idx, uint8_t out[6]);
