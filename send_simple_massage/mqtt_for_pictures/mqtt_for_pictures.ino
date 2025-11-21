#include "M5TimerCAM.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "Base64.h"  // bibliothèque ArduinoBase64

const char* ssid     = "electroProjectWifi";
const char* password = "B1MesureEnv";
IPAddress server(192,168,2,45);

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  TimerCAM.begin();
  TimerCAM.Camera.begin();

  // Connexion WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Connexion MQTT
  client.setServer(server, 1883);
  Serial.print("Connecting to MQTT...");
  while (!client.connected()) {
    if (client.connect("ESP32CAM")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 2s");
      delay(2000);
    }
  }
  client.setBufferSize(100000);
}

void loop() {
  

  if (TimerCAM.Camera.get()) {
    uint8_t* img = TimerCAM.Camera.fb->buf;
    size_t size = TimerCAM.Camera.fb->len;

    Serial.print("Image size (bytes): ");
    Serial.println(size);

    // Conversion en Base64
    String imgBase64 = base64::encode(img, size);

    // Envoi en chunks de 1 KB
    size_t maxChunk = 1024;
    size_t offset = 0;

    client.publish("esp32/MineurBenNanna/image", "start"); // début

    while (offset < imgBase64.length()) {
      
      String chunk = imgBase64.substring(offset, offset + maxChunk);
      client.publish("esp32/MineurBenNanna/image", chunk.c_str(), false);
      offset += maxChunk;
      delay(10);
    }

    client.publish("esp32/MineurBenNanna/image", "end"); // fin
    Serial.println("Image sent via Base64!");

    TimerCAM.Camera.free();
    delay(60000); // attente avant prochaine capture
  }
}
