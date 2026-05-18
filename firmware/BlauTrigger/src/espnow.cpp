#include "espnow.h"
#include "output.h"
#include <blauprotocol.h>
#include <blauprotocol_trg.h>

// Estat ACK privat al mòdul — compartit entre onDataRecv (ISR) i processEspNowPending (loop)
static volatile bool         _ack_pending = false;
static volatile uint8_t      _ack_mac[6];
static volatile BlauPacket_t _ack_pkt;

void initEspNow() {
  if (esp_now_init() == ESP_OK) {
    LOG_I("[ESPNOW] Init OK");
  } else {
    LOG_E("[ESPNOW] Init Failed");
    ESP.restart();
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
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

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  uint8_t br = (g_driver && g_driver->hasBrightness) ? (uint8_t)getBrightnessForType(getControlType()) : 0;
  blau_trg_on_data_recv(mac, data, len,
                        &_ack_pending, (uint8_t*)_ack_mac, (BlauPacket_t*)&_ack_pkt,
                        handleAction,
                        getState(), br, (uint8_t)(getControlType() < 0 ? 0 : getControlType()));
}

void processEspNowPending() {
  blau_trg_process_pending(&_ack_pending, (const uint8_t*)_ack_mac, (const BlauPacket_t*)&_ack_pkt);
}
