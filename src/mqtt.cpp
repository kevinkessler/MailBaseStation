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

char prev_status[5]="????";
uint16_t prev_prox=0xFFFF;
double prev_temp=-99.0;
double prev_batt=10.0;

extern char mqttServer[MQTT_SERVER_LENGTH];
extern char mqttTopic[MQTT_TOPIC_LENGTH];
extern uint16_t mqttPort;

const char *tempJson = "{\"device_class\":\"temperature\",\"name\": \"Mail_Temperature\",\"state_topic\": \"%s/%s\", \"unit_of_measurement\": \"°C\", \"value_template\": \"{{ value_json.temperature}}\" }";
const char *proxJson = "{\"name\": \"Mail_Proximity\",\"state_topic\": \"%s/%s\", \"unit_of_measurement\": \"Count\", \"value_template\": \"{{ value_json.mailproximity}}\" }";
const char *statusJson = "{\"name\": \"Mail_Status\",\"state_topic\": \"%s/%s\", \"value_template\": \"{{ value_json.status}}\" }";
const char *batteryJson = "{\"name\": \"Mail_Battery\",\"state_topic\": \"%s/%s\", \"unit_of_measurement\": \"V\", \"value_template\": \"{{ value_json.battery}}\" }";
const char *stateJson = "{\"lifecycle\":\"%s\", \"mailproximity\":%d, \"status\":\"%.9s\",\"battery\":%.2f,\"temperature\":%.1f}";
const char *lifecycleJson = "{\"name\": \"Mail_Lifecycle\",\"state_topic\": \"%s/%s\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value_json.lifecycle}}\" }";

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
    char payload[512];
    char topic[75];

    sprintf(payload,tempJson,mqttTopic,"state");
    sprintf(topic,"%s_temperature/%s",mqttTopic,"config");

    publishMes(topic,payload);

    sprintf(payload,proxJson,mqttTopic,"state");
    sprintf(topic,"%s_proximity/%s",mqttTopic,"config");

    publishMes(topic,payload);

    sprintf(payload,statusJson,mqttTopic,"state");
    sprintf(topic,"%s_status/%s",mqttTopic,"config");

    publishMes(topic,payload);

    sprintf(payload,batteryJson,mqttTopic,"state");
    sprintf(topic,"%s_battery/%s",mqttTopic,"config");

    publishMes(topic,payload);

    sprintf(payload,lifecycleJson,mqttTopic,"state");
    sprintf(topic,"%s_lifecycle/%s",mqttTopic,"config");

    return publishMes(topic,payload);

}

boolean publishLifecycle(const char *lc) {
    char payload[100];
    sprintf(payload,stateJson,lc,prev_prox,prev_status,prev_batt,prev_temp);
    Serial.println(payload);

    char topic[MQTT_TOPIC_LENGTH + 10];
    sprintf(topic,"%s/%s",mqttTopic,"state");

    return publishMes(topic,payload);
}

boolean publishData(uint16_t prox, char *status, double battery, double temperature) {

    char payload[100];

    strcpy(prev_status,status);
    prev_prox=prox;
    prev_batt=battery;
    prev_temp=temperature;

    sprintf(payload,stateJson,"UPDATE",prox,status,battery,temperature);
    Serial.println(payload);

    char topic[MQTT_TOPIC_LENGTH + 10];
    sprintf(topic,"%s/%s",mqttTopic,"state");

    return publishMes(topic,payload);
}


void initMQTT() {
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    
    sprintf(subName, "maildisplay-%s", &(WiFi.macAddress().c_str())[9]);
    Serial.println(subName);

    publishConfig();
    publishLifecycle("START");
}
