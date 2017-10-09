/**
 *        _____  ____    ____  _______          _       
 *       |_   _||_   \  /   _||_   __ \        / \      
 *         | |    |   \/   |    | |__) |      / _ \     
 *     _   | |    | |\  /| |    |  __ /      / ___ \    
 *    | |__' |   _| |_\/_| |_  _| |  \ \_  _/ /   \ \_  
 *    `.____.'  |_____||_____||____| |___||____| |____| 
 * 
 * ESP Energy Monitor v0.6.1 by Jorge Assunção
 * 
 * Based on a project of timseebeck @ https://community.home-assistant.io/t/power-monitoring-with-an-xtm18s-and-mqtt/16316
 * Remote Debug over Telnet by JoaoLopesF @ https://github.com/JoaoLopesF/ESP8266-RemoteDebug-Telnet
 *
 * See Github for instructions on how to use this code : https://github.com/jorgeassuncao/ESP8266-Energy-Monitor
 * 
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * The entries marked with (**) must be changed to use your own values 
 */
 
/************* INCLUDE LIBRARIES *****************************************************************************/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "RemoteDebug.h"

/************* PROJECT AND VERSION *****************************************************************************/
const char* proj_ver = "ESP Energy Monitor v0.6.1 (09/10/2017)";        // Print project name and version

/************* CONFIG DEBUG *****************************************************************************/
RemoteDebug Debug;                                                      // Debug

/************* CONFIG WIFI *****************************************************************************/
const char* ssid = "wifi_ssid";                   // Wifi SSID (**)
const char* password = "wifi_password";           // Wifi password (**)

byte mac[6];                                      // MAC address
char myBuffer[15];                                // MAC string buffer

IPAddress ip(xxx,xxx,xxx,xxx);                    // IP address (**)
IPAddress dns(xxx,xxx,xxx,xxx);                   // DNS server (**)
IPAddress gateway(xxx,xxx,xxx,xxx);               // Gateway (**)
IPAddress subnet(xxx,xxx,xxx,xxx);                // Subnet mask (**)

#define HOST_NAME "ESP_Energy_Monitor_01"         // Hostname  (**)

ESP8266WebServer server(80);                      // Webserver port

/************* CONFIG MQTT *****************************************************************************/
const char* mqtt_server = "mqtt_server_address";        // MQTT server IP ou URL (**)
int mqtt_port = 1883;                                   // MQTT port (**)
const char* mqtt_username = "mqtt_user";                // MQTT user (**)
const char* mqtt_password = "mqtt_password";            // MQTT password (**)

/************* MQTT topics *****************************************************************************/
const char* mqtt_topic_watt = "ESP_Energy_Meter_01/watt";          // MQTT topic - watt (**)
const char* mqtt_topic_kwh = "ESP_Energy_Meter_01/kwh";            // MQTT topic - kwh (**)
const char* mqtt_topic_pulse = "ESP_Energy_Meter_01/pulse";        // MQTT topic - pulse (**)
const char* mqtt_topic_ip = "ESP_Energy_Meter_01/ip";              // MQTT topic - ip (**)
const char* mqtt_topic_mac = "ESP_Energy_Meter_01/mac";            // MQTT topic - mac (**)

/************* CONFIG PINS *****************************************************************************/
#define DIGITAL_INPUT_SENSOR 12           // Digital input D6 in Wemos D1 mini

int LED_pin = 2;                          // Internal LED in NodeMCU
#define BRIGHT 150                        // Maximum LED intensity (1-500)
#define INHALE 1500                       // Breathe-in time in milliseconds
#define PULSE INHALE*1000/BRIGHT          // Pulse
#define REST 1000                         // Pause between breathes

/************* CONFIG PULSES PER KWH *****************************************************************************/
#define PULSE_FACTOR 1000                 // Number of pulses of the meter per kWh

/************* CONFIG OTHER GLOBALS *****************************************************************************/
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

uint32_t mmLastTime = 0;
uint32_t mmTimeSeconds = 0;

int N = 1;                                // Number of times to loop
int runXTimes = 0;                        // Count times looped

