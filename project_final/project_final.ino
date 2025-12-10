#include "M5TimerCAM.h"
#include <WiFi.h>
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
#define MAGIC_VALUE1 0x18
#define MAGIC_VALUE2 0x40
#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64


WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
IPAddress mqttServer(192,168,2,58);
Preferences prefs_param;
Preferences prefs;

//=======================================================================================================================================================

struct CameraSettings {
    framesize_t frameSizes[16] = {
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

   
    void setContrast(uint8_t value) 
    {
      TimerCAM.Camera.sensor->set_contrast(TimerCAM.Camera.sensor, value);
      Serial.println("change contrast");
    }

   
    void setSaturation(uint8_t value) 
    {
      TimerCAM.Camera.sensor->set_saturation(TimerCAM.Camera.sensor, value);
      Serial.println("change sat");
    }

   
    void setBrightness(uint8_t value) 
    {
      TimerCAM.Camera.sensor->set_brightness(TimerCAM.Camera.sensor, value);
      Serial.println("lum");
    }

    
    void setMirror(uint8_t value) 
    {
      TimerCAM.Camera.sensor->set_hmirror(TimerCAM.Camera.sensor, value);
      Serial.println("mirror");
    }

    
    void setFlip(uint8_t value) 
    {
      TimerCAM.Camera.sensor->set_vflip(TimerCAM.Camera.sensor, value);
      Serial.println("flip");
    }


    void setFrameSize(uint8_t index) 
    {
      TimerCAM.Camera.sensor->set_framesize(TimerCAM.Camera.sensor, frameSizes[index]);
      Serial.println("taille");
    }
};
//=======================================================================================================================================================

// global data
uint8_t b2;
uint8_t b1;
uint8_t missed_connexion;
bool wifi_can_connect;
bool first_time;
esp_sleep_wakeup_cause_t cause;
bool changed_done = true;
CameraSettings camSettings;



 
//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
// Ca s'occupe du set des paramètres
void callback(char* topic, byte* payload, unsigned int length)
{
  prefs_param.begin("Parametre", false); 
  if (strcmp(topic, "B3/MartinOmar/1/parametre/camera/quality") == 0)
  {
    
    char qualite[length+1];
    memcpy(qualite, payload, length);
    qualite[length]='\0';

    prefs_param.putInt("set_qualite",atoi(qualite));
  }
  else if (strcmp(topic,"B3/MartinOmar/1/parametre/camera/contrast") == 0)
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
  else if (strcmp(topic,"B3/MartinOmar/1/parametre/camera/mirror") == 0)
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
  <head charset="UTF-8"> <body style="background-color:#1A1615"> 
  <h1 style="text-align: center; text-decoration: underline;color : white ;"> Assignation du nouveau wifi pour le nichoir connecté </h1>
  <h2 style="text-align: center;color : white ;"> By <strong> Martin &; Omar</strong></h2> 
  <div style=" color : white ;text-align:center "> Ici tu peux formater la configuration du wifi,</div> 
  <div style="color: red;text-align: center;"> <strong> cette opération est définitive </strong></div> 
  <div style="text-align:center;color : white ;"> Si vous vous rendez compte que vous vous êtes trompés veillez contactez le support à l'adresse de contact notez ci-dessous</div> 
  <div style="text-align:center;color : white ;"> Cette opération est délicate : pour cela nous vous conseillons de vous poser et de ne pas vous tromper dans la configuration du wifi</div> 
  <h2 style="text-align: center;color: white;"> Encodage du wifi</h2> 
  <table > 
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

  </table> 
  <p style="text-align:left; color:white; font-family: Arial, sans-serif;"> 
  Merci d avoir choisi notre <strong>nichoir connecté</strong> ! <br><br> Grâce à vous, vos petits amis à plumes vont pouvoir gazouiller en toute tranquillité, et nous, on peut continuer à faire 
  <em>ronronner nos serveurs</em> pour que tout fonctionne parfaitement.<br><br> Nous espérons que votre nichoir vous apportera autant de joie que les oiseaux apportent de chansons au matin.<br> Merci de faire partie de notre volée de passionnés ! <br>
  <br> Avec toute notre gratitude, <strong>Martin &amp; Omar</strong>  </p> 
  <div style="text-align: right; color : 
  white ;"><address>Contact : <a href="martin.mineur@student.hepl.be">Martin</a> ou <a href="Omar.benanna@student.hepl.be">Omar</a></address></div> 
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
    prefs.putInt("magic0", MAGIC_VALUE1); // changement des valeurs des bits magiques et des paramètres wifi
    prefs.putInt("magic1", MAGIC_VALUE2);
    
  }
  prefs.putString("pw", pwdStr);   
  prefs.putString("ssid", ssidStr); 

