#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Arduino_JSON.h>
#include <PubSubClient.h>


// *** CONFIGURACIÓN GENERAL *** //
boolean isDev = true;
String environment = "prod";

// *** CONFIGURACIÓN WIFI *** //
const char* WIFI_SSID = "ssid";
const char* WIFI_PASSWORD = "password";

// *** CONFIGURACIÓN MQTT *** //
const char MQTT_BROKER[] = "192.168.0.11";
int MQTT_PORT = 1883;
String deviceID;

// *** CONFIGURACIÓN MQTT *** //
String API_URL = "http://192.168.0.11:3003";

// Inicialización del cliente WiFi y MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// URL del archivo JSON que contiene la versión y la URL del firmware
String version_url = API_URL + "/version.json";
String current_version = "1.0.1";  // Versión actual del firmware

// *** FUNCIÓN: Actualización de firmware *** //
void updateFirmware(const char* firmware_version) {
  Serial.println("Descargando firmware...");

  String url_download_firmware = API_URL +"/firmware/"+ firmware_version;
  Serial.print("url download->");
  Serial.println(url_download_firmware);
  HTTPClient http;
  http.begin(url_download_firmware);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      Serial.println("Comenzando la actualización...");

      WiFiClient* client = http.getStreamPtr();
      size_t written = Update.writeStream(*client);

      if (written == contentLength) {
        Serial.println("Firmware escrito correctamente.");
      } else {
        Serial.printf("Error al escribir el firmware: escrito solo %d/%d bytes\n", written, contentLength);
      }

      if (Update.end()) {
        if (Update.isFinished()) {
          Serial.println("Actualización completada.");
          ESP.restart();
        } else {
          Serial.println("Error: la actualización no finalizó correctamente.");
        }
      } else {
        Serial.printf("Error al finalizar la actualización: %s\n", Update.errorString());
      }
    } else {
      Serial.println("Error: el tamaño del firmware es demasiado grande o el espacio flash no es suficiente.");
    }
  } else {
    Serial.printf("Error al descargar el firmware: código HTTP %d\n", httpCode);
  }

  http.end();
}

// *** FUNCIÓN: Verificar actualizaciones de firmware *** //
void checkForUpdate() {
  Serial.println("Verificando actualizaciones...");

  HTTPClient http;
  http.begin(version_url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JSONVar json = JSON.parse(payload);

    if (JSON.typeof(json) == "undefined") {
      Serial.println("Error al parsear JSON");
      return;
    }

    const char* latest_version = json["version"];
    const char* firmware_url = json["url"];

    // Comparar versiones y actualizar si es necesario
    if (strcmp(latest_version, current_version.c_str()) > 0) {
      Serial.printf("Nueva versión encontrada: %s. Actualizando...\n", latest_version);
      updateFirmware(firmware_url);
    } else {
      Serial.println("El firmware está actualizado.");
    }
  } else {
    Serial.printf("Error al obtener el archivo de versión: código HTTP %d\n", httpCode);
  }

  http.end();
}

// *** CALLBACK: Función cuando llega un mensaje MQTT *** //
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("Mensaje recibido [%s]: %s\n", topic, message.c_str());

  // Manejar la actualización de firmware cuando llegue el mensaje de actualización
  String expectedTopic = "devices/" + deviceID + "/firmware/update";
  if (String(topic) == expectedTopic) {
    // Se espera que el payload sea la URL del nuevo firmware
    updateFirmware(message.c_str());
  }

}

void mqttConnect() {
  while (!mqttClient.connected()) {
    String clientId = "ESP32Client-" + deviceID;
    Serial.println("Conectando a MQTT...");
    String willTopic = "devices/" + deviceID + "/status";
    const char* willMessage = "offline";
    bool willRetain = true;

    if (mqttClient.connect(clientId.c_str(), willTopic.c_str(), 0, willRetain, willMessage)) {
      Serial.println("Conexión exitosa al broker MQTT.");
      mqttClient.publish(willTopic.c_str(), "online", willRetain);  // Publicar estado online

      // Publicar la versión del firmware
      String versionTopic = "devices/" + deviceID + "/version";
      mqttClient.publish(versionTopic.c_str(), current_version.c_str(), true);  // Retain para que esté siempre disponible

      // Publicar el entorno
      String environmentTopic = "devices/" + deviceID + "/environment";
      mqttClient.publish(environmentTopic.c_str(), environment.c_str(), true);  // Retain para que esté siempre disponible

      // Suscribirse a actualizaciones de firmware
      String updateTopic = "devices/" + deviceID + "/firmware/update";
      mqttClient.subscribe(updateTopic.c_str());  // Suscribirse a tópico de actualización de firmware

      String topic = "devices/" + deviceID;
      mqttClient.subscribe(topic.c_str());  // Suscribirse a tópico del dispositivo
    } else {
      Serial.printf("Falló la conexión, rc=%d. Reintentando...\n", mqttClient.state());
      delay(5000);
    }
  }
}

void setupWiFi() {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Esperar hasta conectarse a WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nConexión WiFi exitosa.");
  Serial.printf("Dirección IP: %s\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);

  // Configurar WiFi y obtener dirección MAC como ID de dispositivo
  setupWiFi();
  deviceID = WiFi.macAddress();
  Serial.printf("Dirección MAC del dispositivo: %s\n", deviceID.c_str());

  // Configurar MQTT y conectarse al broker
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callback);
  mqttConnect();

  // Verificar actualizaciones de firmware si no estamos en modo de desarrollo
  if (!isDev) {
    checkForUpdate();
  }
}

void loop() {
  if (!mqttClient.connected()) {
    mqttConnect();
  }
  mqttClient.loop();
}
