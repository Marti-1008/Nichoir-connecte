#include "M5TimerCAM.h"
#include <WiFi.h>
// ... (autres includes non modifiés) ...
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include "Base64.h"  // bibliothèque ArduinoBase64
#include <NetworkClient.h>

#define batterie_ADC 33
#define LED_PIN     4
#define CAPTEUR_PIN 13
#define MAGIC_VALUE1 0x42
#define MAGIC_VALUE2 0x36
#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64


WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
IPAddress mqttServer(192,168,2,45);
Preferences prefs_param;
Preferences prefs;




// global data
uint8_t b2;
uint8_t b1;
uint8_t mqtt_set;
uint8_t missed_connexion;
bool wifi_can_connect;
bool first_time;
esp_sleep_wakeup_cause_t cause;

//=======================================================================================================================================================
// définis paramètre de l'image 
void F_contrast(uint8_t *value)
{
  TimerCAM.Camera.sensor->set_contrast(TimerCAM.Camera.sensor,*value);
}
 
void F_saturation (uint8_t *value)
{
  TimerCAM.Camera.sensor->set_saturation(TimerCAM.Camera.sensor,*value);
}
 
void F_brightness(uint8_t *value)
{
  TimerCAM.Camera.sensor->set_brightness(TimerCAM.Camera.sensor,*value);
}
 
void F_mirror(uint8_t *value)
{
  TimerCAM.Camera.sensor->set_hmirror(TimerCAM.Camera.sensor,*value);
}
 
void F_flip(uint8_t *value)
{
  TimerCAM.Camera.sensor->set_vflip(TimerCAM.Camera.sensor,*value);
}

framesize_t frameSizes[] = {
  FRAMESIZE_96X96,
  FRAMESIZE_QQVGA,
  FRAMESIZE_128X128,
  FRAMESIZE_QCIF,
  FRAMESIZE_HQVGA,
  FRAMESIZE_240X240,
  FRAMESIZE_QVGA,
  FRAMESIZE_320X320,
  FRAMESIZE_CIF,
  FRAMESIZE_HVGA,
  FRAMESIZE_VGA,
  FRAMESIZE_SVGA,
  FRAMESIZE_XGA,
  FRAMESIZE_HD,
  FRAMESIZE_SXGA,
  FRAMESIZE_UXGA
};
 
void F_image_format(uint8_t *index)
{
  if (*index < sizeof(frameSizes)/sizeof(frameSizes[0])) 
  {
     TimerCAM.Camera.sensor->set_framesize(TimerCAM.Camera.sensor, frameSizes[*index]);
  }
}
//=======================================================================================================================================================


 
  
//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
// Ca s'occupe du set des paramètres
void callback(char* topic, byte* payload, unsigned int length)
{
  prefs_param.begin("Parametre", false); 
  if (strcmp(topic, "B3/MartinOmar/parametre/camera/quality") == 0)
  {
    
    char qualite[length+1];
    memcpy(qualite, payload, length);
    qualite[length]='\0';

    prefs_param.putInt("set_qualite",atoi(qualite));
  }
  else if (strcmp(topic,"B3/MartinOmar/parametre/camera/contrast") == 0)
  {
    char contrast[length+1];
    memcpy(contrast, payload, length);
    contrast[length]='\0';

    prefs_param.putInt("set_contrast",atoi(contrast));
  }
  else if (strcmp(topic,"B3/MartinOmar/parametre/camera/saturation") == 0)
  {
    char saturation[length+1];
    memcpy(saturation, payload, length);
    saturation[length]='\0';

    prefs_param.putInt("set_saturation",atoi(saturation));
  }
  else if (strcmp(topic,"B3/MartinOmar/parametre/camera/brightness") == 0)
  {
    char brightness[length+1];
    memcpy(brightness, payload, length);
    brightness[length]='\0';

    prefs_param.putInt("set_brightness",atoi(brightness));
  }
  else if (strcmp(topic,"B3/MartinOmar/parametre/camera/mirror") == 0)
  {
    char mirror[length+1];
    memcpy(mirror, payload, length);
    mirror[length]='\0';

    prefs_param.putInt("set_mirror",atoi(mirror));
  }
  else if (strcmp(topic,"B3/MartinOmar/parametre/camera/flip") == 0)
  {
    char flip[length+1];
    memcpy(flip, payload, length);
    flip[length]='\0';

    prefs_param.putInt("set_flip",atoi(flip));
  }
  else if (strcmp(topic,"B3/MartinOmar/parametre/wifi/password") == 0)
  {
    char password[length+1];
    memcpy(password, payload, length);
    password[length]='\0';

    prefs_param.putString("set_password",password);
    
  }
  else if (strcmp(topic,"B3/MartinOmar/parametre/wifi/ssid") == 0)
  {
    
    char ssid[length+1];
    memcpy(ssid, payload, length);
    ssid[length]='\0';
    prefs_param.putInt("changed",1);
    prefs_param.putString("set_ssid",ssid);
  }

  prefs_param.end(); // termine la session
}
//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§










