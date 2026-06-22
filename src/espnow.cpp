#include "espnow.h"
#include "output.h"
#include <blauprotocol.h>
#include <blauprotocol_trg.h>
#include <blauprotocol_crypto.h>
#include <blauprotocol_trg2.h>
#include <Preferences.h>

// Estat ACK privat al mòdul — compartit entre onDataRecv (ISR) i processEspNowPending (loop)
static volatile bool         _ack_pending = false;
static volatile uint8_t      _ack_mac[6];
static volatile BlauPacket_t _ack_pkt;
static volatile bool         _ack_is_v2 = false;  // la petició era v2 -> resposta xifrada

void initEspNow() {
  if (esp_now_init() == ESP_OK) {
    LOG_I("[ESPNOW] Init OK");
  } else {
    LOG_E("[ESPNOW] Init Failed");
    ESP.restart();
  }
  blau2_trg_tx_nonce_init();  // nonce aleatori per a les respostes xifrades (mesura 4)
}

// ════════════════════════════════════════════════════════════════
//  BlauProtocol v2 — gestió de seguretat (NVS "blau_rx")
// ════════════════════════════════════════════════════════════════

bool loadSecurityConfig() {
  bool ok = false;
  Preferences p;
  if (p.begin(BLAU2_NVS_NAMESPACE, true)) {
    uint8_t key[BLAU_AES_KEY_LEN];
    if (p.getBytesLength("aes_key") == BLAU_AES_KEY_LEN) {
      p.getBytes("aes_key", key, BLAU_AES_KEY_LEN);
      ok = blau_crypto_set_key(key);
    }
    p.end();
  }
  LOG_I("[SEC] Clau v2: %s", ok ? "configurada (mode segur)" : "no configurada (mode v1 legacy)");
  blau2_trg_load_state();  // whitelist + nonces per MAC (mesura 7/8)
  return ok;
}

bool saveSecurityPassword(const char *pwd) {
  uint8_t key[BLAU_AES_KEY_LEN];
  if (!blau_derive_key(pwd, key)) {
    LOG_E("[SEC] Error derivant clau PBKDF2");
    return false;
  }
  Preferences p;
  if (!p.begin(BLAU2_NVS_NAMESPACE, false)) return false;
  p.putBytes("aes_key", key, BLAU_AES_KEY_LEN);
  p.end();
  bool ok = blau_crypto_set_key(key);
  LOG_I("[SEC] Clau v2 derivada i guardada (password no persistit)");
  return ok;
}

void clearSecurity() {
  Preferences p;
  if (p.begin(BLAU2_NVS_NAMESPACE, false)) { p.clear(); p.end(); }
  blau_crypto_clear_key();
  blau2_trg_load_state();  // reset RAM: whitelist buida -> mode aprenentatge
  LOG_I("[SEC] Seguretat esborrada — mode v1 legacy");
}

bool securityConfigured() {
  return blau_crypto_has_key();
}

int securityWhitelistCount() {
  return _blau2_wl_count;
}

bool securityWhitelistMac(int idx, uint8_t out[6]) {
  if (idx < 0 || idx >= _blau2_wl_count) return false;
  memcpy(out, _blau2_wl[idx], 6);
  return true;
}

