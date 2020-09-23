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
#include <ArduinoOTA.h>
#include <WiFi.h>          
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <WiFiManager.h>
#include "Ticker.h"
#include "ESP32_Servo.h"
#include "SoftwareSerial.h"
#include "LoRa_E32.h"
#include "maildisplay.h"

const char *hostname = "maildisplay";
char mqttServer[MQTT_SERVER_LENGTH];
char mqttTopic[MQTT_TOPIC_LENGTH];
uint16_t mqttPort;
bool configMode = false;
uint8_t flash_toggle=0;
volatile bool buttonLongPress = false; 
volatile uint32_t lastPressTime;

Ticker heartBeat(publishHeartbeat, 300000);

struct mqttConfig {
  uint32_t valid;
  char server[MQTT_SERVER_LENGTH];
  uint16_t port;
  char topic[MQTT_TOPIC_LENGTH]; 
};

//E32 Hardware
uint8_t m0=12;
uint8_t m1=14;
uint8_t aux=13;

LoRa_E32 lora(16, 17, &Serial2, aux, m0, m1, UART_BPS_RATE_9600);

volatile bool sendHeartbeat = false;

Servo mailServo;

uint8_t servoPin = 22;
uint16_t minUs = 500;
uint16_t maxUs = 2400;
uint8_t servoPos = 0;

void configModeCallback(WiFiManager *wfm) {
  Serial.println(F("Config Mode"));
  Serial.println(WiFi.softAPIP());
  configMode = true;
  Serial.println(wfm->getConfigPortalSSID());
}

void printMQTT() {
  Serial.print(F("Server "));
  Serial.println(mqttServer);

  Serial.print(F("Port "));
  Serial.println(mqttPort,DEC);

  Serial.print(F("Topic "));
  Serial.println(mqttTopic);
}

void readEEPROM() {
  mqttConfig conf;
  EEPROM.get(0,conf);
  
  if (conf.valid ==0xDEADBEEF) {
    strncpy(mqttServer, conf.server, MQTT_SERVER_LENGTH);
    strncpy(mqttTopic, conf.topic, MQTT_TOPIC_LENGTH);
    mqttPort=conf.port;
  }
  else {
    WiFiManager wifiManager;

    strncpy(mqttServer,"",MQTT_SERVER_LENGTH);
    strncpy(mqttTopic,"",MQTT_TOPIC_LENGTH);
    mqttPort = 1883;

    Serial.println(F("Setup WIFI Manager"));
    printMQTT();
  
    callWFM(false);
  }
}

void writeEEPROM() {
  Serial.println(F("Writing MQTT Config"));

  mqttConfig conf;

  conf.valid = 0xDEADBEEF;
  strncpy(conf.server, mqttServer, MQTT_SERVER_LENGTH);
  strncpy(conf.topic, mqttTopic, MQTT_TOPIC_LENGTH);
  conf.port = mqttPort;

  EEPROM.put(0,conf);
  EEPROM.commit();
}

void callWFM(bool connect) {
  WiFiManager wfm;

  wfm.setAPCallback(configModeCallback);

  WiFiManagerParameter mqtt_server("server", "MQTT Server", mqttServer, MQTT_SERVER_LENGTH);
  char port_string[6];
  itoa(mqttPort, port_string,10);
  WiFiManagerParameter mqtt_port("port", "MQTT port", port_string, 6);
  WiFiManagerParameter mqtt_topic("topic", "MQTT Topic", mqttTopic,MQTT_TOPIC_LENGTH);

  wfm.addParameter(&mqtt_server);
  wfm.addParameter(&mqtt_port);
  wfm.addParameter(&mqtt_topic);

  if(connect) {
    if(!wfm.autoConnect()) {
      Serial.println(F("Failed to connect and hit timeout"));
      ESP.restart();
      delay(5000);
    }
  } else {
      if(!wfm.startConfigPortal()) {
        Serial.println(F("Portal Error"));
        ESP.restart();
        delay(5000);
    }

  }

  strncpy(mqttServer, mqtt_server.getValue(), MQTT_SERVER_LENGTH);
  strncpy(mqttTopic, mqtt_topic.getValue(), MQTT_TOPIC_LENGTH);
  mqttPort = atoi(mqtt_port.getValue());

  if(configMode) 
    writeEEPROM();
  
}

void otaSetup() {

  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
 
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println(F("Auth Failed"));
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println(F("Begin Failed"));
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println(F("Connect Failed"));
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println(F("Receive Failed"));
    } else if (error == OTA_END_ERROR) {
      Serial.println(F("End Failed"));
    }
  });

  ArduinoOTA.begin();
}

void e32Setup() {
  lora.begin();

  ResponseStructContainer c=lora.getConfiguration();
  Configuration config=*(Configuration *)c.data;
  Serial.println(c.status.getResponseDescription());
  Serial.println(c.status.code);

	
  config.ADDH = 0x00;
  config.ADDL = 0x00;
  config.CHAN = 0x0f;
  config.OPTION.fixedTransmission = FT_FIXED_TRANSMISSION;
  config.OPTION.transmissionPower = POWER_14;
  config.SPED.airDataRate = AIR_DATA_RATE_011_48;

  ResponseStatus rs = lora.setConfiguration(config, WRITE_CFG_PWR_DWN_LOSE);
  Serial.println(rs.getResponseDescription());
  Serial.println(rs.code);

}

void publishHeartbeat()
{
  sendHeartbeat = true;
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(128);

  readEEPROM();

  printMQTT();

  callWFM(true);

  otaSetup();

  initMQTT();

  e32Setup();

  mailServo.attach(servoPin, minUs, maxUs);
  mailServo.write(0);


  pinMode(CONFIG_BUTTON,INPUT);
  attachInterrupt(CONFIG_BUTTON, longPress, CHANGE);

  heartBeat.start();

  Serial.print(F("IP Addr "));
  Serial.println(WiFi.localIP()); 

}

void loop() {
if (sendHeartbeat) {
    Serial.println("Sending Beat");
    publishLifecycle("HB");

    sendHeartbeat = false;
  } 

  if(lora.available()>1) {
    ResponseStructContainer rc = lora.receiveMessage(4);
    Serial.println(rc.status.code);

    uint8_t *data=(uint8_t *)rc.data;
    
    for(int n=0;n<4;n++) {
      Serial.print(data[n],HEX);
      Serial.print(" ");
    }
    Serial.println("");

    uint16_t prox=(data[1]<<8) + data[0];
    char status[5];
    if(prox == 0xffff) {
      strcpy(status,"OPEN");
    } else if (prox < MAIL_THRESHOLD) {
      strcpy(status,"NONE");
      mailServo.write(0);
    } else {
      strcpy(status,"MAIL");
      mailServo.write(90);
    }

    double batt = ((double)data[2]+200)/100;
    double temp = ((double)data[3])/2 -30;    
    publishData(prox,status,batt,temp);

  }

  if(buttonLongPress) {
    callWFM(false);
    buttonLongPress = false;
  }
  

  heartBeat.update();

  ArduinoOTA.handle();
}

ICACHE_RAM_ATTR void longPress() {
  uint8_t curState = digitalRead(CONFIG_BUTTON);
  
  if (curState) {
  // Button Released
    if((millis() - lastPressTime) > 1000)
    {
      buttonLongPress = true;
    }

  } else {
  // Button Pressed
    lastPressTime = millis();
    buttonLongPress = false;
  }


}