//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Fonction permettant de faire appaaître la page web quand pas connecté
void change_wifi()
{
  server.send(200, "text/html",
  R"rawliteral( <!DOCTYPE html> <html lang="fr">
  <head ><meta charset="UTF-8"> <body style="background-color:#1A1615">
  <h1 style="text-align: center; text-decoration: underline;color : white ;"> Assignation du nouveau wifi pour le nichoir connecté </h1>
  <h2 style="text-align: center;color : white ;"> By <strong> Martin & Omar</strong></h2>
  <form method="POST" action="/saveWifi" style="text-align: center; margin-top: 20px;">
   <div style="margin-bottom: 10px; color: white;">
      <label for="ssid">Nom du WiFi :</label>
      <input type="text" id="ssid" name="ssid">
    </div>
    <div style="margin-bottom: 10px; color: white text-align: center;">
      <label for="pw">Mot de passe :</label>
      <input type="password" id="pw" name="pw">
    </div>
    <div>
      <input type="submit" value="Enregistrer">
    </div>
  </form>
  </body> </head> </html>)rawliteral");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//########################################################################################################################################################
// récupère les datas de la page web
void handleSaveWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFi.disconnect(); // si on veut changer de wifi par set du mqtt
  }
  String ssidStr = server.arg("ssid"); // récupère les données du site web
  String pwdStr = server.arg("pw");

  Serial.println("Reçu SSID : " + ssidStr); 
  Serial.println("Reçu Password : " + pwdStr);

  prefs.begin("wifiPrefs", false); 

  if (first_time) // on regarde si on doit les changer
  {
    prefs.putUChar("magic0", MAGIC_VALUE1); // changement des valeurs des bits magiques et des paramètres wifi
    prefs.putUChar("magic1", MAGIC_VALUE2);
    prefs.putString("ssid", ssidStr); 
  }
  prefs.putString("pw", pwdStr);   
  prefs.end(); // termine la session

  server.send(200, "text/plain", "WiFi credentials saved. ESP32 is restarting...");
  prefs.begin("wifiPrefs", true); 

  prefs.begin("wifiPrefs", false);  // réinitialise les paramètres de déconnexion et du change wifi de mqtt
  prefs_param.begin("Parametre",false);
  prefs_param.putInt("changed",0);
  prefs.putInt("disconnect",0);
  prefs_param.end();
  prefs.end();
  // ESP.restart(); permet de rémarrer l'esp32
  esp_deep_sleep_start();
}
//########################################################################################################################################################





//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
// connect to wifi
void connectToWiFi(const char* ssid, const char* password) {
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) 
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) 
    {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        client.setServer(mqttServer, 1883);
    } 
    else 
    {
      Serial.println("Failed to connect to WiFi."); // ajout facteur need connexion
      uint8_t new_missed_connexion = missed_connexion+1;
      prefs.begin("wifiPrefs", false); 
      prefs.putInt("disconnect", new_missed_connexion);
      prefs.end();
      esp_deep_sleep_start();
    }
}
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@



//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// set des caractéristiques de la caméra
void Setcam()
{
  prefs_param.begin("Parametre",true);
  uint8_t value_contrast = prefs.getInt("set_contrast",0);
  uint8_t value_qualite = prefs.getInt("set_qualite",0);
  uint8_t value_saturation = prefs.getInt("set_saturation",0);
  uint8_t value_brightness = prefs.getInt("set_brightness",0);
  uint8_t value_mirror = prefs.getInt("set_mirror",0);
  uint8_t value_flip = prefs.getInt("set_flip",0);
  F_image_format(&value_qualite);
  F_mirror(&value_mirror);
  F_contrast(&value_contrast);
  F_saturation(&value_saturation);
  F_brightness(&value_brightness);
  F_flip(&value_flip);
}
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%






//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void sub()
{
  client.subscribe("B3/MartinOmar/parametre/camera/brightness");
  client.subscribe("B3/MartinOmar/parametre/camera/contrast");
  client.subscribe("B3/MartinOmar/parametre/camera/saturation");
  client.subscribe("B3/MartinOmar/parametre/camera/quality");
  client.subscribe("B3/MartinOmar/parametre/camera/mirror");
  client.subscribe("B3/MartinOmar/parametre/camera/flip");
  client.subscribe("B3/MartinOmar/parametre/wifi/ssid");
  client.subscribe("B3/MartinOmar/parametre/wifi/password");
  client.setCallback(callback);
}
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::