void securityStartLearning() {
  blau2_trg_start_learning(60000);
}

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  (void)info;
  LOG_I("[ESPNOW] ACK TX: %s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

uint8_t handleAction(uint8_t pktType, uint8_t cmd,
                     uint8_t p1, uint8_t p2, uint8_t p3) {
  if (pktType == TYPE_EVENT) {
    LOG_I("[ESPNOW] TYPE_EVENT no suportat (0x%02X), ignorat", cmd);
    return ACK_ERROR;
  }

  if (pktType == TYPE_CMD) {
    switch (cmd) {
      case CMD_TOGGLE:           toggleOutput(); return ACK_OK;
      case CMD_ON:   if (!getState()) applyOutput(true);  return ACK_OK;
      case CMD_OFF:  if ( getState()) applyOutput(false); return ACK_OK;

      case CMD_SET_BRIGHTNESS: {
        if (!g_driver || !g_driver->hasBrightness) return ACK_OK;
        applyBrightness(constrain((int)p1, 0, 100));
        return ACK_OK;
      }

      case CMD_SET_RGB: {
        if (!g_driver || !g_driver->hasColor) return ACK_OK;
        setCurrentColor(((uint32_t)p1 << 16) | ((uint32_t)p2 << 8) | p3);
        if (getState()) applyOutput(true);
        return ACK_OK;
      }

      case CMD_SET_CCT: {
        if (!g_driver || !g_driver->hasCCT) return ACK_OK;
        setBrightnessForType(getControlType(), constrain((int)p1, 0, 100));
        setBrightnessCW(constrain((int)p2, 0, 100));
        if (getState()) applyOutput(true);
        return ACK_OK;
      }

      case CMD_DIM_UP: {
        if (!g_driver || !g_driver->hasBrightness) return ACK_OK;
        int step = (p1 >= 1 && p1 <= 10) ? (int)p1 : 5;
        applyBrightness(constrain(getBrightnessForType(getControlType()) + step, 0, 100));
        return ACK_OK;
      }

      case CMD_DIM_DOWN: {
        if (!g_driver || !g_driver->hasBrightness) return ACK_OK;
        int step = (p1 >= 1 && p1 <= 10) ? (int)p1 : 5;
        int br = constrain(getBrightnessForType(getControlType()) - step, 0, 100);
        setBrightnessForType(getControlType(), br);
        if (br == 0 && getState()) applyOutput(false);
        else if (getState())       applyOutput(true);
        return ACK_OK;
      }

      case CMD_SET_SCENE: {
        struct { uint8_t br; uint8_t r, g, b; uint8_t cw; } scenes[] = {
          { 0,   0,   0,   0,   0 },
          { 100, 255, 200, 100, 100 },
          { 30,  255, 150,  50,  15 },
          { 5,   255,  60,   0,   0 },
        };
        if (p1 >= sizeof(scenes) / sizeof(scenes[0])) return ACK_ERROR;
        auto& sc = scenes[p1];
        if (p1 == 0) {
          if (getState()) applyOutput(false);
          return ACK_OK;
        }
        if (g_driver && g_driver->hasBrightness) setBrightnessForType(getControlType(), sc.br);
        if (g_driver && g_driver->hasColor)      setCurrentColor(((uint32_t)sc.r << 16) | ((uint32_t)sc.g << 8) | sc.b);
        if (g_driver && g_driver->hasCCT)        setBrightnessCW(sc.cw);
        applyOutput(true);
        return ACK_OK;
      }

      default:
        LOG_I("[ESPNOW] CMD no implementat: 0x%02X", cmd);
        return ACK_ERROR;
    }
  }

  return ACK_ERROR;
}

void onDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int len) {
  const uint8_t *mac = recv_info->src_addr;
  LOG_I("[ESPNOW] RX mac=%02X:%02X:%02X:%02X:%02X:%02X len=%d",
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], len);
  uint8_t br = (g_driver && g_driver->hasBrightness) ? (uint8_t)getBrightnessForType(getControlType()) : 0;
  uint8_t ctrl = (uint8_t)(getControlType() < 0 ? 0 : getControlType());

  // Dispatch v2 / v1
  if (len == (int)BLAU_V2_PACKET_SIZE && data[0] == BLAU_PROTO_VERSION_V2) {
    _ack_is_v2 = true;
    blau2_trg_on_data_recv(mac, data, len,
                           &_ack_pending, (uint8_t*)_ack_mac, (BlauPacket_t*)&_ack_pkt,
                           handleAction, getState(), br, ctrl);
    return;
  }

  // Anti-downgrade: amb clau v2 configurada no s'accepta v1 en clar
  if (blau_crypto_has_key()) {
    LOG_I("[SEC] Paquet v1 rebutjat: clau v2 configurada (anti-downgrade)");
    return;
  }

  _ack_is_v2 = false;
  blau_trg_on_data_recv(mac, data, len,
                        &_ack_pending, (uint8_t*)_ack_mac, (BlauPacket_t*)&_ack_pkt,
                        handleAction,
                        getState(), br, ctrl);
}

void processEspNowPending() {
  blau2_trg_process_pending(&_ack_pending, (const uint8_t*)_ack_mac,
                            (const BlauPacket_t*)&_ack_pkt, &_ack_is_v2);
}