/************* SETUP WIFI SERVER *****************************************************************************/
void handleRoot() {

  char temp[3000];                                              // Number of characters
  int sec = millis() / 1000;                                    // Convert from millis to seconds
  int min = sec / 60;                                           // Convert from seconds to minutes
  int hr = min / 60;                                            // Convert from minutes to hours
  
  String ipaddress = WiFi.localIP().toString();                 // Create IP address string
  char ipchar[ipaddress.length()+1];
  ipaddress.toCharArray(ipchar,ipaddress.length()+1);

  snprintf ( temp, 3000,                                        // HTML for the root page

  "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>%s</title>\
    <style>\
      body { background-color: #000000; font-family: Arial, Helvetica, Sans-Serif; color: #ffffff; }\
      H2 { color: #ff8c00; }\
    </style>\
  </head>\
  <body>\
    <h2>%s</h2>\
    <p><span style='font-size: 11pt; color: #a9a9a9;'><strong>The MQTT topic subscribed is </strong></span><br /><span style='font-size: 10pt; color: #f0f0f0;'><em>%s</em></span></p>\
    <p>&nbsp;</p>\
    <p><span style='font-size: 11pt; color: #a9a9a9;'><strong>IP Address:</strong></span> <span style='font-size: 10pt; color: #f0f0f0;'><em>%s</em></span></p>\
    <p><span style='font-size: 11pt; color: #a9a9a9;'><strong>MAC Address:</strong></span> <span style='font-size: 10pt; color: #f0f0f0;'><em>%s</em></span></p>\
    <p>&nbsp;</p>\
    <p><span style='font-size: 11pt; color: #a9a9a9;'><strong>Current kWh:</strong></span> <span style='font-size: 10pt; color: #f0f0f0;'><em>%s</em></span></p>\
    <p><span style='font-size: 11pt; color: #a9a9a9;'><strong>Current Watt:</strong></span> <span style='font-size: 10pt; color: #f0f0f0;'><em>%s</em></span></p>\
    <p><span style='font-size: 11pt; color: #a9a9a9;'><strong>Accum kWh:</strong></span> <span style='font-size: 10pt; color: #f0f0f0;'><em>%s</em></span></p>\
    <p>&nbsp;</p>\
    <p><span style='font-size: 11pt; color: #a9a9a9;'><strong>Uptime:</strong></span> <span style='font-size: 10pt; color: #f0f0f0;'><em>%02d:%02d:%02d</em></span></p>\
    <p>&nbsp;</p>\
    <p><span style='font-size: 11pt; color: #a9a9a9;'><strong>Reset board</strong></span> &nbsp; &nbsp; <input name='reset' type='button' value='reset' /></p>\
  </body>\
</html>",
    host_name,
    proj_ver,
    mqtt_topic_pulse,
    ipchar,
    myBuffer,
    kwhString,
    wattString,
    kwhaccumString,
    hr, min % 60, sec % 60
     );
     
     server.send ( 200, "text/html", temp );                    // Send HTML to server
}