void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(CAPTEUR_PIN, INPUT);
  gpio_pulldown_en((gpio_num_t)CAPTEUR_PIN);
  gpio_pullup_dis((gpio_num_t)CAPTEUR_PIN);
  cause = esp_sleep_get_wakeup_cause();


  // EXT1 wakeup : se réveille si le capteur est HIGH
  esp_sleep_enable_ext1_wakeup((1ULL << CAPTEUR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_sleep_enable_timer_wakeup(24ULL * 60 * 60 * 1000000ULL); // 24h en microsecondes, Timer wakeup : se réveille toutes les 24h



  Serial.begin(115200);
  Serial.println("\nStarting setup...");

  prefs.begin("wifiPrefs", true); 
  prefs_param.begin("Parametre",true);
  b1 = prefs.getInt("magic0", 0); // le éro correspon à la valeur par défaut
  b2 = prefs.getInt("magic1", 0); // vérification premier passage
  mqtt_set =prefs_param.getInt("changed",0);
  missed_connexion = prefs.getInt("disconnect",0);
  prefs_param.end();

  first_time =  ((b1 == MAGIC_VALUE1) && (b2 == MAGIC_VALUE2));
  wifi_can_connect = first_time || mqtt_set != 1 || missed_connexion !=2; // si JAMAIS initialsé, si utilisateur ne change de wifi, si n'est pas déconnecté

  if (wifi_can_connect) // connexion au wifi
  {
    Serial.println("WiFi credentials found. Attempting to connect as STA.");
    // Lecture unique dans setup() pour initialiser la variable globale
    String globalSavedSSID = prefs.getString("ssid", ""); 
    String savedPW = prefs.getString("pw", "");
    prefs.end(); 
    
    WiFi.mode(WIFI_STA);
    connectToWiFi(globalSavedSSID.c_str(), savedPW.c_str()); // appelle fonction connexion to wifi
    
    sub(); // change the parametre


    if (cause == ESP_SLEEP_WAKEUP_EXT1 )
    {
      Setcam();
    }
  }


  else // demande à l'utilisateur de se connecter
  {
    Serial.println("No WiFi credentials found. Starting in AP mode to configure.");
    prefs.end(); 
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP("esp32_config_AP", "12345678"); 
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, change_wifi);
    server.on("/saveWifi", HTTP_POST, handleSaveWifi);
    server.begin();
    Serial.println("Web server started.");
  }
}

void loop()
{
  if (wifi_can_connect) 
  {
    server.handleClient();
  }
  if (cause == ESP_SLEEP_WAKEUP_EXT1 && WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(LED_PIN, HIGH); //
    // take a photoif (TimerCAM.Camera.get())
    if (TimerCAM.Camera.get())
    {
      digitalWrite(LED_PIN, LOW);// éteint la led si rien
      uint8_t* img = TimerCAM.Camera.fb->buf;
      size_t size = TimerCAM.Camera.fb->len;
 
          // Conversion en Base64
      String imgBase64 = base64::encode(img, size);
 
          // Envoi en chunks de 1 KB
      size_t maxChunk = 1024;
      size_t offset = 0;
 
      client.publish("B3/MartinOmar/image/start", "start"); // début
         
      int taille = imgBase64.length();
      int nbr_paquet = taille / maxChunk;
      String nbr_paquet_str = String(nbr_paquet);
  
 
      String chunk;
      while (offset < taille)
      {
           
        chunk = imgBase64.substring(offset, offset + maxChunk);
        client.publish("B3/MartinOmar/image/data", chunk.c_str(), false);
        offset += maxChunk;
        delay(10);
      }
      client.publish("B3/MartinOmar/image/end","end", false);
 
      //Serial.println("Image sent via Base64!");
 
      TimerCAM.Camera.free();
      delay(100); // attente avant prochaine capture
    }
    digitalWrite(LED_PIN, LOW);
    String tensionStr = String(TimerCAM.Power.getBatteryVoltage());
    String levelStr = String(TimerCAM.Power.getBatteryLevel());
    client.publish("B3/MartinOmar/parametre/battrie/tension", tensionStr.c_str());
    client.publish("B3/MartinOmar/parametre/battrie/level",levelStr.c_str());
 
  
 
  }
  else if (cause == ESP_SLEEP_WAKEUP_TIMER)
  {
    // envoie le niveau de la batterie
    String tensionStr = String(TimerCAM.Power.getBatteryVoltage());
    String levelStr = String(TimerCAM.Power.getBatteryLevel());
    client.publish("B3/MartinOmar/parametre/battrie/tension", tensionStr.c_str());
    client.publish("B3/MartinOmar/parametre/battrie/level",levelStr.c_str());
 
  }
  client.publish("esp32/MineurBenNanna/receive_data","true");
  int time= millis();
  while (millis()-time<10000)
  {
    client.loop();
    //  permet de recevoir les messages pendant un laps de temps de 10 sec
  }
  esp_deep_sleep_start();
  
}
