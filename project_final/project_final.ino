#include <WiFi.h> 
#include <NetworkClient.h> 
#include <WebServer.h> 
#include <ESPmDNS.h> 
#include <EEPROM.h>
#include "driver/rtc_io.h"


#define batterie_ADC 33
#define LED_PIN     4
#define CAPTEUR_PIN 13
#define Adress_missed_connexion 2
#define ADDR_MAGIC 0
#define MAGIC_VALUE1 0x42
#define MAGIC_VALUE2 0x37
#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64
#define adresse_wifi 3
#define adresse_pw  (adresse_wifi + SSID_MAX_LEN)
#define adresse_mqtt (adresse_pw+PASS_MAX_LEN)




const char* ssid = "esp32_test"; 
const char* password = "12345678"; 


byte b1, b2;

IPAddress server(192,168,2,45);

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80); 

void change_wifi() 
{ 
  server.send(200, "text/html",
  R"rawliteral( <!DOCTYPE html> <html lang="fr"> 
  <head ><meta charset="UTF-8"> <body style="background-color:#1A1615"> 
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
  Merci d avoir choisi notre <strong>nichoir connecté</strong> ! <br><br> Grâce à vous, vos petits amis à plumes vont pouvoir gazouiller en toute tranquillité , et nous, on peut continuer à faire 
  <em>ronronner nos serveurs</em> pour que tout fonctionne parfaitement.<br><br> Nous espérons que votre nichoir vous apportera autant de joie que les oiseaux apportent de chansons au matin.<br> Merci de faire partie de notre volée de passionnés ! 🕊️<br>
  <br> Avec toute notre gratitude, <strong>Martin &amp; Omar</strong>  </p> 
  <div style="text-align: right; color : 
  white ;"><address>Contact : <a href="martin.mineur@student.hepl.be">Martin</a> ou <a href="Omar.benanna@student.hepl.be">Omar</a></address></div> 
  </body> </head> </html>)rawliteral"); 
}

void handleSaveWifi() 
{ 
  char ssid_received [SSID_MAX_LEN]; 
  char pass_received [PASS_MAX_LEN]; 
  String ssidStr = server.arg("ssid");
  ssidStr.toCharArray(ssid_received, SSID_MAX_LEN);
  String pwStr = server.arg("pw");
  pwdStr.toCharArray(ssid_received, SSID_MAX_LEN);
  Serial.println("Reçu SSID : " + ssidStr); 
  Serial.println("Reçu Password : " + pwStr); 
  server.sendHeader("Location", "/");
  server.send(303); // 303 = See Other
  EEPROM.put(adresse_wifi, ssid_received));
  EEPROM.put(adresse_pw, pass_received);
  EEPROM.commit();
} 

