#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Arduino_JSON.h>  // Cambiamos a la librería correcta

const char* ssid = "ssid";
const char* password = "password";

// URL del archivo JSON que contiene la versión y la URL del firmware
const char* version_url = "http://192.168.0.11:3000/version.json";
String current_version = "1.0.1";  // La versión actual de tu firmware

void updateFirmware(const char* firmware_url) {
  Serial.println("Descargando firmware...");

  HTTPClient http;
  http.begin(firmware_url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      Serial.println("Comenzando la actualización...");

      WiFiClient *client = http.getStreamPtr();
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

void checkForUpdate() {
  Serial.println("Verificando actualizaciones...");

  HTTPClient http;
  http.begin(version_url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    // Parsear el JSON usando Arduino_JSON
    JSONVar json = JSON.parse(payload);

    if (JSON.typeof(json) == "undefined") {
      Serial.println("Error al parsear JSON");
      return;
    }

    const char* latest_version = json["version"];
    const char* firmware_url = json["url"];

    // Comparar versiones
    if (strcmp(latest_version, current_version.c_str()) > 0) {
      Serial.println("Nueva versión encontrada.");
      Serial.printf("Versión actual: %s, Versión más reciente: %s\n", current_version.c_str(), latest_version);

      // Descargar y actualizar el firmware
     updateFirmware(firmware_url);
    } else {
      Serial.println("El firmware está actualizado.");
    }
  } else {
    Serial.printf("Error al obtener el archivo de versión: código HTTP %d\n", httpCode);
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  // Espera a que se conecte a WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }

  Serial.println("Conectado a WiFi");
  
  // Verifica si hay una actualización disponible
  Serial.println("");
  Serial.print("Current version:");
  Serial.println(current_version);
  checkForUpdate();
}

void loop() {
  Serial.println("test");
}
