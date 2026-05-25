<div align="center">

# BlauLux

**Controlador intel·ligent de càrregues AC basat en ESP32**

[![GitHub release](https://img.shields.io/github/release/CasamaMaker/BlauLux.svg)](https://github.com/CasamaMaker/BlauLux/releases)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange?logo=platformio)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino)](https://www.arduino.cc/)
[![License](https://img.shields.io/github/license/CasamaMaker/BlauLux)](LICENSE)
[![ESP32-C3](https://img.shields.io/badge/ESP32--C3-RISC--V-blue)](https://www.espressif.com/en/products/socs/esp32-c3)
[![Protocol](https://img.shields.io/badge/Protocol-ESP--NOW-informational)](https://www.espressif.com/en/solutions/low-power-solutions/esp-now)
[![MQTT](https://img.shields.io/badge/MQTT-Home%20Assistant-41BDF5?logo=homeassistant)](https://www.home-assistant.io/)

---

<!--
  📸 FOTO 1 — IMATGE PRINCIPAL DEL PROJECTE
  Descripció: Fotografia del dispositiu BlauLux muntat i en funcionament.
  Idealment: Placa ESP32-C3 amb el LED WS2812 encès (color verd o blanc),
  connectada a una llum o tira LED. Format horitzontal, fons neutre o fosc.
  Resolució recomanada: 1200×600 px o superior.
  Col·loca la imatge a: docs/img/hero.jpg
-->
<!-- ![BlauLux](docs/img/hero.jpg) -->

*Part receptora de l'ecosistema **BlauClick** — rep ordres sense fils d'un botó ESP-NOW i controla la càrrega connectada.*

</div>

---

## Taula de continguts

- [Ecosistema BlauClick](#ecosistema-blauclick)
- [Característiques](#característiques)
- [Modes de control](#modes-de-control)
- [Hardware](#hardware)
  - [Plantilles de dispositiu](#plantilles-de-dispositiu)
  - [Connexions](#connexions)
- [Primers passos](#primers-passos)
  - [Requisits](#requisits)
  - [Compilar i pujar](#compilar-i-pujar)
  - [Configuració inicial](#configuració-inicial)
- [Configuració](#configuració)
  - [Temps de compilació (config.h)](#temps-de-compilació-configh)
  - [Temps d'execució (Web UI)](#temps-dexecució-web-ui)
- [Ús](#ús)
  - [Botó físic](#botó-físic)
  - [Control remot via BlauClick](#control-remot-via-blauclick)
  - [Interfície web](#interfície-web)
- [MQTT i Home Assistant](#mqtt-i-home-assistant)
- [BlauProtocol](#blauprotocol)
- [Estructura del projecte](#estructura-del-projecte)
- [Resolució de problemes](#resolució-de-problemes)
- [Projectes relacionats](#projectes-relacionats)

---

## Ecosistema BlauClick

BlauLux és el **receptor** d'un sistema wireless complet per controlar llums i càrregues AC sense necessitat de router ni hub:

```
┌─────────────────┐    ESP-NOW (IEEE 802.11)   ┌──────────────────┐
│   BlauClick     │ ─────────────────────────► │      BlauLux     │
│  (botó sender)  │ ◄───────────── ACK ──────  │  (load receiver) │
│  Bateria · BLE  │                            │  ESP32  ·   WiFi │
└─────────────────┘                            └────────┬─────────┘
                                                        │
                                          ┌─────────────┼─────────────┐
                                          ▼             ▼             ▼
                                       Relé          LED RGB       Triac
                                     (On/Off)      (NeoPixel)    (AC dimmer)
```

La comunicació és **peer-to-peer a la capa MAC**, sense router de per mig. La latència és < 10 ms i el consum és mínim. Un sol BlauLux pot gestionar fins a **8 BlauClicks** simultàniament.

<!--
  📸 FOTO 2 — DIAGRAMA FÍSIC O MUNTATGE COMPLET
  Descripció: Fotografia dels dos dispositius junts (BlauLink + BlauLux),
  o bé un diagrama de blocs imprès / dibuixat a mà mostrant la connexió.
  Format horitzontal. Fons blanc o clar per contrast.
  Col·loca la imatge a: docs/img/ecosystem.jpg
-->
<!-- ![Ecosistema BlauLink + BlauLux](docs/img/ecosystem.jpg) -->

---

## Característiques

| Categoria | Detall |
|-----------|--------|
| **Comunicació** | ESP-NOW (connectionless, no cal router) |
| **Fiabilitat** | ACK per cada comanda + 3 reintents al sender |
| **Deduplicació** | Descarta paquets duplicats dins una finestra de 2 s |
| **Configuració** | Portal captiu web (CA / EN / ES) — sense app |
| **Persistència** | Configuració guardada a NVS (sobreviu talls de corrent) |
| **Domòtica** | WiFi STA + MQTT + autodescoberta Home Assistant |
| **Botó físic** | Toggle ràpid i entrada a mode config per pulsació llarga |
| **Multi-font** | Fins a 8 BlauLinks per un únic BlauLux |
| **Plataformes** | ESP32-C3 · ESP32 · ESP32-S3 · ESP32-S2 · ESP32-C6 |
| **Firmware** | v1.0 — PlatformIO + Arduino framework |

---

## Modes de control

BlauLux suporta **4 tipus de control** seleccionables via la interfície web:

| Mode  | Descripció | Hardware típic |
|------|------------|----------------|
| **On/Off**  | Sortida digital binària | Relé, MOSFET, LED |
| **PWM**  | 1 canal LEDC (5 kHz / 8-bit) | Tira LED monocolor o dos WW+CW |
| **Triac cicle**  | Control per cicle a 50 Hz (sense ZCD) | Dimmer AC senzill |
| **Triac fase**  | Control de fase amb ZCD (H11AA4 + MOC3021S) | Dimmer AC de precisió |
| **Led digital**  | Control de NeoPixel/WS2812 | Tira LED WS2812 |


---

## Hardware

### Plantilles de dispositiu

La interfície web ofereix **plantilles predefinides** per alguns dispositius:

| Plantilla | GPIOs preconfigurats | Ús |
|-----------|---------------------|-----|
| `PICO-CLICK` | BTN\_INV→5 · LED→6 | Placa de prototipat genèrica |
| `SONOFF_BASIC_R4` | BTN→9 · RELAY→4 · LED→6 | Interruptor Sonoff de paret |
| `AC_REGULATOR` | BTN→1 · ZCD→0 · TRIAC→4 · LED→5 | Dimmer AC de fase |
| `GL-C-309WL` | BTN→17 · LED→16 · ON\_OFF→18 | Control tira llums digitals


### Connexions

**PICO-CLICK (per defecte):**
```
GPIO 5  →  Botó (pull-down, premut = HIGH)
GPIO 6  →  Dades NeoPixel / WS2812
```

**SONOFF BASIC R4:**
```
GPIO 9  →  Botó integrat (pull-up, premut = LOW)
GPIO 4  →  Control de relé
GPIO 6  →  LED d'estat
```

**Dimmer AC de fase (AC_REGULATOR):**
```
GPIO 0  →  ZCD — sortida optoacoblador H11AA4 (pols actiu HIGH al zero-crossing)
GPIO 4  →  Porta triac — entrada optoacoblador MOC3021S (actiu HIGH per disparar)
GPIO 5  →  WS2812 (LED d'estat — ambre proporcional a la potència)
GPIO 1  →  Botó de configuració
```

> El temps de dispar es calcula com: `retard = (100 − potència%) × 10 ms / 100`.
> Dissenyat per a xarxa de 50 Hz. El pols de dispar del triac és de 100 µs.

<!--
  📸 FOTO 3 — ESQUEMA DE CONNEXIONS / WIRING
  Descripció: Captura de pantalla de l'esquemàtic (Fritzing, KiCad, EasyEDA)
  o fotografia del muntatge en protoboard mostrant les connexions clarament.
  Per al mode triac: inclou el H11AA4 i el MOC3021S amb la xarxa RC snubber.
  Format horitzontal. Etiquetes als pins visibles.
  Col·loca la imatge a: docs/img/wiring.png
-->
<!-- ![Esquema de connexions](docs/img/wiring.png) -->

---

## Primers passos

### Requisits

- [PlatformIO](https://platformio.org/) (CLI o extensió VSCode)
- Cable USB-C
- Placa ESP32-C3 (o compatible — vegeu plantilles)
- Driver USB-UART si cal (CH340, CP210x)

### Compilar i pujar

1. Clona el repositori:
   ```bash
   git clone https://github.com/CasamaMaker/BlauLux.git
   cd BlauLux/firmware/BlauLux
   ```

2. (Opcional) Edita [`src/config.h`](src/config.h) per seleccionar el target o ajustar paràmetres.

3. Compila i puja el firmware:
   ```bash
   pio run -e esp32c3 -t upload
   ```

4. Puja el sistema de fitxers (interfície web):
   ```bash
   pio run -e esp32c3 -t uploadfs
   ```

5. Obre el monitor sèrie per verificar l'arrencada:
   ```bash
   pio device monitor -b 115200
   ```

**Entorns disponibles:** `esp32c3` · `esp32` · `esp32s3` · `esp32s2` · `esp32c6`

### Configuració inicial

Al primer arrencament (o després d'esborrar la config), el dispositiu detecta que no té cap GPIO de botó ni wifi configurats i entra automàticament en mode AP:

1. Encén el BlauLux.
2. Des del mòbil o l'ordinador, connecta't a la xarxa **`BlauLux_XXXX`** (els 4 darrers caràcters de la MAC).
3. S'obre el portal captiu automàticament — o navega a `http://192.168.4.1`.
4. Selecciona el tipus de control, assigna les funcions als GPIOs i els paràmetres extra.
5. Prem **Desa**. El dispositiu reinicia i entra en operació normal.

<!--
  📸 FOTO 4 — PORTAL WEB DE CONFIGURACIÓ
  Descripció: Captura de pantalla del portal captiu obert al mòbil o al navegador
  d'escriptori. Ha de mostrar el formulari de configuració principal amb:
  selecció de tipus de control, assignació de GPIOs, slider de brillantor.
  Dues captures: una a mòbil (vista vertical) i una a escriptori (vista horitzontal).
  Col·loca les imatges a: docs/img/portal_mobile.png i docs/img/portal_desktop.png
-->
<!-- ![Portal captiu — mòbil](docs/img/portal_mobile.png) -->
<!-- ![Portal captiu — escriptori](docs/img/portal_desktop.png) -->

---

## Configuració

### Temps de compilació (`config.h`)

| Macro | Valor per defecte | Descripció |
|-------|-------------------|------------|
| `CLEAR_CONFIG` | *(comentat)* | Si es defineix, esborra tota la NVS a l'arrencada. Torna a comentar i repuja després. |
| `WIFI_SSID` | `"BlauLux"` | Prefix del nom de la xarxa AP (s'afegeix el sufix MAC automàticament) |
| `WIFI_PASSWORD` | `""` | Contrasenya de l'AP (buit = xarxa oberta) |
| `BRIGHTNESS_DEF` | `15` | Brillantor per defecte (0–100 %) |
| `PWM_FREQ` | `5000` | Freqüència LEDC en Hz |
| `PWM_RESOLUTION` | `8` | Resolució PWM en bits (8 → rang 0–255) |
| `WIFI_AP_HOLD_MS` | `3000` | Durada de pulsació (ms) per entrar a mode config |
| `WIFI_AP_TIMEOUT_MS` | `120000` | Temps màxim en mode AP abans de reiniciar |
| `ESPNOW_CHANNEL` | `1` | Canal WiFi per a ESP-NOW |
| `ENABLE_WIFI_STA` | *(definit)* | Comenta per desactivar la connexió a xarxa domèstica |
| `ENABLE_MQTT` | *(definit)* | Comenta per desactivar el client MQTT |
| `LOG_LEVEL` | `3` | 0=silenci · 1=error · 2=info · 3=debug |
| `CONFIG_SCHEMA_VERSION` | `4` | Incrementa quan canvies les claus NVS |
| `FIRMWARE_VERSION` | `"1.0"` | Versió del firmware (cadena de text) |

### Temps d'execució (Web UI)

Tots els paràmetres de hardware es poden canviar des de la interfície web (`http://192.168.4.1`):

- **Plantilla de dispositiu** — selecció predefinida de GPIO functions
- **Assignació de GPIOs** — funció per a cada pin (BTN, ON\_OFF, PWM, ZCD, TRIAC...)
- **Brillantor** — valor per defecte (0–100 %)
- **WiFi STA** — connexió a la xarxa domèstica per activar MQTT
- **MQTT** — broker, credencials i plantilles de topic
- **Previsualització en viu** — prova el color RGB o la brillantor abans de desar

---

## Ús

### Botó físic

| Acció | Resultat |
|-------|----------|
| Pulsació curta | Toggle de la càrrega (on/off) |
| Mantenir 3+ s | Entra al mode AP de configuració WiFi |
| Doble pulsació (en mode AP) | Surt del mode AP i reinicia |

El mode AP té un temps límit automàtic de 2 minuts (`WIFI_AP_TIMEOUT_MS`).

### Control remot via BlauLink

BlauLux escolta paquets ESP-NOW dels dispositius BlauLink. No cal cap parellament ni router — la comunicació és peer-to-peer a la capa MAC WiFi.

En rebre un paquet BlauProtocol vàlid, BlauLux:

1. Verifica el checksum CRC-8.
2. Descarta duplicats (mateix `src_id` + `seq` dins de 2 segons).
3. Executa la comanda (toggle, on, off, brillantor, color...).
4. Envia un paquet ACK al sender amb l'estat actual del dispositiu.

**Comandes suportades:** `TOGGLE` · `ON` · `OFF` · `SET_BRIGHTNESS` · `SET_RGB` · `SET_CCT` · `SET_SCENE` · `DIM_UP` · `DIM_DOWN`

<!-- **Events de botó suportats:** `CLICK_1` (1 clic) · `CLICK_2` (doble clic) · `CLICK_3` (triple clic) · `LONG_START/END` (pulsació llarga) -->

### Interfície web

L'API HTTP és accessible a `http://192.168.4.1` mentre el dispositiu és en mode AP:

| Endpoint | Mètode | Descripció |
|----------|--------|------------|
| `/` | `GET` / `POST` | Pàgina de configuració / desa nova config a NVS |
| `/color` | `POST` | Previsualitza color RGB (`r`, `g`, `b`, 0–255) |
| `/dutty` | `POST` | Previsualitza brillantor (`value`, 0–100) |
| `/duttyCW` | `POST` | Previsualitza blanc fred (`value`, 0–100, mode 3/4) |
| `/wifi` | `POST` | Desa credencials WiFi STA i reconnecta |
| `/mqtt` | `POST` | Desa configuració MQTT i reconnecta |
| `/mymac` | `POST` | Retorna la MAC de l'AP |
| `/pins` | `POST` | Retorna l'assignació de GPIOs (JSON) |
| `/brightness` | `POST` | Retorna la brillantor per mode (JSON) |
| `/wifiStatus` | `POST` | Retorna l'estat de connexió WiFi STA (JSON) |
| `/mqttStatus` | `POST` | Retorna l'estat i la config MQTT (JSON) |
| `/initialSetup` | `POST` | Retorna `"true"` si no hi ha cap GPIO de botó configurat |

---

## MQTT i Home Assistant

Quan es configura una xarxa WiFi STA, BlauLux es connecta a un broker MQTT i publica/subscriu als topics definits a `config.h`:

```
BlauLux/<topic>/state      ← estat actual (ON / OFF, brillantor, color)
BlauLux/<topic>/cmnd/...   ← comandes entrants
BlauLux/<topic>/tele/...   ← telemetria (LWT, IP, MAC, RSSI)
```

On `%id%` es resol automàticament com els **darrers 4 caràcters de la MAC** (ex: `A1B2`), fent que cada dispositiu tingui topics únics sense configuració addicional.

> **Home Assistant:** BlauLux publica el payload d'autodescoberta MQTT estàndard perquè el dispositiu aparegui automàticament a HA sense cap configuració manual.

<!--
  📸 FOTO 5 — HOME ASSISTANT
  Descripció: Captura de pantalla del panell de Home Assistant mostrant
  el dispositiu BlauLux integrat: entitats (light, switch), historial,
  o el dashboard amb el control de llum.
  Col·loca la imatge a: docs/img/homeassistant.png
-->
<!-- ![Integració Home Assistant](docs/img/homeassistant.png) -->

---

## BlauProtocol

BlauLux utilitza **BlauProtocol v1** — un protocol binari compacte de **10 bytes** dissenyat per a ESP-NOW:

```
Byte:  0      1      2      3-4        5      6    7    8    9
      [VER | TYPE | SEQ | SRC_ID(2B) | CMD | P1 | P2 | P3 | CRC8]
```

| Camp | Mida | Descripció |
|------|------|------------|
| `VER` | 1 B | Versió del protocol (`0x01`) |
| `TYPE` | 1 B | Tipus de missatge (EVENT, CMD, ACK, PING...) |
| `SEQ` | 1 B | Número de seqüència circular (0–255) per deduplicació |
| `SRC_ID` | 2 B | Identificador del sender (darrers 2 bytes de la MAC) |
| `CMD` | 1 B | Codi de comanda o event |
| `P1–P3` | 3 B | Paràmetres (brillantor, R/G/B, WW/CW...) |
| `CRC8` | 1 B | CRC-8 (polinomi 0x07) dels bytes 0–8 |

**Tipus de missatge:** `TYPE_EVENT` · `TYPE_CMD` · `TYPE_ACK` · `TYPE_PING` · `TYPE_PONG` · `TYPE_STATUS_REQ` · `TYPE_STATUS_RSP`

**Codis ACK:** `ACK_OK` · `ACK_ERROR` · `ACK_DUPLICATE` · `ACK_UNAUTHORIZED` · `ACK_BAD_VERSION` · `ACK_BAD_CRC`

**Temporitzacions:**

| Constant | Valor | Descripció |
|----------|-------|------------|
| `BLAU_ACK_TIMEOUT_MS` | 50 ms | Temps d'espera de l'ACK per intent |
| `BLAU_MAX_RETRIES` | 3 | Reintents màxims sense ACK |
| `BLAU_CLICK_WINDOW_MS` | 400 ms | Finestra de detecció de multi-clic |
| `BLAU_LONG_PRESS_MS` | 800 ms | Llindar de pulsació llarga |
| `BLAU_DEDUP_WINDOW_MS` | 2000 ms | Finestra de deduplicació al Trigger |
| `BLAU_MAX_SOURCES` | 8 | Màxim de BlauClicks per Trigger |
| `BLAU_MAX_TARGETS` | 4 | Màxim de Triggers per BlauClick |

Especificació completa: [`lib/BlauProtocol/blauprotocol.h`](lib/BlauProtocol/blauprotocol.h)

---

## Estructura del projecte

```
BlauLux/
├── src/
│   ├── main.cpp          # Lògica principal, setup, loop
│   ├── config.h          # Pinout, macros de compilació, constants
│   ├── globals.h         # Declaració de variables globals
│   ├── nvsconfig.h/.cpp  # Persistència NVS (Preferences)
│   ├── output.h/.cpp     # Control de sortides (relay, PWM, NeoPixel, triac)
│   ├── espnow.h/.cpp     # Receptor ESP-NOW i processament BlauProtocol
│   ├── webserver.h/.cpp  # Servidor HTTP i portal captiu
│   ├── mqtt.h/.cpp       # Client MQTT i autodescoberta HA
│   ├── button.h/.cpp     # Gestió del botó (debounce, multi-clic, long press)
│   └── watchdog.h/.cpp   # Watchdog i log del motiu de reset
├── lib/
│   └── BlauProtocol/
│       ├── blauprotocol.h        # Estructura del paquet, tipus, constants
│       ├── blauprotocol.cpp      # CRC-8, inicialització de paquets
│       ├── blauprotocol_trg.h    # Helpers pel Trigger (parse, dedup, ACK)
│       └── blauprotocol_link.h   # Helpers pel Link (sender)
├── data/
│   ├── wifimanager.html   # UI web multilingüe (CA / EN / ES via JS i18n)
│   └── style.css          # Estils de la interfície web
└── platformio.ini         # Configuració PlatformIO (multi-target)
```

---

## Resolució de problemes

| Problema | Causa probable | Solució |
|----------|---------------|---------|
| Sempre en mode AP a l'arrencada | GPIO del botó no configurat | Connecta't al portal i desa l'assignació de pins |
| No s'obre el portal captiu | Bloquejat per xarxa o DNS | Navega manualment a `http://192.168.4.1` |
| El LED no s'encén | Pin o mode de control incorrecte | Verifica el GPIO i el mode al portal web |
| No arriba ACK al BlauClick | Finestra de dedup expirada o paquet perdut | BlauClick reintenta fins a 3 vegades; comprova que el canal ESP-NOW coincideix (`ESPNOW_CHANNEL`) |
| La config no es desa | NVS plena o corrupte | Defineix `CLEAR_CONFIG`, puja el firmware, torna a comentar-ho i repuja |
| Error de compilació | Llibreria no trobada | Executa `pio pkg install` per descarregar les dependències |
| Port USB no detectat | Driver absent | Instal·la el driver CH340 o CP210x pel teu sistema operatiu |
| El dispositiu reinicia sol | Watchdog timeout | Consulta el serial monitor per veure el motiu del reset (`logResetReason`) |

---

## Projectes relacionats

- **[BlauClick](https://github.com/CasamaMaker/BlauClick)** — Botó wireless amb bateria (sender de l'ecosistema)

---

## Llicència

Aquest projecte és de codi obert. Vegeu [LICENSE](LICENSE) per als detalls.

---

<div align="center">

Fet amb ❤️ per [CasamaMaker](https://github.com/CasamaMaker)

</div>
