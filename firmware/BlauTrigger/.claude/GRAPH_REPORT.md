# Graph Report - .  (2026-05-17)

## Corpus Check
- Corpus is ~32,144 words - fits in a single context window. You may not need a graph.

## Summary
- 331 nodes · 569 edges · 11 communities detected
- Extraction: 75% EXTRACTED · 25% INFERRED · 0% AMBIGUOUS · INFERRED: 142 edges (avg confidence: 0.81)
- Token cost: 12,500 input · 3,200 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Web UI Frontend|Web UI Frontend]]
- [[_COMMUNITY_Channel Output Control|Channel Output Control]]
- [[_COMMUNITY_BlauProtocol Core|BlauProtocol Core]]
- [[_COMMUNITY_Firmware Main & Config|Firmware Main & Config]]
- [[_COMMUNITY_Architecture Concepts|Architecture Concepts]]
- [[_COMMUNITY_Protocol Design Rationale|Protocol Design Rationale]]
- [[_COMMUNITY_GPIO Config UI|GPIO Config UI]]
- [[_COMMUNITY_MQTT & HA Integration|MQTT & HA Integration]]
- [[_COMMUNITY_Brightness & I18n UI|Brightness & I18n UI]]
- [[_COMMUNITY_Protocol Constants|Protocol Constants]]
- [[_COMMUNITY_AC Cycle Timer|AC Cycle Timer]]

## God Nodes (most connected - your core abstractions)
1. `webServerSetup()` - 18 edges
2. `t()` - 16 edges
3. `setup()` - 16 edges
4. `buildSubRow()` - 11 edges
5. `_postForm()` - 10 edges
6. `mqttBaseTopic()` - 10 edges
7. `buildGpioRow()` - 9 edges
8. `blau_fill_crc()` - 9 edges
9. `blau_init_packet()` - 9 edges
10. `onMqttMessage()` - 9 edges

## Surprising Connections (you probably didn't know these)
- `fetchGpioMap()` --calls--> `apiGetGpioMap()`  [INFERRED]
  data\js\gpio.js → data\js\api.js
- `setup()` --calls--> `initEspNow()`  [INFERRED]
  src\main.cpp → src\espnow.cpp
- `webServerSetup()` --calls--> `saveConfig()`  [INFERRED]
  src\webserver.cpp → src\nvsconfig.cpp
- `scanWifi()` --calls--> `apiScanWifi()`  [INFERRED]
  data\js\app.js → data\js\api.js
- `confirmClearWifi()` --calls--> `apiClearWifi()`  [INFERRED]
  data\js\app.js → data\js\api.js

## Hyperedges (group relationships)
- **ESP-NOW deferred ACK pattern: ISR â†’ flag â†’ loop() â†’ send** — espnow_cpp_onDataRecv, espnow_cpp_processEspNowPending, blauprotocol_trg_h_TriggerSide, rationale_ISR_deferred_send [EXTRACTED 1.00]
- **GPIO configuration pipeline: JS UI â†’ HTTP API â†’ firmware gpioMap â†’ resolveChannels â†’ g_dev** — gpio_js_GpioConfigurator, api_js_ApiLayer, globals_h_GlobalState, channel_cpp_resolveChannels, channel_h_DeviceRuntime [INFERRED 0.95]
- **BlauProtocol packet lifecycle: init â†’ fill CRC â†’ send â†’ parse â†’ validate â†’ action callback â†’ ACK** — blauprotocol_cpp_CoreImpl, blauprotocol_link_h_LinkSide, blauprotocol_trg_h_TriggerSide, blauprotocol_trg_h_ActionCallback, espnow_cpp_handleAction [EXTRACTED 1.00]
- **State Change Pipeline (Web/MQTT/ESP-NOW â†’ State API â†’ Output â†’ Sync Legacy)** — webserver_cpp_channelControl, state_cpp_setChannelOn, output_cpp_applyChannelOutput, state_cpp_syncLegacy, mqtt_h_publishState [INFERRED 0.95]
- **Triac Phase-Cut Control Pipeline (ZCD ISR â†’ Semaphore â†’ FreeRTOS Task â†’ GPIO pulse)** — output_cpp_zcdISR, output_cpp_triacTask, output_cpp_applyChannelOutput [EXTRACTED 1.00]
- **GPIO Configuration Boot Sequence (load NVS â†’ applyGpioConfig â†’ configuracioLlum)** — nvsconfig_cpp_loadConfig, output_cpp_applyGpioConfig, output_cpp_configuracioLlum [INFERRED 0.95]

## Communities

### Community 0 - "Web UI Frontend"
Cohesion: 0.05
Nodes (76): apiClearConfig(), apiClearDeviceName(), apiClearHardware(), apiClearMqtt(), apiClearWifi(), apiGetAcFreq(), apiGetChannels(), apiGetConfigMode() (+68 more)

### Community 1 - "Channel Output Control"
Cohesion: 0.07
Nodes (27): chTypeName(), resolveChannels(), handleAction(), applyBrightness(), applyChannelOutput(), applyGpioConfig(), applyMosfetDuty(), applyOutput() (+19 more)