  server.send(200, "text/plain", "WiFi credentials saved. ESP32 is restarting...");
  
  prefs_param.begin("Parametre",false);
  prefs_param.putInt("changed",0);
  prefs.putInt("disconnect",0);
  prefs_param.end();
  prefs.end();
  changed_done = false;
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
      Serial.println("DORS");
    }
}
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@



//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// set des caractéristiques de la caméra
void Setcam()
{
  prefs_param.begin("Parametre",true);
  uint8_t value_contrast = prefs_param.getInt("set_contrast",0);
  uint8_t value_qualite = prefs_param.getInt("set_qualite",0);
  uint8_t value_saturation = prefs_param.getInt("set_saturation",0);
  uint8_t value_brightness = prefs_param.getInt("set_brightness",0);
  uint8_t value_mirror = prefs_param.getInt("set_mirror",0);
  uint8_t value_flip = prefs_param.getInt("set_flip",0);
  camSettings.setContrast(value_contrast);
  camSettings.setSaturation(value_saturation);
  camSettings.setBrightness(value_brightness);
  camSettings.setMirror(value_mirror);
  camSettings.setFlip(value_flip);
  camSettings.setFrameSize(value_qualite);  // 320x320
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


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//Tentative de connexion au broker plus attente de connexion
void connect_TO_mqtt()
{
  uint8_t index = 0;
  while (!(client.connected() or index == 10))
  {
    Serial.println("Tentative de connecion");
    index +=1;
    delay(1000);
  }
  if (index==10)
  {
    Serial.println("SOMMEIL");
    esp_deep_sleep_start();
    
  }
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//__________________________________________________________________________________________________________________________________________________________
void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(CAPTEUR_PIN, INPUT);
  //gpio_pulldown_en((gpio_num_t)CAPTEUR_PIN);
  //gpio_pullup_dis((gpio_num_t)CAPTEUR_PIN);
  //cause = esp_sleep_get_wakeup_cause();
  cause == ESP_SLEEP_WAKEUP_EXT1; // pour manip sans le raspberry et les appareils

  // EXT1 wakeup : se réveille si le capteur est HIGH
  //esp_sleep_enable_ext1_wakeup((1ULL << CAPTEUR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
  //esp_sleep_enable_timer_wakeup(24ULL * 60 * 60 * 1000000ULL); // 24h en microsecondes, Timer wakeup : se réveille toutes les 24h
  esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL); // 1 min


  Serial.println("\nStarting setup...");

  prefs.begin("wifiPrefs", false); 
  prefs_param.begin("Parametre",false);
  b1 = prefs.getInt("magic0", 0); // le éro correspon à la valeur par défaut
  b2 = prefs.getInt("magic1", 0); // vérification premier passage
  first_time =  ((b1 != MAGIC_VALUE1) && (b2 != MAGIC_VALUE2)); // si différent alors premier passage
  if (first_time)
  {
    prefs.putString("pw", "0");   
    prefs.putString("ssid"," 0"); 
    prefs_param.putInt("disconnect",0);
    missed_connexion = 0;
  }
  else 
  {
    missed_connexion = prefs.getInt("disconnect",0);
  }
  prefs_param.end();

  
  wifi_can_connect = (first_time ||  ( missed_connexion ==2)); // si JAMAIS initialsé, si utilisateur ne change de wifi, si n'est pas déconnecté
  Serial.println(first_time);
  if (wifi_can_connect) // demande à l'utilisateur de se connecter
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

  else  // connexion au wifi
  {
    



    Serial.println("WiFi credentials found. Attempting to connect as STA.");
    // Lecture unique dans setup() pour initialiser la variable globale
    String globalSavedSSID = prefs.getString("ssid", ""); 
    String savedPW = prefs.getString("pw", "");
    prefs.end(); 
    
    WiFi.mode(WIFI_STA);
    connectToWiFi(globalSavedSSID.c_str(), savedPW.c_str()); // appelle fonction connexion to wifi
    connect_TO_mqtt();
    sub(); // change the parametre


    if (cause == ESP_SLEEP_WAKEUP_EXT1 )
    {
      Setcam();
    }
  
  }
}
//__________________________________________________________________________________________________________________________________________________________

