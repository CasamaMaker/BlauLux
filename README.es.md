<div align="center">

# BlauLux ⚡

**Controlador inteligente de cargas AC basado en ESP32**

[![GitHub release](https://img.shields.io/github/release/CasamaMaker/BlauLux.svg)](https://github.com/CasamaMaker/BlauLux/releases)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange?logo=platformio)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino)](https://www.arduino.cc/)
[![License](https://img.shields.io/github/license/CasamaMaker/BlauLux)](LICENSE)
[![ESP32-C3](https://img.shields.io/badge/ESP32--C3-RISC--V-blue)](https://www.espressif.com/en/products/socs/esp32-c3)
[![Protocol](https://img.shields.io/badge/Protocol-ESP--NOW-informational)](https://www.espressif.com/en/solutions/low-power-solutions/esp-now)
[![MQTT](https://img.shields.io/badge/MQTT-Home%20Assistant-41BDF5?logo=homeassistant)](https://www.home-assistant.io/)

[English](README.md) |
[Català](README.cat.md) |
[Español](README.es.md)
---

<!--
  📸 FOTO 1 — IMAGEN PRINCIPAL DEL PROYECTO
  Descripción: Fotografía del dispositivo BlauLux montado y en funcionamiento.
  Idealmente: Placa ESP32-C3 con el LED WS2812 encendido (color verde o blanco),
  conectada a una luz o tira LED. Formato horizontal, fondo neutro o oscuro.
  Resolución recomendada: 1200×600 px o superior.
  Coloca la imagen en: docs/img/hero.jpg
-->
<!-- ![BlauLux](docs/img/hero.jpg) -->

*Parte receptora del ecosistema **Blau** — recibe órdenes inalámbricas de un botón ESP-NOW y controla la carga conectada.*

</div>

---

[🌐 Ecosistema](#ecosistema-blau) · [✨ Características](#características) · [🎛️ Modos](#modos-de-control) · [🔌 Hardware](#hardware) · [🚀 Primeros pasos](#primeros-pasos) · [⚙️ Configuración](#configuración) · [📖 Uso](#uso) · [🏠 MQTT & HA](#mqtt-y-home-assistant) · [📡 Protocolo](#blauprotocol) · [📁 Estructura](#estructura-del-proyecto) · [🔧 Resolución](#resolución-de-problemas) · [🔗 Relacionados](#proyectos-relacionados)

---

## 🌐 Ecosistema Blau

BlauLux es el **receptor** de un sistema inalámbrico completo para controlar luces y cargas AC sin necesidad de router ni hub:

```
┌─────────────────┐    ESP-NOW (IEEE 802.11)   ┌──────────────────┐
│    BlauClick    │ ─────────────────────────► │     BlauLux      │
│  (botón sender) │ ◄───────────── ACK ──────  │  (load receiver) │
│  Batería · BLE  │                            │  ESP32  ·   WiFi │
└─────────────────┘                            └────────┬─────────┘
                                                        │
                                          ┌─────────────┼─────────────┐
                                          ▼             ▼             ▼
                                        Relé          LED RGB       Triac
                                      (On/Off)      (NeoPixel)   (AC dimmer)
```

La comunicación es **peer-to-peer en la capa MAC**, sin router de por medio. La latencia es < 10 ms y el consumo es mínimo. Un solo BlauLux puede gestionar hasta **8 BlauClicks** simultáneamente.

<!--
  📸 FOTO 2 — DIAGRAMA FÍSICO O MONTAJE COMPLETO
  Descripción: Fotografía de los dos dispositivos juntos (BlauClick + BlauLux),
  o bien un diagrama de bloques impreso / dibujado a mano mostrando la conexión.
  Formato horizontal. Fondo blanco o claro para contraste.
  Coloca la imagen en: docs/img/ecosystem.jpg
-->
<!-- ![Ecosistema BlauClick + BlauLux](docs/img/ecosystem.jpg) -->

---

## ✨ Características

- 📡 **Comunicación** ESP-NOW peer-to-peer sin router (latencia < 10 ms)
- ✅ **Fiabilidad** ACK por cada comando + 3 reintentos automáticos en el sender
- 🔁 **Deduplicación** descarta paquetes duplicados dentro de una ventana de 2 s
- 🌐 **Configuración** portal cautivo web (CA / EN / ES) sin ninguna app
- 💾 **Persistencia** configuración guardada en NVS (sobrevive cortes de corriente)
- 🏠 **Domótica** WiFi STA + MQTT + autodescubrimiento Home Assistant
- 🔘 **Botón físico** toggle rápido y entrada a modo config por pulsación larga
- 👥 **Multi-fuente** hasta 8 BlauClicks por un único BlauLux
- 🖥️ **Plataformas** ESP32-C3 · ESP32 · ESP32-S3 · ESP32-S2 · ESP32-C6
- 🔧 **Firmware** v1.0 — PlatformIO + Arduino framework

---

## 🎛️ Modos de control

BlauLux soporta **4 tipos de control** seleccionables desde la interfaz web:

| Modo | Descripción | Hardware típico |
|------|-------------|----------------|
| **On/Off** | Salida digital binaria | Relé, MOSFET, LED |
| **PWM** | 1 canal LEDC (5 kHz / 8-bit) | Tira LED monocolor o dos WW+CW |
| **Triac ciclo** | Control por ciclo a 50 Hz (sin ZCD) | Dimmer AC sencillo |
| **Triac fase** | Control de fase con ZCD (H11AA4 + MOC3021S) | Dimmer AC de precisión |
| **Led digital** | Control de NeoPixel/WS2812 | Tira LED WS2812 |

---

## 🔌 Hardware

### 📋 Plantillas de dispositivo

La interfaz web ofrece **plantillas predefinidas** para algunos dispositivos:

| Plantilla | GPIOs preconfigurados | Uso |
|-----------|----------------------|-----|
| `PICO-CLICK` | BTN\_INV→5 · LED→6 | Placa de prototipado genérica |
| `SONOFF_BASIC_R4` | BTN→9 · RELAY→4 · LED→6 | Interruptor Sonoff de pared |
| `AC_REGULATOR` | BTN→1 · ZCD→0 · TRIAC→4 · LED→5 | Dimmer AC de fase |
| `GL-C-309WL` | BTN→17 · LED→16 · ON\_OFF→18 | Control tira luces digitales |

### 🔧 Conexiones

**PICO-CLICK (por defecto):**
```
GPIO 5  →  Botón (pull-down, pulsado = HIGH)
GPIO 6  →  Datos NeoPixel / WS2812
```

**SONOFF BASIC R4:**
```
GPIO 9  →  Botón integrado (pull-up, pulsado = LOW)
GPIO 4  →  Control de relé
GPIO 6  →  LED de estado
```

**Dimmer AC de fase (AC_REGULATOR):**
```
GPIO 0  →  ZCD — salida optoacoplador H11AA4 (pulso activo HIGH en el cruce por cero)
GPIO 4  →  Puerta triac — entrada optoacoplador MOC3021S (activo HIGH para disparar)
GPIO 5  →  WS2812 (LED de estado — ámbar proporcional a la potencia)
GPIO 1  →  Botón de configuración
```

> El tiempo de disparo se calcula como: `retardo = (100 − potencia%) × 10 ms / 100`.
> Diseñado para red de 50 Hz. El pulso de disparo del triac es de 100 µs.

<!--
  📸 FOTO 3 — ESQUEMA DE CONEXIONES / CABLEADO
  Descripción: Captura de pantalla del esquemático (Fritzing, KiCad, EasyEDA)
  o fotografía del montaje en protoboard mostrando las conexiones claramente.
  Para el modo triac: incluye el H11AA4 y el MOC3021S con la red RC snubber.
  Formato horizontal. Etiquetas en los pines visibles.
  Coloca la imagen en: docs/img/wiring.png
-->
<!-- ![Esquema de conexiones](docs/img/wiring.png) -->

---

## 🚀 Primeros pasos

### 📦 Requisitos

- [PlatformIO](https://platformio.org/) (CLI o extensión VSCode)
- Cable USB-C
- Placa ESP32-C3 (o compatible — ver plantillas)
- Driver USB-UART si es necesario (CH340, CP210x)

### 💾 Compilar y subir

1. Clona el repositorio:
   ```bash
   git clone https://github.com/CasamaMaker/BlauLux.git
   cd BlauLux/firmware/BlauLux
   ```

2. (Opcional) Edita [`src/config.h`](src/config.h) para seleccionar el target o ajustar parámetros.

3. Compila y sube el firmware:
   ```bash
   pio run -e esp32c3 -t upload
   ```

4. Sube el sistema de archivos (interfaz web):
   ```bash
   pio run -e esp32c3 -t uploadfs
   ```

5. Abre el monitor serie para verificar el arranque:
   ```bash
   pio device monitor -b 115200
   ```

**Entornos disponibles:** `esp32c3` · `esp32` · `esp32s3` · `esp32s2` · `esp32c6`

### 🔑 Configuración inicial

En el primer arranque (o después de borrar la config), el dispositivo detecta que no tiene ningún GPIO de botón ni WiFi configurados y entra automáticamente en modo AP:

1. Enciende el BlauLux.
2. Desde el móvil o el ordenador, conéctate a la red **`BlauLux_XXXX`** (los 4 últimos caracteres de la MAC).
3. El portal cautivo se abre automáticamente — o navega a `http://192.168.4.1`.
4. Selecciona el tipo de control, asigna las funciones a los GPIOs y los parámetros extra.
5. Pulsa **Guardar**. El dispositivo reinicia y entra en operación normal.

<!--
  📸 FOTO 4 — PORTAL WEB DE CONFIGURACIÓN
  Descripción: Captura de pantalla del portal cautivo abierto en el móvil o en el navegador
  de escritorio. Debe mostrar el formulario de configuración principal con:
  selección del tipo de control, asignación de GPIOs, slider de brillo.
  Dos capturas: una en móvil (vista vertical) y una en escritorio (vista horizontal).
  Coloca las imágenes en: docs/img/portal_mobile.png y docs/img/portal_desktop.png
-->
<!-- ![Portal cautivo — móvil](docs/img/portal_mobile.png) -->
<!-- ![Portal cautivo — escritorio](docs/img/portal_desktop.png) -->

<p align="center">
  <img src="docs/web-manager.png" width="400">
</p>

---

## ⚙️ Configuración

### 🕐 Tiempo de compilación (`config.h`)

| Macro | Valor por defecto | Descripción |
|-------|------------------|-------------|
| `CLEAR_CONFIG` | *(comentado)* | Si se define, borra toda la NVS en el arranque. Vuelve a comentar y re-sube después. |
| `WIFI_SSID` | `"BlauLux"` | Prefijo del nombre de la red AP (se añade el sufijo MAC automáticamente) |
| `WIFI_PASSWORD` | `""` | Contraseña del AP (vacío = red abierta) |
| `BRIGHTNESS_DEF` | `15` | Brillo por defecto (0–100 %) |
| `PWM_FREQ` | `5000` | Frecuencia LEDC en Hz |
| `PWM_RESOLUTION` | `8` | Resolución PWM en bits (8 → rango 0–255) |
| `WIFI_AP_HOLD_MS` | `3000` | Duración de pulsación (ms) para entrar en modo config |
| `WIFI_AP_TIMEOUT_MS` | `120000` | Tiempo máximo en modo AP antes de reiniciar |
| `ESPNOW_CHANNEL` | `1` | Canal WiFi para ESP-NOW |
| `ENABLE_WIFI_STA` | *(definido)* | Comenta para desactivar la conexión a red doméstica |
| `ENABLE_MQTT` | *(definido)* | Comenta para desactivar el cliente MQTT |
| `LOG_LEVEL` | `3` | 0=silencio · 1=error · 2=info · 3=debug |
| `CONFIG_SCHEMA_VERSION` | `4` | Incrementa cuando cambies las claves NVS |
| `FIRMWARE_VERSION` | `"1.0"` | Versión del firmware (cadena de texto) |

### 🌍 Tiempo de ejecución (Web UI)

Todos los parámetros de hardware se pueden cambiar desde la interfaz web (`http://192.168.4.1`):

- **Plantilla de dispositivo** — selección predefinida de funciones GPIO
- **Asignación de GPIOs** — función para cada pin (BTN, ON\_OFF, PWM, ZCD, TRIAC...)
- **Brillo** — valor por defecto (0–100 %)
- **WiFi STA** — conexión a la red doméstica para activar MQTT
- **MQTT** — broker, credenciales y plantillas de topic
- **Previsualización en vivo** — prueba el color RGB o el brillo antes de guardar

---

## 📖 Uso

### 🔘 Botón físico

| Acción | Resultado |
|--------|-----------|
| Pulsación corta | Toggle de la carga (on/off) |
| Mantener 3+ s | Entra en el modo AP de configuración WiFi |
| Doble pulsación (en modo AP) | Sale del modo AP y reinicia |

El modo AP tiene un tiempo límite automático de 2 minutos (`WIFI_AP_TIMEOUT_MS`).

### 📡 Control remoto via BlauClick

BlauLux escucha paquetes ESP-NOW de los dispositivos BlauClick. No es necesario ningún emparejamiento ni router — la comunicación es peer-to-peer en la capa MAC WiFi.

Al recibir un paquete BlauProtocol válido, BlauLux:

1. Verifica el checksum CRC-8.
2. Descarta duplicados (mismo `src_id` + `seq` dentro de 2 segundos).
3. Ejecuta el comando (toggle, on, off, brillo, color...).
4. Envía un paquete ACK al sender con el estado actual del dispositivo.

**Comandos soportados:** `TOGGLE` · `ON` · `OFF` · `SET_BRIGHTNESS` · `SET_RGB` · `SET_CCT` · `SET_SCENE` · `DIM_UP` · `DIM_DOWN`

<!-- **Eventos de botón soportados:** `CLICK_1` (1 clic) · `CLICK_2` (doble clic) · `CLICK_3` (triple clic) · `LONG_START/END` (pulsación larga) -->

### 🌐 Interfaz web

La API HTTP es accesible en `http://192.168.4.1` mientras el dispositivo está en modo AP:

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/` | `GET` / `POST` | Página de configuración / guarda nueva config en NVS |
| `/color` | `POST` | Previsualiza color RGB (`r`, `g`, `b`, 0–255) |
| `/dutty` | `POST` | Previsualiza brillo (`value`, 0–100) |
| `/duttyCW` | `POST` | Previsualiza blanco frío (`value`, 0–100, modo 3/4) |
| `/wifi` | `POST` | Guarda credenciales WiFi STA y reconecta |
| `/mqtt` | `POST` | Guarda configuración MQTT y reconecta |
| `/mymac` | `POST` | Devuelve la MAC del AP |
| `/pins` | `POST` | Devuelve la asignación de GPIOs (JSON) |
| `/brightness` | `POST` | Devuelve el brillo por modo (JSON) |
| `/wifiStatus` | `POST` | Devuelve el estado de conexión WiFi STA (JSON) |
| `/mqttStatus` | `POST` | Devuelve el estado y la config MQTT (JSON) |
| `/initialSetup` | `POST` | Devuelve `"true"` si no hay ningún GPIO de botón configurado |

---

## 🏠 MQTT y Home Assistant

Cuando se configura una red WiFi STA, BlauLux se conecta a un broker MQTT y publica/suscribe en los topics definidos en `config.h`:

```
BlauLux/<topic>/state      ← estado actual (ON / OFF, brillo, color)
BlauLux/<topic>/cmnd/...   ← comandos entrantes
BlauLux/<topic>/tele/...   ← telemetría (LWT, IP, MAC, RSSI)
```

Donde `%id%` se resuelve automáticamente como los **últimos 4 caracteres de la MAC** (ej: `A1B2`), haciendo que cada dispositivo tenga topics únicos sin configuración adicional.

> **Home Assistant:** BlauLux publica el payload de autodescubrimiento MQTT estándar para que el dispositivo aparezca automáticamente en HA sin ninguna configuración manual.

<!--
  📸 FOTO 5 — HOME ASSISTANT
  Descripción: Captura de pantalla del panel de Home Assistant mostrando
  el dispositivo BlauLux integrado: entidades (light, switch), historial,
  o el dashboard con el control de luz.
  Coloca la imagen en: docs/img/homeassistant.png
-->
<!-- ![Integración Home Assistant](docs/img/homeassistant.png) -->

---

## 📡 BlauProtocol

BlauLux utiliza **BlauProtocol v1** — un protocolo binario compacto de **10 bytes** diseñado para ESP-NOW:

```
Byte:  0      1      2      3-4        5      6    7    8    9
      [VER | TYPE | SEQ | SRC_ID(2B) | CMD | P1 | P2 | P3 | CRC8]
```

| Campo | Tamaño | Descripción |
|-------|--------|-------------|
| `VER` | 1 B | Versión del protocolo (`0x01`) |
| `TYPE` | 1 B | Tipo de mensaje (EVENT, CMD, ACK, PING...) |
| `SEQ` | 1 B | Número de secuencia circular (0–255) para deduplicación |
| `SRC_ID` | 2 B | Identificador del sender (últimos 2 bytes de la MAC) |
| `CMD` | 1 B | Código de comando o evento |
| `P1–P3` | 3 B | Parámetros (brillo, R/G/B, WW/CW...) |
| `CRC8` | 1 B | CRC-8 (polinomio 0x07) de los bytes 0–8 |

**Tipos de mensaje:** `TYPE_EVENT` · `TYPE_CMD` · `TYPE_ACK` · `TYPE_PING` · `TYPE_PONG` · `TYPE_STATUS_REQ` · `TYPE_STATUS_RSP`

**Códigos ACK:** `ACK_OK` · `ACK_ERROR` · `ACK_DUPLICATE` · `ACK_UNAUTHORIZED` · `ACK_BAD_VERSION` · `ACK_BAD_CRC`

**Temporizaciones:**

| Constante | Valor | Descripción |
|-----------|-------|-------------|
| `BLAU_ACK_TIMEOUT_MS` | 50 ms | Tiempo de espera del ACK por intento |
| `BLAU_MAX_RETRIES` | 3 | Reintentos máximos sin ACK |
| `BLAU_CLICK_WINDOW_MS` | 400 ms | Ventana de detección de multi-clic |
| `BLAU_LONG_PRESS_MS` | 800 ms | Umbral de pulsación larga |
| `BLAU_DEDUP_WINDOW_MS` | 2000 ms | Ventana de deduplicación en el Trigger |
| `BLAU_MAX_SOURCES` | 8 | Máximo de BlauClicks por Trigger |
| `BLAU_MAX_TARGETS` | 4 | Máximo de Triggers por BlauClick |

Especificación completa: [`lib/BlauProtocol/blauprotocol.h`](lib/BlauProtocol/blauprotocol.h)

---

## 📁 Estructura del proyecto

```
BlauLux/
├── src/
│   ├── main.cpp          # Lógica principal, setup, loop
│   ├── config.h          # Pinout, macros de compilación, constantes
│   ├── globals.h         # Declaración de variables globales
│   ├── nvsconfig.h/.cpp  # Persistencia NVS (Preferences)
│   ├── output.h/.cpp     # Control de salidas (relé, PWM, NeoPixel, triac)
│   ├── espnow.h/.cpp     # Receptor ESP-NOW y procesamiento BlauProtocol
│   ├── webserver.h/.cpp  # Servidor HTTP y portal cautivo
│   ├── mqtt.h/.cpp       # Cliente MQTT y autodescubrimiento HA
│   ├── button.h/.cpp     # Gestión del botón (debounce, multi-clic, long press)
│   └── watchdog.h/.cpp   # Watchdog y log del motivo de reset
├── lib/
│   └── BlauProtocol/
│       ├── blauprotocol.h        # Estructura del paquete, tipos, constantes
│       ├── blauprotocol.cpp      # CRC-8, inicialización de paquetes
│       ├── blauprotocol_trg.h    # Helpers para el Trigger (parse, dedup, ACK)
│       └── blauprotocol_link.h   # Helpers para el Link (sender)
├── data/
│   ├── wifimanager.html   # UI web multilingüe (CA / EN / ES via JS i18n)
│   └── style.css          # Estilos de la interfaz web
└── platformio.ini         # Configuración PlatformIO (multi-target)
```

---

## 🔧 Resolución de problemas

| Problema | Causa probable | Solución |
|----------|---------------|---------|
| Siempre en modo AP en el arranque | GPIO del botón no configurado | Conéctate al portal y guarda la asignación de pines |
| No se abre el portal cautivo | Bloqueado por red o DNS | Navega manualmente a `http://192.168.4.1` |
| El LED no se enciende | Pin o modo de control incorrecto | Verifica el GPIO y el modo en el portal web |
| No llega ACK al BlauClick | Ventana de dedup expirada o paquete perdido | BlauClick reintenta hasta 3 veces; comprueba que el canal ESP-NOW coincide (`ESPNOW_CHANNEL`) |
| La config no se guarda | NVS llena o corrupta | Define `CLEAR_CONFIG`, sube el firmware, vuelve a comentarlo y re-sube |
| Error de compilación | Librería no encontrada | Ejecuta `pio pkg install` para descargar las dependencias |
| Puerto USB no detectado | Driver ausente | Instala el driver CH340 o CP210x para tu sistema operativo |
| El dispositivo reinicia solo | Watchdog timeout | Consulta el monitor serie para ver el motivo del reset (`logResetReason`) |

---

## 🔗 Proyectos relacionados

- **[BlauClick](https://github.com/CasamaMaker/BlauClick)** — Botón inalámbrico con batería (sender del ecosistema)

---

## 📜 Licencia

Este proyecto es de código abierto. Consulta [LICENSE](LICENSE) para más detalles.

---

<div align="center">

Hecho con ❤️ por [CasamaMaker](https://github.com/CasamaMaker)

</div>
