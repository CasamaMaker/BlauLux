# MQTT i Home Assistant — Pla d'implementació

## Estat actual del firmware

**Hardware:** ESP32-C3 (RISC-V, Espressif)
**Plataforma:** PlatformIO, Arduino framework
**Filesystem:** LittleFS (HTML/CSS del portal web)
**Persistència:** ESP32 Preferences API (NVS)

### Connectivitat existent
| Protocol | Mode | Ús |
|----------|------|----|
| ESP-NOW | AP canal 1 | Rep comandes del BlauLink (botó sense fils) |
| WiFi AP | `BlauLux_XXXX` | Portal de configuració web (192.168.4.1) |
| HTTP | AsyncWebServer | Configuració de pins, mode, colors, brillantor |

**No hi ha WiFi STA ni client MQTT.**

### Modes de control suportats
| Mode | Descripció | Hardware |
|------|------------|----------|
| 0 | On/Off relay | GPIO digital |
| 1 | RGB NeoPixel | WS2812 (Adafruit NeoPixel) |
| 2 | Dimmer PWM 1 canal | LEDC (brillantor 0-100%) |
| 3 | Warm/Cold white | LEDC 2 canals (temperatura color) |
| 4 | Triac phase-cut AC | ZCD H11AA4 + MOC3021S optoacoblador |

### Fitxers clau
```
firmware/BlauLux/
├── src/
│   ├── main.cpp              # Lògica principal (~785 línies)
│   └── config.h              # Definicions hardware compile-time
├── lib/BlauProtocol/
│   ├── blauprotocol.h        # Estructura de paquet 10 bytes + CRC-8
│   ├── blauprotocol.cpp      # CRC-8, inicialització de paquets
│   ├── blauprotocol_trg.h    # Helpers del receptor (BlauLux)
│   └── blauprotocol_link.h   # Helpers del emissor (BlauLink)
├── data/
│   ├── wifimanager_CAT.html  # Portal web en català
│   ├── wifimanager_EN.html   # Portal web en anglès
│   └── style.css
└── platformio.ini
```

---

## Repte principal: canal WiFi vs ESP-NOW

L'ESP-NOW opera al canal 1 perquè el dispositiu és AP pur. En mode **AP+STA simultani**, el canal del AP s'adapta al router domèstic. Si el router no usa el canal 1, el BlauLink perd la connexió.

**Solució:** Forçar el canal WiFi a 1 al router, o documentar-ho com a requisit de configuració. A ESP32-C3, AP+STA en el mateix canal permet coexistència completa.

---

## Fase 1 — WiFi STA + credencials

**Objectiu:** Connectar el BlauLux a la xarxa domèstica sense trencar el portal web ni l'ESP-NOW.

### Canvis a `platformio.ini`
Sense canvis de dependències en aquesta fase.

### Canvis a `config.h`
```c
// Nous camps de configuració
#define NVS_WIFI_SSID   "wifi_ssid"
#define NVS_WIFI_PASS   "wifi_pass"
```

### Canvis a `main.cpp`

**NVS — lectura de credencials WiFi:**
```cpp
String wifi_ssid = prefs.getString(NVS_WIFI_SSID, "");
String wifi_pass = prefs.getString(NVS_WIFI_PASS, "");
```

**Arrancada WiFi en mode AP+STA:**
```cpp
WiFi.mode(WIFI_AP_STA);
WiFi.softAP(ap_ssid, nullptr, 1);  // Canal forçat a 1
if (wifi_ssid.length() > 0) {
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
}
```

**Indicació d'estat amb NeoPixel:**
- Parpelleig blau lent → connectant a WiFi
- Blau fix → WiFi connectat
- Comportament original → sense credencials WiFi

**Portal web — nous inputs a `wifimanager_CAT.html` i `wifimanager_EN.html`:**
```html
<input type="text" name="wifi_ssid" placeholder="SSID del router">
<input type="password" name="wifi_pass" placeholder="Contrasenya WiFi">
```

