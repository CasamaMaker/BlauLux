#include <Arduino.h>

#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <esp_wifi.h>  // Necesario para modificar la MAC del AP

// Configuración de WiFi
#define WIFI_SSID "SALICRU-BYOD"           // Cambiar por tu SSID
#define WIFI_PASSWORD "aFg6aGrFbJ9Pc5d9at"   // Cambiar por tu contraseña WiFi

// Configuración de MQTT
#define MQTT_SERVER "cangoita.duckdns.org"  // Cambiar por tu servidor MQTT
#define MQTT_PORT 444                  // Puerto MQTT (1883 es el estándar)
#define MQTT_USER "marti"      // Cambiar por tu usuario MQTT
#define MQTT_PASSWORD "Marcas1935"  // Cambiar por tu contraseña MQTT
#define MQTT_CLIENT_ID "ESP32_C3_LED"   // ID único para este cliente

// Tópicos MQTT para Home Assistant
#define HA_TOPIC_STATE "homeassistant/light/esp32_led/state"          // Estado actual del LED
#define HA_TOPIC_COMMAND "homeassistant/light/esp32_led/set"          // Para recibir comandos
#define HA_TOPIC_CONFIG "homeassistant/light/esp32_led/config"        // Para auto-discovery

// Configuración de LED
#define NUM_LEDS 1
#define DATA_PIN 6
CRGB leds[NUM_LEDS];

// Configuración del botón
#define BUTTON_PIN 5
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int buttonState = HIGH;  // Estado inicial es HIGH por el pull-up
int lastButtonState = HIGH;

// Configuración para modo de identificación
bool idModeActive = false;
unsigned long idModeStartTime = 0;
const unsigned long ID_MODE_DURATION = 60000; // 1 minuto en milisegundos

// Estado actual del LED
bool ledState = false;
CRGB previousLedColor = CRGB::Black;

// Flag para habilitar o deshabilitar cifrado ESP-NOW
#define ENABLE_ENCRYPTION 1  // 1 = cifrado habilitado, 0 = cifrado deshabilitado

// Define la clave de cifrado (debe coincidir con el emisor)
#define CRYPTO_KEY "PASSWORD12345678"  // La misma clave que en el dispositivo emisor

// MAC Address para almacenar la original
uint8_t originalMacAddress[6];

// Estructura del mensaje (debe coincidir con el emisor)
typedef struct {
  char topic[50];
  char payload[50];
} struct_message;

// Variable para almacenar el mensaje recibido
struct_message incomingMessage;

// Clientes WiFi y MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Prototipo de funciones
void connectToWiFi();
void connectToMQTT();
void publishHomeAssistantConfig();
void updateLEDState(bool state);
void handleMQTTMessage(char* topic, byte* payload, unsigned int length);
void handleButton();
void enterIdentificationMode();
void exitIdentificationMode();
void checkIdModeTimeout();

// Callback ESP-NOW actualizado compatible con ESP-IDF 5.x
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  // Copiar directamente los datos recibidos a la estructura
  memcpy(&incomingMessage, incomingData, sizeof(struct_message));
  
  // Imprimir dirección MAC del remitente
  char macStr[18];
  const uint8_t *mac = recv_info->src_addr;
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  // Imprimir los datos recibidos para depuración
  Serial.println("Datos recibidos de: " + String(macStr));
  Serial.print("Topic: ");
  Serial.println(incomingMessage.topic);
  Serial.print("Payload: ");
  Serial.println(incomingMessage.payload);
  
  // Procesar el comando recibido por ESP-NOW
  if (strcmp(incomingMessage.topic, "luz") == 0) {
    updateLEDState(!ledState);
    mqttClient.publish(HA_TOPIC_STATE, ledState ? "ON" : "OFF", true);
  }
}

void setup() {
  // Inicializar el puerto serie para depuración
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Esperar a que el puerto serie se conecte (con timeout)
  }
  
  Serial.println("Iniciando ESP32-C3 con ESP-NOW y MQTT para Home Assistant...");
  
  // Configurar els pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  
  // Inicializar FastLED
  FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(25);  // 0-255
  leds[0] = CRGB::Black;
  FastLED.show();
  
  // Primero inicializamos ESP-NOW para recibir comandos mientras nos conectamos a WiFi
  WiFi.mode(WIFI_STA);
  
  // Guardar la MAC original
  String macString = WiFi.macAddress();
  Serial.print("MAC original: ");
  Serial.println(macString);
  
  // Convertir la MAC String a un array de bytes
  sscanf(macString.c_str(), "%x:%x:%x:%x:%x:%x", 
         &originalMacAddress[0], &originalMacAddress[1], &originalMacAddress[2], 
         &originalMacAddress[3], &originalMacAddress[4], &originalMacAddress[5]);
