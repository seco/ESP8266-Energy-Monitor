/**
 * 
 * ESP Energy Monitor v3 by Jorge Assunção
 * 
 * Based on a project of timseebeck https://community.home-assistant.io/u/timseebeck on https://community.home-assistant.io
 * See timseebeck project here: https://community.home-assistant.io/t/power-monitoring-with-an-xtm18s-and-mqtt/16316
 * 
 * See Github for instructions on how to use this code : https://github.com/jorgeassuncao/ESP8266-Energy-Monitor
 * 
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 */
 
/************* INCLUDES *****************************************************************************/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/************* CONFIG WIFI *****************************************************************************/
const char* ssid = "wifi_ssid";                   // Wifi SSID
const char* password = "wifi_password";           // Wifi password

byte mac[6];                                      // MAC address

IPAddress ip(xxx,xxx,xxx,xxx);                    // IP address
IPAddress dns(xxx,xxx,xxx,xxx);                   // DNS server
IPAddress gateway(xxx,xxx,xxx,xxx);               // Gateway
IPAddress subnet(xxx,xxx,xxx,xxx);                // Subnet mask

/************* CONFIG MQTT *****************************************************************************/
const char* mqtt_server = "mqtt_server_address";        // MQTT server IP ou URL
const char* mqtt_username = "mqtt_user";                // MQTT user
const char* mqtt_password = "mqtt_password";            // MQTT password

/************* MQTT topics *****************************************************************************/
const char* mqtt_topic_watt = "ESP_Energy_Meter_01/watt";       // MQTT topic - watt
const char* mqtt_topic_kwh = "ESP_Energy_Meter_01/kwh";         // MQTT topic - kwh
const char* mqtt_topic_pulse = "ESP_Energy_Meter_01/pulse";     // MQTT topic - pulse
const char* mqtt_topic_ip = "ESP_Energy_Meter_01/ip";           // MQTT topic - ip
const char* mqtt_topic_mac = "ESP_Energy_Meter_01/mac";         // MQTT topic - mac


/************* CONFIG PINS *****************************************************************************/
#define DIGITAL_INPUT_SENSOR 12           // Digital input D6 in Wemos D1 mini

int LED_pin = 2;                          // Internal LED in NodeMCU
#define BRIGHT 150                        // Maximum LED intensity (1-500)
#define INHALE 1500                       // Breathe-in time in milliseconds
#define PULSE INHALE*1000/BRIGHT          // Pulse
#define REST 1000                         // Pause between breathes

/************* CONFIG PULSES PER KWH *****************************************************************************/
#define PULSE_FACTOR 1000                 // Number of pulses per kWh of the meter

/************* CONFIG GLOBALS *****************************************************************************/
unsigned long SEND_FREQUENCY = 15000;     // Minimum time between send in milliseconds
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
double kwhaccum;
char kwhaccumString[7];
float kwhReading;

/************* SETUP WIFI *****************************************************************************/
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

/************* READ MQTT TOPIC *****************************************************************************/
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived for topic [");
  Serial.print(topic);
  Serial.print("] ");
  String value = "";
  for (int i=0;i<length;i++) {
    value += (char)payload[i];
  }
  kwhReading = value.toFloat();
  Serial.println();
  Serial.print("Last kWh Reading: ");
  Serial.println(kwhReading);
  Serial.println();
}
  
/************* RECONNECT MQTT *****************************************************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting connection to MQTT server... ");
    // Attempt to connect
    if (client.connect("ESPenergyMonitor01", mqtt_username, mqtt_password)) {
      Serial.println(" connected!");
      client.subscribe("ESP_Energy_Meter_01/pulse");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/************* SETUP *****************************************************************************/
void setup()
{
  pinMode(LED_pin, OUTPUT);   // Configure internal LED pin as output.
  
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

/************* LOOP *****************************************************************************/
void loop()
{
    // Breath in 
    for (int ii=1;ii<BRIGHT;ii++){
      digitalWrite(LED_pin, LOW);         // LED on
      delayMicroseconds(ii*10);           // wait
      digitalWrite(LED_pin, HIGH);        // LED off
      delayMicroseconds(PULSE-ii*10);     // wait
      delay(0);                           // prevent watchdog firing
    }
    // Breath in
    for (int ii=BRIGHT-1;ii>0;ii--){
      digitalWrite(LED_pin, LOW);         // LED on
      delayMicroseconds(ii*10);           // wait
      digitalWrite(LED_pin, HIGH);        // LED off
      delayMicroseconds(PULSE-ii*10);     // wait
      ii--;
      delay(0);                           // prevent watchdog firing
    }
      delay(REST);                        // pause before repeat
  
    if (!client.connected()) {
        reconnect();
    }
    
    client.loop();

    unsigned long now = millis();
    // Only send values at a maximum frequency
    bool sendTime = now - lastSend > SEND_FREQUENCY;
    
    if (sendTime) {
      dtostrf(kwh, 2, 4, kwhString);                                              // Convert Current kWh to string
      client.publish(mqtt_topic_kwh,kwhString);                                   // Publish Current kWh to MQTT topic
      Serial.print("- Current kWh: "); Serial.println(kwhString);                 // Publish Current kWh to serial interface
      lastSend = now;                                                             // Update the send time after publishing
        
      double watt = kwh * 1000.0 * 3600.0 / (double)SEND_FREQUENCY * 1000.0;      // Calculate the power using the energy
      dtostrf(watt, 4, 2, wattString);                                            // Convert Current Watt to string
      client.publish(mqtt_topic_watt,wattString);                                 // Publish Current Watt to MQTT topic
      Serial.print("- Current Watt: "); Serial.println(wattString);               // Publish Current Watt to serial interface

      kwhaccum = kwhReading + ((double)pulseCount/((double)PULSE_FACTOR));        // Calculate the accumulated energy since the begining
      dtostrf(kwhaccum, 5, 2, kwhaccumString);                                    // Convert Accum kWh (pulses) to string
      client.publish(mqtt_topic_pulse,kwhaccumString, true);                      // Publish Accum kWh (pulses) to MQTT topic
      Serial.print("- Accum kWh: "); Serial.println(kwhaccumString);              // Publish Accum kWh (pulses) to serial interface
      kwh = 0;                                                                    // Reset kWh count
        
      // Send IP address
      String ipaddress = WiFi.localIP().toString();                               // Create IP address string
      char ipchar[ipaddress.length()+1];
      ipaddress.toCharArray(ipchar,ipaddress.length()+1);
      client.publish(mqtt_topic_ip,ipchar);                                       // Publish IP address to MQTT topic
    }
}

/************* ON PULSE *****************************************************************************/
void onPulse()
{
    unsigned long newBlink = micros();
    unsigned long interval = newBlink-lastBlink;
    
    if (interval<10000L) {                          // Sometimes we get interrupt on RISING
            return;
    }
    
    kwh += 1.0 / (double)PULSE_FACTOR;    // Every time there is a pulse, the energy consumption is 1 [pulse] / PULSE_FACTOR [pulses/kWh]
    lastBlink = newBlink;
    pulseCount++;                         // Accumulate the energy (it will be initialized again once MQTT message is sent)
}
