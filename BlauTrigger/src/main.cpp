#include <Arduino.h>

#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>

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

// Estado actual del LED
bool ledState = false;

// Flag para habilitar o deshabilitar cifrado ESP-NOW
#define ENABLE_ENCRYPTION 0  // 1 = cifrado habilitado, 0 = cifrado deshabilitado

// Define la clave de cifrado (debe coincidir con el emisor)
#define CRYPTO_KEY "PASSWORD1"  // La misma clave que en el dispositivo emisor

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

// bool isAnyLedOn() {
//   for (int i = 0; i < NUM_LEDS; i++) {
//     if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0) {
//       return true;  // Al menos un LED está encendido
//     }
//   }
//   return false;  // Todos los LEDs están apagados
// }

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
    // if (strcmp(incomingMessage.payload, "ON") == 0) {
    //   // Encender la luz
    updateLEDState(!ledState);
    mqttClient.publish(HA_TOPIC_STATE, ledState ? "ON" : "OFF", true);
    //   // Publicar el nuevo estado en MQTT para mantener sincronizado Home Assistant
    //   mqttClient.publish(HA_TOPIC_STATE, "ON", true);
    // } else if (strcmp(incomingMessage.payload, "OFF") == 0) {
    //   // Apagar la luz
    //   updateLEDState(false);
    //   // Publicar el nuevo estado en MQTT para mantener sincronizado Home Assistant
    //   mqttClient.publish(HA_TOPIC_STATE, "OFF", true);
    // }
  }
}

void setup() {
  // Inicializar el puerto serie para depuración
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Esperar a que el puerto serie se conecte (con timeout)
  }
  
  Serial.println("Iniciando ESP32-C3 con ESP-NOW y MQTT para Home Assistant...");
  
  // Configurar el pin del LED como salida
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Inicializar FastLED
  FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);  // 0-255
  leds[0] = CRGB::Black;
  FastLED.show();
  
  // Primero inicializamos ESP-NOW para recibir comandos mientras nos conectamos a WiFi
  WiFi.mode(WIFI_STA);
  
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
  Serial.print("Dirección MAC del receptor: ");
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
}

void loop() {
  // Mantener la conexión MQTT activa
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();
  
  // Comprobar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conexión WiFi perdida. Reconectando...");
    connectToWiFi();
  }
  
  delay(10);
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
  
  // Crear configuración en formato JSON para que Home Assistant descubra automáticamente este dispositivo
  String config = "{";
  config += "\"name\":\"ESP32 LED\",";
  config += "\"unique_id\":\"esp32_c3_led\""+ WiFi.macAddress()+",";
  config += "\"state_topic\":\"" + String(HA_TOPIC_STATE) + "\",";
  config += "\"command_topic\":\"" + String(HA_TOPIC_COMMAND) + "\",";
  config += "\"payload_on\":\"ON\",";
  config += "\"payload_off\":\"OFF\",";
  config += "\"retain\":true,";
  config += "\"device\":{";
  config += "\"identifiers\":[\"esp32c3_" + WiFi.macAddress() + "\"],";
  config += "\"name\":\"ESP32-C3 con ESP-NOW\",";
  config += "\"manufacturer\":\"ESP\",";
  config += "\"model\":\"ESP32-C3\"";
  config += "}";
  config += "}";
  
  // Publicar la configuración
  Serial.println("Publicando configuración para Home Assistant...");
  mqttClient.publish(HA_TOPIC_CONFIG, config.c_str(), true);
}

// Actualizar el estado del LED (tanto LED_BUILTIN como FastLED)
void updateLEDState(bool state) {
  ledState = state;
  
  // Actualizar LED integrado
  digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
  
  // Actualizar LED FastLED
  if (state) {
    leds[0] = CRGB::Red;  // Puedes cambiar el color según tus preferencias
  } else {
    leds[0] = CRGB::Black;
  }
  FastLED.show();
  
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