;
  
  // Inicializar ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }
  
  #if ENABLE_ENCRYPTION
    // Configurar la misma clave PMK que en el emisor
    esp_err_t result = esp_now_set_pmk((uint8_t *)CRYPTO_KEY);
    if (result == ESP_OK) {
      Serial.println("Cifrado PMK habilitado correctamente");
    } else {
      Serial.println("Error al configurar cifrado PMK");
    }
  #else
    Serial.println("Cifrado PMK deshabilitado");
  #endif
  
  // Registrar la función callback para recibir datos
  esp_now_register_recv_cb(OnDataRecv);
  
  // Mostrar la dirección MAC del receptor
  Serial.print("Dirección MAC del receptor ESP-NOW: ");
  Serial.println(WiFi.macAddress());
  
  // Configurar el cliente MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(handleMQTTMessage);
  mqttClient.setBufferSize(512);  // Tamaño del buffer para mensajes grandes (como config)
  
  // Conectar a WiFi
  connectToWiFi();
  
  // Conectar a MQTT
  connectToMQTT();
  
  // Publicar configuración de Home Assistant (auto-discovery)
  publishHomeAssistantConfig();
  
  // Publicar el estado inicial
  updateLEDState(false);
  mqttClient.publish(HA_TOPIC_STATE, "OFF", true);
  
  Serial.println("Dispositivo listo para recibir comandos por ESP-NOW y MQTT");
  Serial.println("Presiona el botón en pin 5 para mostrar la MAC durante 1 minuto");
}

void loop() {
  // Monitorear el botón
  handleButton();
  
  // Comprobar si el modo de identificación debe finalizar
  if (idModeActive) {
    checkIdModeTimeout();
  } else {
    // Modo normal - mantener la conexión MQTT activa
    if (!mqttClient.connected()) {
      connectToMQTT();
    }
    mqttClient.loop();
    
    // Comprobar conexión WiFi
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Conexión WiFi perdida. Reconectando...");
      connectToWiFi();
    }
  }
  
  delay(10);
}

void handleButton() {
  // Leer el estado actual del botón
  int reading = digitalRead(BUTTON_PIN);
  
  // Comprobar si ha cambiado el estado del botón
  if (reading != lastButtonState) {
    // Reiniciar el temporizador de debounce
    lastDebounceTime = millis();
  }
  
  // Si ha pasado suficiente tiempo desde el último cambio...
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Si el estado ha cambiado realmente...
    if (reading != buttonState) {
      buttonState = reading;
      
      // Si el botón ha sido presionado (HIGH según corrección)
      if (buttonState == HIGH) {
        Serial.println("Botón presionado");
        
        // Alternar el modo de identificación
        if (idModeActive) {
          exitIdentificationMode();
        } else {
          enterIdentificationMode();
        }
      }
    }
  }
  
  // Guardar el último estado del botón
  lastButtonState = reading;
}

void enterIdentificationMode() {
  if (idModeActive) return;  // Ya está en modo identificación
  
  Serial.println("Entrando en modo de identificación...");
  idModeActive = true;
  idModeStartTime = millis();
  
  // Guardar el color actual del LED para restaurarlo después
  previousLedColor = leds[0];
  
  // Desconectar WiFi si está conectado
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Desconectando WiFi para cambiar a modo AP...");
    WiFi.disconnect();
    delay(100);
  }
  
  // Cambiar a modo AP para mostrar la MAC
  WiFi.mode(WIFI_AP);
  
  // Asegurar que la MAC del AP sea la misma que la de ESP-NOW (STA)
  esp_wifi_set_mac(WIFI_IF_AP, originalMacAddress);
  
  // Obtener la MAC que se usará en AP
  uint8_t ap_mac[6];
  esp_wifi_get_mac(WIFI_IF_AP, ap_mac);
  
  // Convertir MAC a string legible
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           ap_mac[0], ap_mac[1], ap_mac[2], ap_mac[3], ap_mac[4], ap_mac[5]);
           
  // Crear un nombre de red que incluya la MAC
  String ap_ssid = "ESP32-";
  for (int i = 0; i < 6; i++) {
    if (ap_mac[i] < 0x10) ap_ssid += "0";
    ap_ssid += String(ap_mac[i], HEX);
  }
  
  // Iniciar AP sin contraseña
  if (WiFi.softAP(ap_ssid.c_str())) {
    Serial.println("Punto de acceso creado correctamente");
    Serial.print("SSID: ");
    Serial.println(ap_ssid);
    Serial.print("Dirección MAC del AP: ");
    Serial.println(macStr);
    Serial.println("Esta MAC es la que deben usar otros dispositivos para ESP-NOW");
    
    // Encender el LED en rojo para indicar modo de identificación
    leds[0] = CRGB::Red;
    FastLED.show();
  } else {
    Serial.println("Error al crear punto de acceso");
    exitIdentificationMode();  // Salir del modo si hay error
  }
}

