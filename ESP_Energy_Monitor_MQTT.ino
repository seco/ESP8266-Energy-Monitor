/**
 * This script is based on these sources:
 *
 * To count pulses using interruptions:  https://github.com/mysensors/MySensors/blob/master/examples/EnergyMeterPulseSensor/EnergyMeterPulseSensor.ino
 * To connect to Wifi and publish MQTT messages:  https://github.com/knolleary/pubsubclient/blob/master/examples/mqtt_esp8266/mqtt_esp8266.ino
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * DESCRIPTION
 * - Use this sensor to measure kWh and Watt of your house meter.
 * - You need to set the correct pulsefactor of your meter (pulses per kWh).
 * - You have to update the information related to the MQTT server
 * - Reports to different MQTT topics every SEND_FREQUENCY miliseconds
 * 
 * - Flash using: Board "NodeMCU 1.0 (ESP-12E Module)", CPU Frequency "80MHz", Flash Size "4M (3M SPIFFS)" and Upload Speed "115200"
 *
 * (1)- Energy (in kWh) consumed in the last SEND_FREQUENCY/1000 seconds
 * (2)- Power (in W) consumed in the last SEND_FREQUENCY/1000 seconds
 * (3)- Energy (in kWh) consumed since the device was switched on
 * There could be errors in (1) and (2) since pulse counting and information sending events
 * do not take place at the same time. (3), if enough time has passed by, is as accurate as
 * the one from the measuring device
 * 
 * v3
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>   / NEWNEW

/************************* CONFIG WIFI *********************************/
const char* ssid = "UNIVERSE";                    // Wifi SSID
const char* password = "2526F68597";              // Wifi password

byte mac[6];                                      // MAC address

IPAddress ip(192,168,1,220);                      // IP address
IPAddress dns(192,168,1,1);                       // DNS server
IPAddress gateway(8,8,8,8);                       // Gateway
IPAddress subnet(255,255,255,0);                  // Subnet mask

/**************************** CONFIG MQTT ************************************/
const char* mqtt_server = "192.168.1.200";      // MQTT server
const char* mqtt_username = "homeassistant";                    // MQTT user
const char* mqtt_password = "JA2508ca72";                       // MQTT password

/**************************** MQTT topics ************************************/
const char* mqtt_topic_watt = "ESP_Energy_Meter_01/watt_test";   // MQTT topic - watt
const char* mqtt_topic_kwh = "ESP_Energy_Meter_01/kwh_test";     // MQTT topic - kwh
const char* mqtt_topic_pulse = "ESP_Energy_Meter_01/pulse_test"; // MQTT topic - pulse
const char* mqtt_topic_ip = "ESP_Energy_Meter_01/ip_test";       // MQTT topic - ip
const char* mqtt_topic_mac = "ESP_Energy_Meter_01/mac_test";     // MQTT topic - mac


/**************************** CONFIG PINS ************************************/
#define DIGITAL_INPUT_SENSOR 12   // The digital input D6 in Wemos D1 mini

/**************************** CONFIG PULSES PER KWH ************************************/
#define PULSE_FACTOR 1000         // Number of pulses per kWh of your meeter

/**************************** CONFIG GLOBALS ************************************/
unsigned long SEND_FREQUENCY = 15000;   // Minimum time between send (in milliseconds)
volatile unsigned long pulseCount = 0;
volatile unsigned long lastBlink = 0;
double kwh;
unsigned long lastSend;

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
char wattString[7];
char kwhString[7];
double kwhaccum;    // NEWNEW
char kwhaccumString[7];    // NEWNEW
float kwhReading;    // NEWNEW

