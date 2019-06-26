/**
 *  @filename   :   main.cpp
 *  @brief      :   Lora Mail Sensor Base Station Receiver and MQTT Transmitter
 *
 *  @author     :   Kevin Kessler
 *
 * Copyright (C) 2019 Kevin Kessler
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <SPIFFS.h>
#include "ArduinoJson.h"
#include "Ticker.h"
#include "ESP32_Servo.h"
#include "E32Lora.h"

char *ssid;
char *pass;
char *pskIdent;
char *pskKey;
char *mqtt_host;
uint16_t mqtt_port;

const char *heartbeat_topic = "/mailsensor";
const char *hostname = "maildisplay";

//E32 Hardware
uint8_t m0=12;
uint8_t m1=14;
uint8_t aux=13;

volatile bool sendHeartbeat = false;

Ticker heartBeat;
WiFiClientSecure net;
MQTTClient client;
Servo mailServo;
E32Lora e(Serial2);

uint8_t servoPin = 22;
uint16_t minUs = 500;
uint16_t maxUs = 2400;
uint8_t servoPos = 0;

void e32Setup() {
  e.begin(m0,m1,aux);
  e.setMode(CONFIG_MODE);
  e.setTransmissionMode(TxMode_Fixed);
  e.setAddress(0x00);
  e.setChannel(0xf);
  e.setTargetAddress(0x02);
  e.setTargetChannel(0x0f);
  e.saveParams();
  e.setMode(NORMAL_MODE); 
  e.reset();

}

void publishHeartbeat()
{
  sendHeartbeat = true;
}

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect("maildisplay")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nMQTT connected!");

}

void readConfig() {

  if(!SPIFFS.begin()){
    Serial.println("SPIFFS begin failed");
    return;
  }
 
  File config = SPIFFS.open("/secrets.json",FILE_READ);
  if(!config) {
    Serial.println("File Open Failed");
    return;
  }

  StaticJsonDocument<512> secrets;
  DeserializationError err = deserializeJson(secrets,config);
  if(err) {
    Serial.println("Error Deserializing Json");
    config.close();
    return;
  }


  if((const char *)secrets["ssid"] != nullptr) {
    ssid = (char *)malloc(strlen(secrets["ssid"]) + 1);
    strcpy(ssid,secrets["ssid"]);
  }else{
    Serial.println("ssid not found in secrets file");
  }

  if((const char *)secrets["password"] != nullptr) {;
    pass = (char *)malloc(strlen(secrets["password"]) + 1 );
    strcpy(pass,secrets["password"]);
  }else{
    Serial.println("password not found in secrets file");
  }

  if((const char *)secrets["pskIdent"]!= nullptr) {
    pskIdent = (char *)malloc(strlen(secrets["pskIdent"]) +1);
    strcpy(pskIdent,secrets["pskIdent"]);
  } else {
    Serial.println("pskIdent not found in secrets file");
  }
  
  // Key must be max 28 Hex digits
  if((const char *)secrets["pskKey"] != nullptr) {
    pskKey = (char *)malloc(strlen(secrets["pskKey"]) +1);
    strcpy(pskKey,secrets["pskKey"]);
  }else{
    Serial.println("pskKey not found in secrets file");
  }

  if((const char *)secrets["mqtt_host"] != nullptr) {
    mqtt_host = (char *)malloc(strlen(secrets["mqtt_host"])+1);
    strcpy(mqtt_host,secrets["mqtt_host"]);
  }else{
    Serial.println("mqtt_host not found in secrets file");
  }

  if(secrets["mqtt_port"] != 0) {
    mqtt_port = secrets["mqtt_port"];
  }else{
    Serial.println("mqtt_port not found in secrets file");
  }
  
  config.close();
}
void otaSetup() {

  ArduinoOTA.setHostname(hostname);
  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    SPIFFS.end();
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);

  readConfig();
  
  WiFi.begin(ssid, pass);
  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000); 
  }

  Serial.println(WiFi.localIP());
  if(!MDNS.begin(hostname)) {
    Serial.println("MDNS Failed");
  }
  
  otaSetup();

  net.setPreSharedKey(pskIdent, pskKey);
  client.begin(mqtt_host, mqtt_port, net);

  connect();

  e32Setup();

  heartBeat.attach(3,publishHeartbeat);

  mailServo.attach(servoPin, minUs, maxUs);
  mailServo.write(0);

}

void sendMQTTMessage(uint8_t mailFlag, uint8_t battery) {
    char output[100];
    sprintf(output,"{\"mailstate\":%d, \"battery\":%d}",mailFlag,battery);

    client.publish(heartbeat_topic,output); 
}

void loop() {
  if (sendHeartbeat) {
    if(!client.connected()){
      Serial.println("Reconnect");
      connect();
    }
    Serial.println("Sending Beat");
    sendMQTTMessage(2,127);

    sendHeartbeat = false;
  }

  if(e.dataAvailable()) {
    Serial.print("Data ");
    uint8_t data[3];
    e.receiveData(data, 3);
    for(int n=0;n<3;n++) {
      Serial.print(data[n],HEX);
      Serial.print(" ");
    }
    Serial.println("");
    
    bool mailFlag = 0x80 & data[0];
    uint8_t battery = 0x7f & data[0];

    if(mailFlag) {
      mailServo.write(90);
    } else {
      mailServo.write(0);
    }
    sendMQTTMessage(mailFlag,battery);
  }
  
  ArduinoOTA.handle();
}