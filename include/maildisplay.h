/**
 *  @filename   :   maildisplay.h
 *  @brief      :   Lora Mail Sensor Base Station Receiver and MQTT Transmitter
 *  @author     :   Kevin Kessler
 *
 * Copyright (C) 2020 Kevin Kessler
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

#ifndef INCLUDE_MAILDISPLAY_H_
#define INCLUDE_MAILDISPLAY_H_

#define MQTT_SERVER_LENGTH 30
#define MQTT_TOPIC_LENGTH 40

#define CONFIG_BUTTON 0

ICACHE_RAM_ATTR void longPress(void);
void initMQTT(void);
boolean publishData(uint8_t prox, uint8_t flagADC, uint8_t battery, uint8_t temperature);
boolean publishConfig(void);
void publishHeartbeat(void);
void callWFM(bool connect);

#endif /* INCLUDE_MAILDISPLAY_H_ */