**Nou endpoint POST per guardar credencials WiFi:**
```cpp
server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *req) {
    prefs.putString(NVS_WIFI_SSID, req->getParam("wifi_ssid", true)->value());
    prefs.putString(NVS_WIFI_PASS, req->getParam("wifi_pass", true)->value());
    req->send(200);
    ESP.restart();
});
```

---

## Fase 2 — Client MQTT bàsic

**Objectiu:** Publicar canvis d'estat i subscriure's a comandes de control.

### Canvis a `platformio.ini`
```ini
lib_deps =
    ...
    marvinroger/AsyncMqttClient@^0.9.0
    heman/AsyncTCP@^3.3.1   ; ja existent
```

> **Alternativa:** `knolleary/PubSubClient` (síncrona, més simple però bloquejant). Preferir `AsyncMqttClient` per mantenir el patró async del projecte.

### Nous camps NVS (`config.h`)
```c
#define NVS_MQTT_HOST   "mqtt_host"
#define NVS_MQTT_PORT   "mqtt_port"    // default 1883
#define NVS_MQTT_USER   "mqtt_user"
#define NVS_MQTT_PASS   "mqtt_pass"
#define NVS_MQTT_ID     "mqtt_id"      // default BlauLux_{mac}
```

### Topics MQTT

**Identificador base:** `BlauLux/{id}` on `{id}` = últims 4 caràcters de la MAC.

| Topic | Direcció | Valor | Quan |
|-------|----------|-------|------|
| `BlauLux/{id}/state` | Publish | `ON` / `OFF` | En cada canvi d'estat |
| `BlauLux/{id}/brightness` | Publish | `0`-`100` | En canvi de brillantor |
| `BlauLux/{id}/rgb` | Publish | `R,G,B` | En canvi de color RGB |
| `BlauLux/{id}/cct` | Publish | `warm,cold` | En canvi CCT |
| `BlauLux/{id}/set` | Subscribe | `ON` / `OFF` | Comanda on/off |
| `BlauLux/{id}/brightness/set` | Subscribe | `0`-`100` | Comanda brillantor |
| `BlauLux/{id}/rgb/set` | Subscribe | `R,G,B` | Comanda color |
| `BlauLux/{id}/available` | Publish LWT | `online` / `offline` | Connexió/desconnexió |

### Estructura del codi (`main.cpp`)

```cpp
#include <AsyncMqttClient.h>
AsyncMqttClient mqttClient;

void onMqttConnect(bool sessionPresent) {
    mqttClient.subscribe("BlauLux/{id}/set", 1);
    mqttClient.subscribe("BlauLux/{id}/brightness/set", 1);
    mqttClient.subscribe("BlauLux/{id}/rgb/set", 1);
    mqttClient.publish("BlauLux/{id}/available", 1, true, "online");
    publishState();  // Publicar estat actual en connectar
}

void onMqttMessage(char* topic, char* payload, ...) {
    // Parsear topic i cridar handleAction() amb la comanda corresponent
    // handleAction() ja existeix i gestiona tots els modes
}

void publishState() {
    mqttClient.publish("BlauLux/{id}/state", 1, true, state ? "ON" : "OFF");
    // + brillantor, RGB, etc. segons el mode actiu
}
```

**Qualsevol canvi d'estat** (per botó físic, ESP-NOW, o MQTT) ha de cridar `publishState()`.

---

## Fase 3 — Home Assistant MQTT Discovery

**Objectiu:** El dispositiu apareix automàticament a Home Assistant sense configuració manual.

### Com funciona la descoberta

HA escolta el prefix `homeassistant/`. En connectar al broker, el dispositiu publica un JSON de configuració a:
- **Llum** (modes 1/2/3/4): `homeassistant/light/{id}/config`
- **Interruptor** (mode 0): `homeassistant/switch/{id}/config`

Els payloads es publiquen amb `retain: true`.

### Payload per a entitat `light` (modes 1/2/3/4)

