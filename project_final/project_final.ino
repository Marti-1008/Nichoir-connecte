// import of different libraries
#include "M5TimerCAM.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h> // back up data
#include <driver/gpio.h>
#include <Arduino.h>
#include <PubSubClient.h> // send data with mosquitto
#include <NetworkClient.h>






#define batterie_ADC 33 // GPIO of the battery
#define LED_PIN     4   // GPIO of the led
#define CAPTEUR_PIN 13  // GPIO of the sensor PIR
#define MAGIC_VALUE1 0x23
#define MAGIC_VALUE2 0x46
#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64


WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
IPAddress mqttServer(10,42,0,1); // address of the broker mqtt
Preferences prefs_param;
Preferences prefs;

//=======================================================================================================================================================
//Structure used to change the caracteristic of the picture 
struct CameraSettings {
    framesize_t frameSizes[4] = { // different picture formats
        FRAMESIZE_QQVGA,
        FRAMESIZE_VGA,
        FRAMESIZE_XGA,
        FRAMESIZE_UXGA
    };

    // Functions to change the caracteristic of the picture 
    void setContrast(uint8_t value) 
    {
      TimerCAM.Camera.sensor->set_contrast(TimerCAM.Camera.sensor, value);
      Serial.println("change contrast");
      Serial.println(value);
    }

   
    void setSaturation(uint8_t value) 
    {
      TimerCAM.Camera.sensor->set_saturation(TimerCAM.Camera.sensor, value);
      Serial.println("change sat");
      Serial.println(value);
    }

   
    void setBrightness(uint8_t value) 
    {
      TimerCAM.Camera.sensor->set_brightness(TimerCAM.Camera.sensor, value);
      Serial.println("lum");
      Serial.println(value);
    }
    
  
    void setFrameSize(uint8_t index) 
    {
      TimerCAM.Camera.sensor->set_framesize(TimerCAM.Camera.sensor, frameSizes[index]);
      Serial.println("taille");
      Serial.println(index);
    }
};
//=======================================================================================================================================================

// global data
uint8_t b2; //Bytes to know when the ESP32 starts for the first time
uint8_t b1;
bool first_time;// Data to launch the webserver of the ESP32 (data for the first launch)
// website to configure wifi  
uint8_t missed_connexion; //Data of the number of missed connections
bool wifi_can_connect; // Data to launch of the webserver of the ESP32
// website to configure wifi
bool changed_done = true; //  confirmation of sent data fromthe server 