void setup() 
{ 
  pinMode(LED_PIN, OUTPUT);
  pinMode(CAPTEUR_PIN, INPUT);
  rtc_gpio_pullup_dis((gpio_num_t)CAPTEUR_PIN);
  // EXT1 wakeup : se réveille si le capteur est HIGH
  esp_sleep_enable_ext1_wakeup((1ULL << CAPTEUR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);

  // Timer wakeup : se réveille toutes les 24h
  esp_sleep_enable_timer_wakeup(24ULL * 60 * 60 * 1000000ULL); // 24h en microsecondes
  Serial.begin(115200); 
  bool condition1 = EEPROM.get(ADDR_MAGIC, b1) != MAGIC_VALUE1;
  bool condition2 = MAGIC_VALUE2 != EEPROM.get(ADDR_MAGIC+1, b2);
  int flag_wifi;
  EEPROM.get(Adress_missed_connexion, flag_wifi);
  bool flag_WIFI = (flag_wifi >= 2);

  
  int flag_mqtt;
  bool condition_mqtt = (EEPROM.get(adresse_mqtt,flag_mqtt)==1);
  bool condition_final =  (condition1 && condition2)  || flag_WIFI || condition_mqttt; // rajouter déconnexion et mqt


  WiFi.mode(condition_final ? WIFI_MODE_AP : WIFI_STA ); // en fonction de si c'est le premier passage ou non définit le mode
  if (condition_final)
  {
    
    server.on("/", HTTP_GET, change_wifi); 
    server.on("/saveWifi", HTTP_POST, handleSaveWifi);
    server.begin();//à  voir en fonction de flag
      bool ok = WiFi.softAP(ssid, password); 
    if(ok) 
    { 
      Serial.println("AP lancé avec succès !");
    } 
    else 
    { 
      Serial.println("Erreur lors du lancement de l'AP !"); 
    } 
    Serial.print("IP AP : "); 
    Serial.println(WiFi.softAPIP()); 
    
    if (flag_mqtt ==1)
    {
      EEPROM.put(adresse_mqtt, 0);
      EEPROM.commit();
    }
  }
  else 
  {
    char ssid [SSID_MAX_LEN];
    char password [PASS_MAX_LEN];
    EEPROM.get(adresse_wifi, ssid);
    EEPROM.get(adresse_pw, password);
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);
    Serial.println("");
    Serial.print("Connecting to ");
    Serial.println(ssid);
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    int valeur_missed_connexion ;
    EEPROM.get(Adress_missed_connexion,valeur_missed_connexion);
    if (valeur_missed_connexion !=0)
    {
      EEPROM.put(Adress_missed_connexion, 0);
      EEPROM.commit();
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
    if (cause == ESP_SLEEP_WAKEUP_EXT1)
    {
      TimerCAM.begin();
      TimerCAM.Camera.begin();
      client.setBufferSize(100000);
    }
  }
}





 void loop() 
{ 
  int flag_mqtt;
  bool deconnexion = EEPROM.get(adresse_mqtt, flag_mqtt)==1;
  if (WiFi.status() != WL_CONNECTED || deconnexion)
  {
  bool condition1 = EEPROM.get(ADDR_MAGIC, b1) != MAGIC_VALUE1;
  bool condition2 = MAGIC_VALUE2 != EEPROM.get(ADDR_MAGIC+1, b2);
  bool condition_final =  condition1 && condition2; // rajouter déconnexion et mqtt
    if (condution_final)//lors du premier passage utilisation du site web
    {
      EEPROM.put(ADDR_MAGIC,MAGIC_VALUE1);
      EEPROM.put(ADDR_MAGIC+1,MAGIC_VALUE2);
      EEPROM.commit();
      server.handleClient();
    }
    else 
    {
      int nombre;
      EEPROM.get(Adress_missed_connexion, nombre);
      if (nombre == 2)
      {
        server.handleClient(); // si il y a une déconnexion
      }
      else
      {
        EEPROM.put(Adress_missed_connexion, nombre+1);
        EEPROM.commit();
      }
    }
  }
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT1 && WiFi.status() == WL_CONNECTED) 
  {
    digitalWrite(LED_PIN, HIGH); //
    // take a photoif (TimerCAM.Camera.get()) 
    if (TimerCAM.Camera.get()) 
    {
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
          
      int taille = imgBase64.length()
      client.publish("esp32/MineurBenNanna/nbrpaquet",taille/maxChunk)
      while (offset < taille) 
      {
            
        String chunk = imgBase64.substring(offset, offset + maxChunk);
        client.publish("esp32/MineurBenNanna/image", chunk.c_str(), false);
        offset += maxChunk;
        delay(10);
      }
      Serial.println("Image sent via Base64!");

      TimerCAM.Camera.free();
      delay(60000); // attente avant prochaine capture
    } 
  }
  else if (cause == ESP_SLEEP_WAKEUP_TIMER)
  {
    // envoie le niveau de la batterie
    char tension = '0' + TimerCAM.Power.getBatteryVoltage();
    char level = '0'+ TimerCAM.Power.getBatteryLevel()
    client.publish("esp32/MineurBenNanna/batterie/level",level);
    client.publish("esp32/MineurBenNanna/batterie/tension", tension);

  }
  esp_deep_sleep_start();
}