void exitIdentificationMode() {
  if (!idModeActive) return;  // No está en modo identificación
  
  Serial.println("Saliendo del modo de identificación...");
  idModeActive = false;
  
  // Detener el AP
  WiFi.softAPdisconnect(true);
  delay(100);
  
  // Volver a modo estación para WiFi y ESP-NOW
  WiFi.mode(WIFI_STA);
  
  // Restaurar la MAC original en modo STA si fuera necesario
  esp_wifi_set_mac(WIFI_IF_STA, originalMacAddress);
  
  // Restaurar el color anterior del LED
  leds[0] = previousLedColor;
  FastLED.show();
  
  // Reconectar a WiFi
  connectToWiFi();
  
  // Reiniciar ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error reinicializando ESP-NOW");
  } else {
    // Registrar nuevamente el callback
    esp_now_register_recv_cb(OnDataRecv);
    
    #if ENABLE_ENCRYPTION
      esp_err_t result = esp_now_set_pmk((uint8_t *)CRYPTO_KEY);
      if (result == ESP_OK) {
        Serial.println("Cifrado PMK rehabilitado correctamente");
      }
    #endif
  }
  
  // Reconectar a MQTT
  connectToMQTT();
}

void checkIdModeTimeout() {
  // Comprobar si ha transcurrido el tiempo asignado para el modo de identificación
  if (millis() - idModeStartTime > ID_MODE_DURATION) {
    Serial.println("Tiempo de modo de identificación expirado");
    exitIdentificationMode();
  }
}

// Conectar a la red WiFi
void connectToWiFi() {
  Serial.print("Conectando a WiFi ");
  Serial.print(WIFI_SSID);
  Serial.print("...");
  
  // Conservar modo WiFi STA para ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Esperar hasta 30 segundos para la conexión
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 30000) {
    Serial.print(".");
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado a WiFi!");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFallo al conectar a WiFi!");
  }
}

// Conectar al servidor MQTT
void connectToMQTT() {
  Serial.println("Conectando a MQTT...");
  
  // Intentar conexión hasta 5 veces
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    Serial.print("Intento #");
    Serial.print(attempts + 1);
    Serial.print("... ");
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Conectado a MQTT!");
      
      // Suscribirse al topic de comandos
      mqttClient.subscribe(HA_TOPIC_COMMAND);
      
      // Publicar estado actual
      mqttClient.publish(HA_TOPIC_STATE, ledState ? "ON" : "OFF", true);
      
    } else {
      Serial.print("Falló, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" intentando de nuevo en 2 segundos");
      delay(2000);
    }
    
    attempts++;
  }
}

// Publicar configuración para auto-discovery en Home Assistant
void publishHomeAssistantConfig() {
  if (!mqttClient.connected()) return;
  
  // Get MAC address once and store it
  String mac = WiFi.macAddress();
  // Crear configuración en formato JSON para que Home Assistant descubra automáticamente este dispositivo
  String config = "{";
  config += "\"name\":\"ESP32 LED\",";
  config += "\"unique_id\":\"esp32_c3_led_";
  config += mac;
  config += "\",";
  config += "\"state_topic\":\"";
  config += HA_TOPIC_STATE;
  config += "\",";
  config += "\"command_topic\":\"";
  config += HA_TOPIC_COMMAND;
  config += "\",";
  config += "\"payload_on\":\"ON\",";
  config += "\"payload_off\":\"OFF\",";
  config += "\"retain\":true";
  config += "}";
  
  mqttClient.setBufferSize(1024);  // Increase from 512 to 1024
  // Publicar la configuración
  Serial.println("Publicando configuración para Home Assistant...");
  mqttClient.publish(HA_TOPIC_CONFIG, config.c_str(), true);
}

// Actualizar el estado del LED (tanto LED_BUILTIN como FastLED)
void updateLEDState(bool state) {
  ledState = state;
  
  // No actualizar si estamos en modo de identificación
  if (idModeActive) return;
  
  // Actualizar LED integrado
  digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
  
  // Actualizar LED FastLED
  if (state) {
    leds[0] = CRGB::Red;  // Puedes cambiar el color según tus preferencias
  } else {
    leds[0] = CRGB::Black;
  }
  FastLED.show();
  
  // Guardar el color actual para recordarlo cuando salga del modo de identificación
  previousLedColor = leds[0];
  
  Serial.println(state ? "LED encendido" : "LED apagado");
}

// Callback para recibir mensajes MQTT
void handleMQTTMessage(char* topic, byte* payload, unsigned int length) {
  // Convertir el payload a un string terminado en null
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.print("Mensaje MQTT recibido en topic: ");
  Serial.println(topic);
  Serial.print("Mensaje: ");
  Serial.println(message);
  
  // Procesar comando del topic
  if (String(topic) == HA_TOPIC_COMMAND) {
    if (strcmp(message, "ON") == 0) {
      updateLEDState(true);
      mqttClient.publish(HA_TOPIC_STATE, "ON", true);
    } else if (strcmp(message, "OFF") == 0) {
      updateLEDState(false);
      mqttClient.publish(HA_TOPIC_STATE, "OFF", true);
    }
  }
}