CameraSettings camSettings;



 
//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
//reception of data from the broker mosquitto and saving the data in the ESP32 for the next pictures
//the first part in the topic to be has to be different from other projects.
//The second part is one (1 = identification number). This number is used to determine the camera (ESP32). Each camera has a different identification.
//The last part of the topic identifies the sended data.
void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.println("reception de messages");
  Serial.println(*payload);
  if (strcmp(topic,"B3/MartinOmar/1/parametre/camera/contrast") == 0)
  {
    
    char contrast[length+1];
    memcpy(contrast, payload, length);
    contrast[length]='\0';
    int contrast_value = atoi(contrast);
    if (-3<contrast_value && contrast_value<3)
    {
      prefs_param.putInt("set_contrast",contrast_value);
      Serial.println(contrast_value);
    }
  }
  else if (strcmp(topic,"B3/MartinOmar/1/parametre/camera/saturation") == 0)
  {
    
    char saturation[length+1];
    memcpy(saturation, payload, length);
    saturation[length]='\0';
    int saturation_value= atoi(saturation);
    if (-3<saturation_value && saturation_value<3)
    {
      prefs_param.putInt("set_saturation",saturation_value);
      Serial.println(saturation_value);
    }
  }
  else if (strcmp(topic,"B3/MartinOmar/1/parametre/camera/brightness") == 0)
  {
    
    char brightness[length+1];
    memcpy(brightness, payload, length);
    brightness[length]='\0';
    int brightness_value=atoi(brightness);
    if (-3<brightness_value && brightness_value<3)
    {
      prefs_param.putInt("set_brightness",brightness_value);
    }
  }
  else if (strcmp(topic,"B3/MartinOmar/1/parametre/camera/resolution") == 0)
  {

    char res[length+1];
    memcpy(res, payload, length);
    res[length]='\0';
    int res_index = atoi(res); 
    if (-1<res_index && res_index<4)
    {
      prefs_param.putInt("resolution",res_index);
    }
  } 
}
//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//This function is called when the web server starts. To go on this website you connect with this wifi "esp32_config_AP_1" and this password "C-EST-PARFAIT". When connected, you type 192.168.4.1 on your browser.
void change_wifi()
{
  server.send(200, "text/html",
    R"rawliteral(
    <!DOCTYPE html>
    <html lang="fr">
    <head charset="UTF-8">
    <style>
      body {
        background-color: #1A1615;
        color: white;
        font-family: Arial, sans-serif;
      }
      h1, h2 {
        text-align: center;
      }
      form {
        display: flex;
        flex-direction: column;
        align-items: center;
        margin-top: 20px;
      }
      .form-group {
        margin-bottom: 10px;
        text-align: center;
      }
      input[type="text"], input[type="password"] {
        width: 200px;
        padding: 5px;
      }
      input[type="submit"] {
        padding: 8px 20px;
        margin-top: 10px;
      }
    </style>
    </head>
    <body>
      <h1 style="text-decoration: underline;">Assignation du nouveau wifi pour le nichoir connecte</h1>
      <h2>By <strong>Martin & Omar</strong></h2>
      <div style="text-align:center;">Ici, formater la configuration du wifi</div>
      <div style="color: red; text-align:center;"><strong>Cette operation est definitive</strong></div>
      <div style="text-align:center;">Si vous vous êtes trompes, veuillez contacter le support IT à l'adresse ci-dessous</div>
      <div style="text-align:center;">Cette operation est delicate : pour cela nous vous conseillons de vous poser et de ne pas vous tromper dans la configuration du wifi</div>
      
      <h2 style="text-align: center;">Encodage du wifi</h2>

      <form method="POST" action="/saveWifi">
        <div class="form-group">
          <label for="ssid">Nom du WiFi :</label><br>
          <input type="text" id="ssid" name="ssid" required>
        </div>

        <div class="form-group">
          <label for="pw">Mot de passe :</label><br>
          <input type="password" id="pw" name="pw" required>
        </div>

        <div class="form-group">
          <!-- Hidden pour envoyer 0 si non coché -->
          <input type="hidden" name="noPassword" value="0">
          <label>
            <input type="checkbox" id="noPasswordCheckbox" name="noPassword" value="1" onclick="togglePassword()">
            Réseau sans mot de passe
          </label>
        </div>

        <div class="form-group">
          <input type="submit" value="Enregistrer">
        </div>
      </form>

      <p>
        Merci d'avoir choisi notre <strong>nichoir connecté</strong> !<br>
        Grace à vous, vos petits amis à plumes vont gazouiller en toute tranquillite, et nous, on peut continuer à faire 
        <em>ronronner nos serveurs</em> pour que tout fonctionne parfaitement.<br>
        Nous esperons que votre nichoir vous apportera autant de joie que les oiseaux apportent de chansons au matin.<br>
        Merci de faire partie de notre volée de passionnés !<br><br>
        Avec toute notre gratitude, <strong>Martin &amp; Omar</strong>
      </p>

      <p>
        Ce projet de nichoir connecte est un projet open source qui a pour but de partager differentes photos d'oiseaux à travers le monde.<br>
        Sur le site web de notre compagnie vous pourrez retrouver toutes les cameras utilisées à travers le monde. De plus contrairement à nos concurrents, notre systeme permet une configuration rapide et efficace des cameras à chaque instant.
      </p>

      <div style="text-align: right;">
        <address>
          Contact : <a href="mailto:martin.mineur@student.hepl.be">Martin</a> ou 
          <a href="mailto:Omar.benanna@student.hepl.be">Omar</a>
        </address>
      </div>

      <script>
        function togglePassword() {
          const pwInput = document.getElementById("pw");
          const checkbox = document.getElementById("noPasswordCheckbox");
          if (checkbox.checked) {
            pwInput.value = "";
            pwInput.disabled = true;
            pwInput.required = false;
          } else {
            pwInput.disabled = false;
            pwInput.required = true;
          }
        }
      </script>
    </body>
    </html>
    )rawliteral");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//########################################################################################################################################################
//It sent data from the webserver of the ESP32 are read. This data are written in the ESP32. When done, it changes the magic bytes if it is the first time. 
void handleSaveWifi()
{

  String ssidStr = server.arg("ssid"); //It received the data from the website
  String choice = server.arg("noPassword");
  if (choice =="0")
  {
    String pwdStr = server.arg("pw");
    Serial.println("Reçu Password : " + pwdStr);
     prefs.putString("pw", pwdStr);  //It writes the password in the memory of the ESP32 
  }
  
  

  Serial.println("Reçu SSID : " + ssidStr); 
  
  Serial.println("Reçu choix : " + choice);



  if (first_time) 
  {
    prefs.putInt("magic0", MAGIC_VALUE1); //it changes the magic bytes if it is the first time. 
    prefs.putInt("magic1", MAGIC_VALUE2);
    
  }
  //It writes the coice and the wifi in the memory of the ESP32 
  prefs.putString("ssid", ssidStr); 
  prefs.putString("choice", choice); 


  server.send(200, "text/html", R"rawliteral( <!DOCTYPE html>
  <html lang="fr">
  <body style="background-color:#1A1615">
  <head>
  <meta charset="UTF-8">
  <title style="text-align:center; color:white; font-family: Arial, sans-serif;">Configuration envoyée</title>
  </head>
  <body>
  <h1 style="text-align:center; color:white; font-family: Arial, sans-serif;"> Configuration envoyée</h1>
  <p style="text-align:left; color:white; font-family: Arial, sans-serif;">Les paramètres Wi-Fi ont été transmis.</p>
  <p style="text-align:left; color:white; font-family: Arial, sans-serif;">L'ESP32 va redémarrer et tenter de se connecter au Wi-Fi.</p>
  <p style="text-align:left; color:white; font-family: Arial, sans-serif;">Si la connexion échoue, le mode Point d’Accès sera relancé pour reconfigurer le Wi-Fi.<br> En cas de problème, contacter nos ingénieurs ingénieux à ces adresses mail : </p>
  <div style="text-align: right; color : 
  white ;"><address>Contact : <a href="martin.mineur@student.hepl.be">Martin</a> ou <a href="Omar.benanna@student.hepl.be">Omar</a></address></div> 
  </body>
  </body>
  </html>>)rawliteral"); //Confirm the right reception of the password and the name of the wifi
  
  
  prefs.putInt("disconnect",0);
  changed_done = false;
}
//########################################################################################################################################################





//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//to connexion of the ESP32 to the wifi
void connectToWiFi() {
    String globalSavedSSID = prefs.getString("ssid", "O"); //Get the password and the name of the wifi from the memory
    String savedPW = prefs.getString("pw", "0");
    String choice = prefs.getString("choice", "");
    if (choice=="0")
    {
      WiFi.begin(globalSavedSSID.c_str(), savedPW.c_str());
      Serial.println("Wifi sécurisé");
    }
    else
    {
      WiFi.begin(globalSavedSSID.c_str());
      Serial.println("Wifi non sécurisé");
    }

    Serial.printf("Connecting to ", globalSavedSSID.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) //It trys connecting to the wifi for 5 seconds 
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) //When the ESP32 is connected, It connects to the broquer mqtt on this port 1883
    {
      client.setServer(mqttServer, 1883);
      if (client.connect("ESP32CAM")) 
      {
        Serial.println("connected");
      }
      Serial.println("WiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      
    } 
    else //If the ESP32 can't connect to the wifi. It increments the data missed_connexion. If this data is 3, the webserver started to configure the new wifi. 
    {
      Serial.println("Failed to connect to WiFi."); 
      uint8_t new_missed_connexion = missed_connexion+1;       
      prefs.putInt("disconnect", new_missed_connexion);
      prefs_param.end();
      prefs.end();
      Serial.println("DORS");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      Serial.flush(); 
      esp_deep_sleep_start(); //It goes to deep sleep
    }
}
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@



//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//Settings of the camera 
void Setcam()
{
  uint8_t value_contrast = prefs_param.getInt("set_contrast",0); //Get the data from the memory
  uint8_t value_qualite = prefs_param.getInt("resolution",0);
  uint8_t value_saturation = prefs_param.getInt("set_saturation",0);
  uint8_t value_brightness = prefs_param.getInt("set_brightness",0);

  if (TimerCAM.Camera.sensor == NULL)  //Security of the camera if the camera does not start correctly
  {      
    Serial.println("Erreur: Capteur non initialisé !"); 
    prefs_param.end();
    prefs.end();
    Serial.println("DORS");
    Serial.flush(); 
    esp_deep_sleep_start(); //It goes to deep sleep      
  }
  camSettings.setContrast(value_contrast);

  camSettings.setSaturation(value_saturation);
  
  camSettings.setBrightness(value_brightness);
  
  camSettings.setFrameSize(value_qualite);  

}
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%






//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//subscribtion to some topics.
//This topics configure the camera or the wifi of the ESP32
void sub()
{
  if (client.connected())
  {
    client.subscribe("B3/MartinOmar/1/parametre/camera/resolution");
    client.subscribe("B3/MartinOmar/1/parametre/camera/brightness");
    client.subscribe("B3/MartinOmar/1/parametre/camera/contrast");
    client.subscribe("B3/MartinOmar/1/parametre/camera/saturation");
    client.setCallback(callback);
  }
  else
  {
    prefs_param.end();
    prefs.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("DORS");
    Serial.flush(); 
    esp_deep_sleep_start(); //It goes to deep sleep
  }
}
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//This function of the ESP32 is called when the ESP32 is not connected to the broker mqtt.
//try  of connection to the broker for 5 seconds.
//If not possible, it goes to the deep sleep.
void connect_TO_mqtt()
{

  uint8_t index = 0;
  while (!(client.connected() or index == 10))
  {
    Serial.println("Tentative de connecion");
    index +=1;
    delay(500);
  }
  if (index==10) 
  {
    
    prefs_param.end();
    prefs.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("SOMMEIL");
    Serial.flush(); 
    esp_deep_sleep_start();
  }
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
//It configures the ESP32 
void configure_ESP32()
{
  Serial.begin(115200);
  TimerCAM.begin();
  TimerCAM.Power.begin(); 
  TimerCAM.Camera.begin();
  TimerCAM.Power.setLed(128); // It turns on a LED, it confirms the functionning of the ESP32
  gpio_hold_en((gpio_num_t)POWER_HOLD_PIN);    
  gpio_deep_sleep_hold_en(); //It enables the ESP32 to use energy from a battery.
 
  pinMode(LED_PIN, OUTPUT);
  pinMode(CAPTEUR_PIN, INPUT);
  gpio_pulldown_en((gpio_num_t)CAPTEUR_PIN);// it is used so that the pin is not floating
  gpio_pullup_dis((gpio_num_t)CAPTEUR_PIN);
  
}
//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//Creation of the wifi of the ESP32
void Acces_point()
{
  esp_sleep_enable_ext1_wakeup((1ULL << CAPTEUR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH); //It can wake up with the sensor PIR
  Serial.println("No WiFi credentials found. Starting in AP mode to configure.");
  WiFi.mode(WIFI_MODE_AP); //The ESP32 becomes an acces point
  WiFi.softAP("esp32_config_AP_1", "C-EST-PARFAIT"); //The name of the wifi and its password
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  server.on("/", HTTP_GET, change_wifi); //It called the function to send data on the website
  server.on("/saveWifi", HTTP_POST, handleSaveWifi);//It called the function to receive data from the website
  server.begin();//It enables the server
  Serial.println("Web server started.");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~





//¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨
void battery()
{
  String tensionStr = String(TimerCAM.Power.getBatteryVoltage()); //Receiving and sending battery data
  String levelStr = String(TimerCAM.Power.getBatteryLevel());
  client.publish("B3/MartinOmar/1/parametre/battrie/tension", tensionStr.c_str());
  client.publish("B3/MartinOmar/1/parametre/battrie/level",levelStr.c_str());
}
//¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨



//...........................................................................................................................................................
void send_picture( uint8_t* img ,size_t size )
{
  int chunkSize=0;
  size_t offset = 0;
  Serial.print("Image size (bytes): ");
  Serial.println(size);

     
  size_t maxChunk = 1024; //send the image in several packages 
  
  client.publish("B3/MartinOmar/1/image/start", "start"); //beginning of the sending
  
        
  while (offset < size) //send the image in several packages 
  {
    Serial.println("envoie paquet");
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
  
  Serial.println("Image senvoyee");
  
  TimerCAM.Camera.free();
  TimerCAM.Camera.deinit();
}
//...........................................................................................................................................................




//__________________________________________________________________________________________________________________________________________________________
void setup()
{
  configure_ESP32();  //It called the function "configure esp32" to conigure the ESP32

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause(); //It informs the ESP32 of the reason for its wake up.
  Serial.println("\nStarting setup...");
  
  String lecture; // to go to deep sleep after have taken a picture

  
  prefs.begin("wifiPrefs", false);  //It allows the programs to write and read in the memory
  prefs_param.begin("Parametre",false); //It allows the programs to write and read in the memory

  //The next lines are used to know if it is the first time it works
  b1 = prefs.getInt("magic0", 0); 
  b2 = prefs.getInt("magic1", 0); 
  first_time =  ((b1 != MAGIC_VALUE1) && (b2 != MAGIC_VALUE2)); 
  if (first_time)
  {
    //It changes the magic bytes 
    prefs.putString("LECTURE", "HIGH"); //It allows the ESP32 to wake up if the sensor PIR detects motion
    prefs.putString("pw", "0");   
    prefs.putString("ssid"," 0"); 
    prefs_param.putInt("disconnect",0);
    missed_connexion = 0;
  }
  else 
  {
    missed_connexion = prefs.getInt("disconnect",0); //It knows how many times the ESP32 was unable to connect to the wifi
  }
  
  
  
  
  wifi_can_connect = (first_time ||  ( missed_connexion ==2)); //If the ESP32 must reconfigure the wifi. The value of "wifi_can_connect" is true. 
  Serial.println(first_time);
  if (wifi_can_connect) 
  {
    Acces_point();  //Function "Acces_point" enables the acces point and the server web
  }

  

  else  //The ESP32 can connect to the wifi
  {
   
    lecture = prefs.getString("LECTURE", "LOW");
    if (lecture=="HIGH")
    {
      // it is necessary because ESP32 can not detect rising EDGE. So the ESP32 wakes up a second time to go to deep sleep for minimum 5 minutes.
      prefs.putString("LECTURE", "LOW"); 
      esp_sleep_enable_timer_wakeup(60ULL*5 *1000000ULL); // 5 min
      Serial.println("passage en mode dormant");
      Serial.println("mode bas"); 
    }

    else
    {
      //This wake up is due to the second wake up. It allows the ESP32 to wake up if the sensor PIR detects motion
      prefs.putString("LECTURE", "HIGH"); 
      esp_sleep_enable_timer_wakeup(24ULL * 60 * 60 * 1000000ULL); // 24 hours in microsencode, ULL means :  Unsigned Long Long
      esp_sleep_enable_ext1_wakeup((1ULL << CAPTEUR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
      prefs_param.end();
      prefs.end();
      Serial.println("mode haut");
      Serial.flush(); 
      esp_deep_sleep_start();
    }


    Serial.println("WiFi credentials found. Attempting to connect as STA.");
    
     
    
    WiFi.mode(WIFI_STA);  //Connexion of the ESP32 to the wifi
    connectToWiFi();  //It called the function "connectToWiFi" to try to connect to the wifi
    connect_TO_mqtt();   //It called the function connect_to_mqtt to try to connect to the broker
    sub(); //It subscribe to some topics

    

    if (cause == ESP_SLEEP_WAKEUP_EXT1 ) //If the wake up is due to sensor PIR
    {
      String lecture =prefs.getString("LECTURE","0");
      Setcam(); //It configures the camera
      client.setBufferSize(4096); //It increases the size of the buffer
    }
  }
  TimerCAM.Power.setLed(0); //Turn off the LED of the ESP32
  
}
//__________________________________________________________________________________________________________________________________________________________





//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
void loop()
{
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause(); //It informs the ESP32 of the reason for its wake up.
    if (wifi_can_connect)  //If the ESP32 must reconfigure the wifi. The value of "wifi_can_connect" is true. 
    {
      
      while (changed_done)//It called the function until the user encodes datas
      {
        server.handleClient(); //continuously checks for incoming HTTP requests and calls the appropriate handler functions you defined with server.on().
      }    
    }


    Serial.println(cause == ESP_SLEEP_WAKEUP_EXT1 && WiFi.status() == WL_CONNECTED && client.connected());
    Serial.println(WiFi.status() == WL_CONNECTED );
    Serial.println(client.connected());


    if (cause == ESP_SLEEP_WAKEUP_EXT1 && WiFi.status() == WL_CONNECTED && client.connected()) //If the wake up is due to the sensorPIR and if the connexion between the broker and the ESP32 is functional, it is true
    {
      digitalWrite(LED_PIN, HIGH); //It turns on the infrared LED on the PCB
      if (TimerCAM.Camera.get()) //it takes pictures and returns true if it was able to take a picture
      {
        digitalWrite(LED_PIN, LOW); //It truns off the infrared LED on the PCB


        uint8_t* img = TimerCAM.Camera.fb->buf; //The place of the picture in the memory
        size_t size = TimerCAM.Camera.fb->len; //Length of the picture
        send_picture(img ,size );  //The function "send_picture" sends data to the broker mqtt
      }
      else 
      {
        digitalWrite(LED_PIN, LOW); //It truns off the infrared LED on the PCB
        client.disconnect();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        prefs_param.end();
        prefs.end();
        Serial.println("eteint");
        Serial.flush(); 
        esp_deep_sleep_start(); //go to the deep sleep
      }
      Serial.println("passage");
      battery();
  
    }
    else if (cause == ESP_SLEEP_WAKEUP_TIMER) //This wake up is due to the RTC alarm every 24 hours
    {
      battery();
    }
    else
    {
      client.disconnect();
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      prefs_param.end();
      prefs.end();
      Serial.println("eteint du à une perte de connexion");
      Serial.flush(); 
      esp_deep_sleep_start(); //go to the deep sleep
    }


    int time= millis();
    client.publish("B3/MartinOmar/1/parametre/camera/set","set"); //Reception of data from the broker during 5 seconds
    while (millis()-time<5000)
    {
      client.loop();
    }
  
  client.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  prefs_param.end();
  prefs.end();
  Serial.println("eteint");
  Serial.flush(); 
  esp_deep_sleep_start(); //go to the deep sleep

}
//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||