#include <PubSubClient.h>
#include <WiFi.h>

const char* ssid     = "electroProjectWifi";
const char* password = "B1MesureEnv";

IPAddress server(192,168,2,35); // IP du broker MQTT

WiFiClient espClient;           // client réseau WiFi
PubSubClient client(espClient); // client MQTT
void setup()
{
  
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);
    Serial.println("");
    Serial.print("Connecting to ");
    Serial.println(ssid);
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    client.setServer(server,1883);
  String esp32 = "ESP32Client-";
  esp32 += String(random(0xffff), HEX);
  client.connect(esp32.c_str());
    
}


void loop()
{
  Serial.println(WiFi.status());
  delay(1);
  client.publish("B33","hello world");
  
}