void handleNotFound(){                                          // Handle page not found
  String message = "Page or File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

/************* SETUP WIFI *****************************************************************************/
void setup_wifi() {
  delay(20);

/** TELNET **/
  Debug.begin("ESP_Energy_Monitor_01");                                   // Initiaze the telnet server
  Debug.setResetCmdEnabled(true);                                         // Enable/disable (true/false) the reset command (true/false)
  Debug.showTime(false);                                                  // Enable/disable (true/false) timestamps
  Debug.showProfiler(false);                                              // Enable/disable (true/false) Profiler - time between messages of Debug
  Debug.showDebugLevel(false);                                            // Enable/disable (true/false) debug levels
  Debug.showColors(true);                                                 // Enable/disable (true/false) colors

  Serial.println("- - - - - - - - - - - - - - - - - -");                  // Block separator to serial interface
  Debug.println("- - - - - - - - - - - - - - - - - -");                   // Block separator to telnet debug interface
  Serial.println(proj_ver);                                               // Send project name and version to serial interface
  Debug.println(proj_ver);                                                // Send project name and version to telnet debug interface
  Serial.println("- - - - - - - - - - - - - - - - - -");                  // Block separator to serial interface
  Debug.println("- - - - - - - - - - - - - - - - - -");                   // Block separator to telnet debug interface
  Serial.println();                                                       // Send space to serial interface
  Debug.println();                                                        // Send space to telnet debug interface
  
  Serial.println();                                                       // Connecting to wifi network
  Serial.print("Connecting to "); Serial.println(ssid);                   // Send network name to serial interface
  Debug.printf("Connecting to "); Debug.println(ssid);                    // Send network name to telnet debug interface

  WiFi.config(ip, dns, gateway, subnet);                                  // Configure connection with IP, DNS, Gateway and subnet
  WiFi.mode(WIFI_STA);                                                    // Switch to STA mode
  WiFi.begin(ssid, password);                                             // Start wifi connection with SSID and Password
  WiFi.macAddress(mac);                                                   // Get MAC address of the node
  
  while (WiFi.status() != WL_CONNECTED) {                                 // Wait until connected to wifi
    delay(500);
    Serial.print(".");
  }

  Serial.println();                                                       // Block space to serial interface
  Debug.println();                                                        // Block space to telnet debug interface
  Serial.println("WiFi connected");                                       // Send successful connection to serial interface
  Debug.println("WiFi connected");                                        // Send successful connection to telnet debug interface
  
  Serial.print("IP address is "); Serial.println(WiFi.localIP());         // Send IP address to serial interface
  Debug.printf("IP address is "); Debug.println(WiFi.localIP());          // Send IP address to telnet debug interface

  sprintf(myBuffer,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);    // Get MAC address
  Serial.print("MAC address is "); Serial.println(myBuffer);                                      // Send MAC address to serial interface
  Debug.printf("MAC address is "); Debug.println(myBuffer);                                       // Send MAC address to telnet debug interface

}

/************* READ MQTT TOPIC *****************************************************************************/
void callback(char* topic, byte* payload, unsigned int length) {
  if (runXTimes < N) {                                                    // Read the topic only N times
  
  Serial.println();                                                       // Block space to serial interface
  Debug.println();                                                        // Block space to telnet debug interface
  Serial.print("Message arrived for topic [");                            // Send text to serial interface
  Debug.printf("Message arrived for topic [");                            // Send text to telnet debug interface
  Serial.print(topic); Serial.print("] ");                                // Send MQTT topic to serial interface
  Debug.printf(topic); Debug.printf("] ");                                // Send MQTT topic to telnet debug interface
  
  String value = "";                                                      // Store MQTT topic inside string
  for (int i=0;i<length;i++) {
    value += (char)payload[i];
  }
  
  kwhReading = value.toFloat();                                           // Convert kwhReading to float
  
  Serial.println();                                                       // Block space to serial interface
  Debug.println();                                                        // Block space to telnet debug interface
  Serial.print("Payload: "); Serial.println(kwhReading);                  // Send MQTT payload to serial interface
  Debug.printf("Payload: "); Debug.println(kwhReading);                   // Send MQTT payload to telnet debug interface
  Serial.println();                                                       // Block space to serial interface
  Debug.println();                                                        // Block space to telnet debug interface
  
  runXTimes++;                                                            // Increase loop count
  }
}
  
/************* RECONNECT MQTT *****************************************************************************/
void reconnect() {
  
  while (!client.connected()) {                                           // Loop until reconnected
    Serial.print("Attempting connection to MQTT server... ");             // Send text to serial interface
    Debug.printf("Attempting connection to MQTT server... ");             // Send text to telnet debug interface
    if (client.connect(host_name, mqtt_username, mqtt_password)) {        // Connect to MQTT brocker
      Serial.println(" connected!");                                      // Send text to serial interface
      Debug.println(" connected!");                                       // Send text to telnet debug interface
      client.subscribe(mqtt_topic_pulse);                                 // MQTT topic to subscribe
    } else {
      Serial.print("failed, rc=");                                        // Send text to serial interface
      Debug.printf("failed, rc=");                                        // Send text to telnet debug interface
      Serial.print(client.state());                                       // Send failure state to serial interface
      //Debug.printf(client.state());                                     // Send failure state to telnet debug interface
      Serial.println(" try again in 5 seconds");                          // Send text to serial interface
      Debug.println(" try again in 5 seconds!");                          // Send text to telnet debug interface
      delay(5000);                                                        // Wait 5 seconds before retrying
    }
  }
}

/************* SETUP *****************************************************************************/
void setup()
{
  pinMode(LED_pin, OUTPUT);                                               // Configure internal LED pin as output.

  if (MDNS.begin("esp8266")) {                                            // 
    Serial.println("MDNS responder started");                             // 
  }                                                                       // 

  server.on("/", handleRoot);                                             // Serve root page when asked

  server.onNotFound(handleNotFound);                                      // Serve page not found

  server.begin();                                                         // Start Webserver
  Serial.println("HTTP server started");                                  // Send text to serial interface
  Debug.println("HTTP server started");                                   // Send text to telnet debug interface
  
  Serial.begin(115200);                                                   // Start serial interface
  setup_wifi();                                                           // Start wifi
  client.setServer(mqtt_server, mqtt_port);                               // client.setServer(mqtt_server, 1883);
  client.setCallback(callback);                                           // Execute when MQTT subscribed message received
  
  // Use the internal pullup to be able to hook up this sketch directly to an energy meter with S0 output
  // If no pullup is used, the reported usage will be too high because of the floating pin
  pinMode(DIGITAL_INPUT_SENSOR,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DIGITAL_INPUT_SENSOR), onPulse, RISING);

  Debug.begin(host_name);                                                 // Start Telnet server
  
  // Initialize variables
  kwh = 0;
  lastSend=millis();
}

