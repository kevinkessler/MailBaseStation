/**
 *  @filename   :   mqtt.cpp
 *  @brief      :   Lora Mail Sensor Base Station Receiver and MQTT Transmitter
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

#include <WiFi.h> 
#include "PubSubClient.h"
#include "maildisplay.h"


WiFiClient espClient;
PubSubClient mqttClient(espClient);
char subName[25];

extern char mqttServer[MQTT_SERVER_LENGTH];
extern char mqttTopic[MQTT_TOPIC_LENGTH];
extern uint16_t mqttPort;

const char *configJson = "{\"device_class\":\"temperature\",\"name\": \"RailcoreFuse\",\"state_topic\": \"%s/%s\", \"unit_of_measurement\": \"°C\", \"value_template\": \"{{ value_json.temperature}}\" }";
const char *stateJson = "{\"mailProximity\":%d, \"flagADC\":%d,\"battery\":%.2f,\"temperature\":%.1f}";

void mqttCallback(char *topic, byte *payload, uint16_t length) {
    Serial.printf("Message Received on topic %s\n", topic);
}

static void reconnect() {
    if(mqttClient.connect(subName)) {
        Serial.println("MQTT Connected");
        publishConfig();
    } else {
        char errorMes[50];
        sprintf(errorMes, "MQTT Connection failed, rc=%d",mqttClient.state());
        Serial.println(errorMes);
    }

}

static boolean publishMes(char *topic, char *payload) {
    if(!mqttClient.connected())
        reconnect();
        if(!mqttClient.connected())
            return false;

    if(!mqttClient.publish(topic, payload)) {
        char errorMes[50];
        sprintf(errorMes, "MQTT Publish failed, rc=%d",mqttClient.state());
        Serial.println(errorMes);
        return false;
    }

    return true;
}

boolean publishConfig() {
    uint8_t configLen = strlen(configJson);
    char payload[configLen + MQTT_TOPIC_LENGTH + 10];
    sprintf(payload,configJson,mqttTopic,"state");

    char topic[MQTT_TOPIC_LENGTH+10];
    sprintf(topic,"%s/%s",mqttTopic,"config");

    return publishMes(topic,payload);
}

boolean publishData(uint8_t prox, uint8_t flagADC, uint8_t battery, uint8_t temperature) {

    char payload[100];
    double batt = ((double)battery+150)/100;
    double temp = ((double)temperature)/2 -30;
    sprintf(payload,stateJson,prox,flagADC,batt,temp);
    Serial.println(payload);

    char topic[MQTT_TOPIC_LENGTH + 10];
    sprintf(topic,"%s/%s",mqttTopic,"state");

    return publishMes(topic,payload);
}


void initMQTT() {
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    
    sprintf(subName, "maildisplay-%s", &(WiFi.macAddress().c_str())[9]);
}