//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
void loop()
{
  if (wifi_can_connect) 
  {
    
    while (changed_done)//appelle la fonction tant que pas de nom de wifi reçu
    {
      server.handleClient();
    }    
  }
  
  if (cause == ESP_SLEEP_WAKEUP_EXT1 && WiFi.status() == WL_CONNECTED && client.connected()) // je regarde si je suis connecté ici
  {
    digitalWrite(LED_PIN, HIGH); //
    // take a photoif (TimerCAM.Camera.get())
    if (TimerCAM.Camera.get())
    {
      uint8_t* img = TimerCAM.Camera.fb->buf;
      size_t size = TimerCAM.Camera.fb->len;
      int chunkSize=0;
      size_t offset = 0;
      Serial.print("Image size (bytes): ");
      Serial.println(size);

    // Envoi en chunks de 1 KB
      size_t maxChunk = 1024;
 
      client.publish("B3/MartinOmar/1/image/start", "start"); // début
 
      
      while (offset < size) 
      {
        if  (size < offset+maxChunk)
        {
          chunkSize = size-offset;
        }
        else 
        {
          chunkSize = 1024;
        }
        client.publish("B3/MartinOmar/1/image/data", img+offset ,  chunkSize , false);
        offset = offset+chunkSize;
        delay(10);
      }
      client.publish("B3/MartinOmar/1/image/end","end", false);
 
      //Serial.println("Image sent via Base64!");
 
      TimerCAM.Camera.free();
      TimerCAM.Camera.deinit();
      delay(100); // attente avant prochaine capture
    }
    digitalWrite(LED_PIN, LOW);
    String tensionStr = String(TimerCAM.Power.getBatteryVoltage());
    String levelStr = String(TimerCAM.Power.getBatteryLevel());
    client.publish("B3/MartinOmar/1/parametre/battrie/tension", tensionStr.c_str());
    client.publish("B3/MartinOmar/1/parametre/battrie/level",levelStr.c_str());
 
  
 
  }
  else if (cause == ESP_SLEEP_WAKEUP_TIMER)
  {
    // envoie le niveau de la batterie
    String tensionStr = String(TimerCAM.Power.getBatteryVoltage());
    String levelStr = String(TimerCAM.Power.getBatteryLevel());
    client.publish("B3/MartinOmar/1/parametre/battrie/tension", tensionStr.c_str());
    client.publish("B3/MartinOmar/1/parametre/battrie/level",levelStr.c_str());
 
  }
  client.publish("esp32/MineurBenNanna/1/receive_data","true");
  int time= millis();
 
  while (millis()-time<10000)
  {
     client.loop();
    //  permet de recevoir les messages pendant un laps de temps de 10 sec
  }
  Serial.println("eteint");
  esp_deep_sleep_start();

}
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||