/************* LOOP *****************************************************************************/
void loop()
{

  server.handleClient();
  
    for (int ii=1;ii<BRIGHT;ii++){                  // Breath in 
      digitalWrite(LED_pin, LOW);                   // LED on
      delayMicroseconds(ii*10);                     // Wait
      digitalWrite(LED_pin, HIGH);                  // LED off
      delayMicroseconds(PULSE-ii*10);               // Wait
      delay(0);                                     // Prevent watchdog firing
    }
    for (int ii=BRIGHT-1;ii>0;ii--){                // Breath out
      digitalWrite(LED_pin, LOW);                   // LED on
      delayMicroseconds(ii*10);                     // Wait
      digitalWrite(LED_pin, HIGH);                  // LED off
      delayMicroseconds(PULSE-ii*10);               // Wait
      ii--;
      delay(0);                                     // Prevent watchdog firing
    }
      delay(REST);                                  // Pause before repeat
  
    if (!client.connected()) {                      // If client disconnects...
        reconnect();                                // ...connect again
    }

    client.loop();

    unsigned long now = millis();
    bool sendTime = now - lastSend > SEND_FREQUENCY;                              // Only send values at a maximum frequency
    
    if (sendTime) {
      Serial.println("- - - - - - - - - - - -");                                  // Block separator to serial interface
      Debug.println("- - - - - - - - - - - -");                                   // Block separator to telnet debug interface
      
      dtostrf(kwh, 2, 4, kwhString);                                              // Convert Current kWh to string
      client.publish(mqtt_topic_kwh,kwhString);                                   // Publish Current kWh to MQTT topic
      Serial.print("- Current kWh: "); Serial.println(kwhString);                 // Send Current kWh to serial interface
      Debug.printf("- Current kWh: "); Debug.println(kwhString);                  // Send Current kWh to telnet debug interface
      lastSend = now;                                                             // Update the send time after publishing
        
      double watt = kwh * 1000.0 * 3600.0 / (double)SEND_FREQUENCY * 1000.0;      // Calculate the power using the energy
      dtostrf(watt, 4, 2, wattString);                                            // Convert Current Watt to string
      client.publish(mqtt_topic_watt,wattString);                                 // Publish Current Watt to MQTT topic
      Serial.print("- Current Watt: "); Serial.println(wattString);               // Send Current Watt to serial interface
      Debug.printf("- Current Watt: "); Debug.println(wattString);                // Send Current Watt to telnet debug interface

      kwhaccum = kwhReading + ((double)pulseCount/((double)PULSE_FACTOR));        // Calculate the accumulated energy since the begining
      dtostrf(kwhaccum, 5, 2, kwhaccumString);                                    // Convert Accum kWh (pulses) to string
      client.publish(mqtt_topic_pulse,kwhaccumString, true);                      // Publish Accum kWh (pulses) to MQTT topic
      Serial.print("- Accum kWh: "); Serial.println(kwhaccumString);              // Send Accum kWh (pulses) to serial interface
      Debug.printf("- Accum kWh: "); Debug.println(kwhaccumString);               // Send Accum kWh (pulses) to telnet debug interface
      kwh = 0;                                                                    // Reset kWh count
        
      String ipaddress = WiFi.localIP().toString();                               // Create IP address string
      char ipchar[ipaddress.length()+1];
      ipaddress.toCharArray(ipchar,ipaddress.length()+1);
      client.publish(mqtt_topic_ip,ipchar);                                       // Publish IP address to MQTT topic

      Serial.println();                                                           // Block space to serial interface
      Debug.println();                                                            // Block space to telnet debug interface
    }

  Debug.handle();                                                                 // Remote debug over telnet

  yield();                                                                        // Yielding
}

/************* ON PULSE *****************************************************************************/
void onPulse()
{
    unsigned long newBlink = micros();
    unsigned long interval = newBlink-lastBlink;
    
    if (interval<10000L) {                                                        // Sometimes we get interrupt on RISING
            return;
    }
    
    kwh += 1.0 / (double)PULSE_FACTOR;                                            // Every time there is a pulse, the energy consumption is 1 [pulse] / PULSE_FACTOR [pulses/kWh]
    lastBlink = newBlink;
    pulseCount++;                                                                 // Accumulate the energy (it will be initialized again once MQTT message is sent)
}

// END
