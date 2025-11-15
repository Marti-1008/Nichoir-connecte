/**
 * @file sta.ino
 * @author SeanKwok (shaoxiang@m5stack.com)
 * @brief TimerCAM WEB CAM STA Mode
 * @version 0.1
 * @date 2024-01-02
 *
 *
 * @Hardwares: TimerCAM
 * @Platform Version: Arduino M5Stack Board Manager v2.0.9
 * @Dependent Library:
 * TimerCam-arduino: https://github.com/m5stack/TimerCam-arduino
 */
#include "M5TimerCAM.h"
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h> 
#include <PubSubClient.h> //Librairie pour la gestion Mqtt 
const char* ssid = "plc-511";
const char* password = "TOMHVEGGSZQZHAT";

if seriavl.available()
    {
      ssid=serial.readstring
      password=serial.readstring()
      Serial.print("Le nom du wifi est :")
      Serial;println(*ssid)
      Serial.print("Le code du wifi est :")
      Serial;println(*password)
    }

;



void setup() {
  TimerCAM.begin(); //initialise la caméra

  if (!TimerCAM.Camera.begin()) { // on regarde si la caméra est initialisée
      Serial.println("Camera Init Fail");
      return;
  }

}

void loop() {
    if (TimerCAM.Camera.get()) {
        Serial.printf("pic size: %d\n", TimerCAM.Camera.fb->len);
        TimerCAM.Camera.free();
        delay(10);
    }
}