```json
{
  "name": "BlauLux Sala",
  "unique_id": "BlauLux_XXXX",
  "state_topic": "BlauLux/XXXX/state",
  "command_topic": "BlauLux/XXXX/set",
  "brightness_state_topic": "BlauLux/XXXX/brightness",
  "brightness_command_topic": "BlauLux/XXXX/brightness/set",
  "brightness_scale": 100,
  "rgb_state_topic": "BlauLux/XXXX/rgb",
  "rgb_command_topic": "BlauLux/XXXX/rgb/set",
  "availability_topic": "BlauLux/XXXX/available",
  "payload_available": "online",
  "payload_not_available": "offline",
  "device": {
    "identifiers": ["BlauLux_XXXX"],
    "name": "BlauLux",
    "model": "BlauLux v1",
    "manufacturer": "Blau",
    "sw_version": "1.0.0"
  }
}
```

### Payload per a entitat `switch` (mode 0 — relay)

```json
{
  "name": "BlauLux Relay",
  "unique_id": "BlauLux_XXXX",
  "state_topic": "BlauLux/XXXX/state",
  "command_topic": "BlauLux/XXXX/set",
  "payload_on": "ON",
  "payload_off": "OFF",
  "availability_topic": "BlauLux/XXXX/available",
  "device": { ... }
}
```

### Codi de publicació (`main.cpp`)

```cpp
void publishHADiscovery() {
    String id = getDeviceId();  // últims 4 caràcters MAC
    String topic;
    String payload;

    if (control_type == 0) {
        topic = "homeassistant/switch/BlauLux_" + id + "/config";
        // construir payload switch...
    } else {
        topic = "homeassistant/light/BlauLux_" + id + "/config";
        // construir payload light (incloure brightness, rgb segons mode)
    }
    mqttClient.publish(topic.c_str(), 1, true, payload.c_str());
}
```

> **Nota:** Generar el JSON manualment (sprintf/String) sense `ArduinoJson` per evitar dependència addicional, o afegir `ArduinoJson` si la complexitat ho justifica.

---

## Fase 4 — Poliment i robustesa

### Reconnexió automàtica MQTT
```cpp
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    if (WiFi.isConnected()) {
        // Reintent amb backoff exponencial (màx 60s)
        xTimerStart(mqttReconnectTimer, 0);
    }
}
```

### LWT (Last Will Testament)
Configurar en la connexió inicial perquè el broker publiqui `offline` automàticament si el dispositiu perd connexió:
```cpp
mqttClient.setWill("BlauLux/{id}/available", 1, true, "offline");
```

### OTA (opcional)
Aprofitar el WiFi STA per habilitar actualitzacions over-the-air:
```cpp
#include <ArduinoOTA.h>
// setup(): ArduinoOTA.begin()
// loop(): ArduinoOTA.handle()
```

### Portal web — informació MQTT
Afegir a la pàgina de configuració:
- Estat de connexió MQTT (connectat/desconnectat)
- IP assignada per DHCP
- Topic base actiu

---

## Resum de riscos

| Risc | Impacte | Mitigació |
|------|---------|-----------|
| Canal WiFi ≠ canal 1 → ESP-NOW falla | Alt | Documentar requisit: router al canal 1, o forçar-lo |
| MQTT async + ESP-NOW IRQ simultanis | Mitjà | `publishState()` cridada des del loop(), no des d'IRQ |
| RAM insuficient (ESP32-C3 ~160KB usable) | Baix | AsyncMqttClient és lleuger; monitoritzar amb `ESP.getFreeHeap()` |
| JSON discovery malformat → HA ignora | Baix | Validar amb MQTT Explorer abans de flashar |

---

## Ordre d'implementació recomanat

```
Fase 1  →  WiFi STA + credencials al portal
Fase 2  →  Client MQTT + publish/subscribe bàsic
Fase 3  →  HA Discovery (automàtic)
Fase 4  →  Reconnexió robusta + LWT + OTA
```

Cada fase és independent i testejable per separat.
