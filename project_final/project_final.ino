#include <WiFi.h> 
#include <NetworkClient.h> 
#include <WebServer.h> 
#include <ESPmDNS.h> 
#include <EEPROM.h>


#define Adress_first_connexion 2
#define Adress_missed_connexion 3
#define ADDR_MAGIC 0
#define MAGIC_VALUE1 0x42
#define MAGIC_VALUE2 0x37
#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64
#define adresse_wifi 4
#define adresse_pw  (adresse_wifi + SSID_MAX_LEN)



const char* ssid = "esp32_test"; 
const char* password = "12345678"; 

String ssid_received; 
String pass_received; 
byte b1, b2;


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
  ssid_received = server.arg("ssid"); 
  pass_received = server.arg("pw"); 
  Serial.println("Reçu SSID : " + ssid_received); 
  Serial.println("Reçu Password : " + pass_received); 
  server.sendHeader("Location", "/");
  server.send(303); // 303 = See Other
  EEPROM.put(Address_first_connexion, 1);
  EEPROM.put(adresse_wifi, ssid_received));
  EEPROM.put(adresse_pw, pass_received);
  server.end();
} 

void setup() 
{ 
  Serial.begin(115200); 
  bool condition1 = EEPROM.get(ADDR_MAGIC, b1) != MAGIC_VALUE1;
  bool condition2 = MAGIC_VALUE2 != EEPROM.get(ADDR_MAGIC+1, b1);

  bool condition_final =  condition1 || condition2; // rajouter déconnexion et mqtt
  WiFi.mode(condition_final ? WIFI_MODE_AP : WIFI_STA ); // en fonction de si c'est le premier passage ou non définit le mode
  if (condition_final)
  {
    server.begin();//à  voir en fonction de flag
    server.on("/", HTTP_GET, change_wifi); 
    server.on("/saveWifi", HTTP_POST, handleSaveWifi);
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
  }
  else 
  {
    String ssid ;
    String password;
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
}


 void loop() 
{ 
  if (WiFi.status() == WL_CONNECTED)
  {
    if (EEPROM.get(ADDR_MAGIC, b1) != MAGIC_VALUE1 || MAGIC_VALUE2 != EEPROM.get(ADDR_MAGIC+1, b1))//lors du premier passage utilisation du site web
    {
      EEPROM.put(ADDR_MAGIC,MAGIC_VALUE1);
      EEPROM.put(ADDR_MAGIC+1,MAGIC_VALUE2);
      server.handleClient();
    }
    else 
    {
      int nombre;
      EEPROM.get(Adress_missed_connexion, nombre);
      if nombre == 2
      {
        server.handleClient();
        EEPROM.put(Adress_missed_connexion, 0);
      }
      else
      {
        EEPROM.put(Adress_missed_connexion, nombre+1);
      }
    }
  }
}