/**************************** SETUP WIFI ************************************/
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.config(ip, dns, gateway, subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.macAddress(mac);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("MAC: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);
}

/**************************** READ MQTT TOPIC ************************************/
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String value = "";   // NEWNEW
  for (int i=0;i<length;i++) {
    value += (char)payload[i];   // NEWNEW
  }
  kwhReading = value.toFloat();   // NEWNEW
  
  Serial.println();
  Serial.print("value: ");   // NEWNEW
  Serial.println(value);   // NEWNEW
  Serial.print("kwhReading: ");   // NEWNEW
  Serial.println(kwhReading);   // NEWNEW
  Serial.println();
}
  
/**************************** RECONNECT MQTT ************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266ClientTest", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("ESP_Energy_Meter_01/pulse_test");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/**************************** SETUP ************************************/
void setup()
{

  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  // Use the internal pullup to be able to hook up this sketch directly to an energy meter with S0 output
  // If no pullup is used, the reported usage will be too high because of the floating pin
  pinMode(DIGITAL_INPUT_SENSOR,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DIGITAL_INPUT_SENSOR), onPulse, RISING);
  
  // Initialization of variables
  kwh = 0;
  lastSend=millis();

}

/**************************** LOOP ************************************/
void loop()
{
  
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long now = millis();
    // Only send values at a maximum frequency
    bool sendTime = now - lastSend > SEND_FREQUENCY;
    if (sendTime) {
      
        // convert to a string with 2 digits before the comma and 2 digits for precision
        dtostrf(kwh, 2, 4, kwhString);
        client.publish(mqtt_topic_kwh,kwhString);  // Publish kwh to MQTT topic
        Serial.print("kwh: ");   // NEWNEW
        Serial.println(kwh);    // NEWNEW
        Serial.print("kwhString: ");   // NEWNEW
        Serial.println(kwhString);    // NEWNEW
        lastSend = now;  // once every thing is published we update the send time
        
        // We calculate the power using the energy
        double watt = kwh * 1000.0 * 3600.0 / (double)SEND_FREQUENCY * 1000.0;
        dtostrf(watt, 4, 2, wattString);
        client.publish(mqtt_topic_watt,wattString);  // Publish watt to MQTT topic
        Serial.print("watt: ");    // NEWNEW
        Serial.println(watt);   // NEWNEW
        Serial.print("wattString: ");    // NEWNEW
        Serial.println(wattString);   // NEWNEW
        
        // We calculate the accumulated energy since the begining using pulses count
        // kwhaccum = kwhaccum + kwhReading;
        kwhaccum = kwhReading + ((double)pulseCount/((double)PULSE_FACTOR));
        
        dtostrf(kwhaccum, 5, 2, kwhaccumString);
        
        client.publish(mqtt_topic_pulse,kwhaccumString, true);  // Publish pulses to MQTT topic   <<<<- - - - - - - - -
        
        Serial.print("kwhReading: ");   // NEWNEW
        Serial.println(kwhReading);   // NEWNEW
        Serial.print("kwhaccum: ");   // NEWNEW
        Serial.println(kwhaccum);   // NEWNEW
        Serial.print("kwhaccumString: ");   // NEWNEW
        Serial.println(kwhaccumString);   // NEWNEW
        kwh = 0;
        
        // Send IP address
        String ipaddress = WiFi.localIP().toString();
        char ipchar[ipaddress.length()+1];
        ipaddress.toCharArray(ipchar,ipaddress.length()+1);
        client.publish(mqtt_topic_ip,ipchar);  // Publish IP address
    }

}

/**************************** ON PULSE ************************************/
void onPulse()
{
    unsigned long newBlink = micros();
    unsigned long interval = newBlink-lastBlink;
    if (interval<10000L) { // Sometimes we get interrupt on RISING
            return;
    }
    
    // Every time there is a pulse, the energy consumption is 1 [pulse] / PULSE_FACTOR [pulses/kWh]
    // We also want to accumulate the energy (it will be initialized again once MQTT message is sent)
    kwh += 1.0 / (double)PULSE_FACTOR;
    lastBlink = newBlink;
    pulseCount++;
}