### Community 2 - "BlauProtocol Core"
Cohesion: 0.11
Nodes (26): blau_check_crc(), blau_crc8(), blau_fill_crc(), blau_get_src_id(), blau_init_packet(), blau_build_cmd_packet(), blau_build_event_packet(), blau_build_ping_packet() (+18 more)

### Community 3 - "Firmware Main & Config"
Cohesion: 0.12
Nodes (27): buttonPressed(), getChannelCaps(), configDeviceAP(), getMyMacAddress(), loop(), setup(), wifiApModeServer(), connectMqtt() (+19 more)

### Community 4 - "Architecture Concepts"
Cohesion: 0.08
Nodes (33): JS API Layer (api.js), JS App Controller (app.js), BlauProtocol core implementation (CRC8, packet init, src_id), BlauPacket_t â€” 10-byte fixed binary packet, BlauProtocol Link-side helpers (build EVENT/CMD/PING, send-with-ACK), blau_action_fn_t â€” action callback prototype, Deduplication table (_BlauSourceRecord_t / blau_is_duplicate), BlauProtocol Trigger-side helpers (parse, dedup, ACK/PONG/STATUS) (+25 more)

### Community 5 - "Protocol Design Rationale"
Cohesion: 0.07
Nodes (25): Application-Level ACK (vs MAC-Level ACK), BlauPacket_t 10-byte Binary Wire Format, CRC-8 Integrity (poly 0x07) on BlauPacket, Deduplication by (src_id, seq) within 2s window, BlauProtocol v1 Specification, ISR Callback Non-Blocking Rule (ACK from loop only), Coding Guidelines Document, No Dynamic Allocation at Runtime Rule (+17 more)

### Community 6 - "GPIO Config UI"
Cohesion: 0.17
Nodes (26): apiGetFunclist(), apiGetTemplates(), applyMcuProfile(), applyTemplate(), buildGpioRow(), buildGpioTable(), buildSubRow(), defaultParams() (+18 more)

### Community 7 - "MQTT & HA Integration"
Cohesion: 0.14
Nodes (13): publishState(), WiFi Channel Constraint (ESP-NOW vs STA coexistence), Home Assistant MQTT Auto-Discovery, MQTT Home Assistant Implementation Plan, applyChannelOutput(), BlauTrigger Specs (specs.md), setChannelBrightness(), setChannelColor() (+5 more)

### Community 8 - "Brightness & I18n UI"
Cohesion: 0.2
Nodes (9): apiGetBrightness(), apiGetDriverMode(), fetchBrightnessConfig(), fetchDriveMode(), updateControlTypeUI(), updateMosfetTest(), updateExtraParams(), applyTranslations() (+1 more)

### Community 20 - "Protocol Constants"
Cohesion: 1.0
Nodes (1): BlauProtocol v1 â€” packet type and command constants

### Community 23 - "AC Cycle Timer"
Cohesion: 1.0
Nodes (1): cycleTimerCallback() esp_timer

## Knowledge Gaps
- **24 isolated node(s):** `BlauProtocol v1 â€” packet type and command constants`, `Deduplication table (_BlauSourceRecord_t / blau_is_duplicate)`, `DEVICE_TEMPLATES â€” hardware pin presets`, `GpioCaps â€” per-GPIO capability tables (ESP32C3/S3)`, `ChannelType enum (CH_ONOFF, CH_PWM, CH_PWM_CCT, CH_NEOPIXEL, CH_TRIAC_*)` (+19 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Protocol Constants`** (1 nodes): `BlauProtocol v1 â€” packet type and command constants`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `AC Cycle Timer`** (1 nodes): `cycleTimerCallback() esp_timer`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `loop()` connect `Firmware Main & Config` to `Channel Output Control`, `BlauProtocol Core`?**
  _High betweenness centrality (0.031) - this node is a cross-community bridge._
- **Why does `processEspNowPending()` connect `BlauProtocol Core` to `Firmware Main & Config`?**
  _High betweenness centrality (0.026) - this node is a cross-community bridge._
- **Why does `setup()` connect `Firmware Main & Config` to `Channel Output Control`, `BlauProtocol Core`?**
  _High betweenness centrality (0.025) - this node is a cross-community bridge._
- **Are the 17 inferred relationships involving `webServerSetup()` (e.g. with `wifiApModeServer()` and `setup()`) actually correct?**
  _`webServerSetup()` has 17 INFERRED edges - model-reasoned connections that need verification._
- **Are the 15 inferred relationships involving `t()` (e.g. with `confirmClearConfig()` and `confirmRestart()`) actually correct?**
  _`t()` has 15 INFERRED edges - model-reasoned connections that need verification._
- **Are the 13 inferred relationships involving `setup()` (e.g. with `wdtSetup()` and `logResetReason()`) actually correct?**
  _`setup()` has 13 INFERRED edges - model-reasoned connections that need verification._
- **What connects `BlauProtocol v1 â€” packet type and command constants`, `Deduplication table (_BlauSourceRecord_t / blau_is_duplicate)`, `DEVICE_TEMPLATES â€” hardware pin presets` to the rest of the system?**
  _24 weakly-connected nodes found - possible documentation gaps or missing edges._