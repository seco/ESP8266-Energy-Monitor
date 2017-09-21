# ESP8266-Energy-Monitor

### Description
The **ESP Energy Monitor** is a wi-fi energy monitor based on a ESP8266 board that uses XTM18S (or compatible) single phase energy meter. This energy monitor with MQTT

### Features
+ Support for ESP8266 boards. [View wiki](https://github.com/jorgeassuncao/ESP8266-Energy-Monitor/wiki/Compatible-boards)
+ MQTT enabled
+ Configurable topics to publish
+ Publishes various information over MQTT:
  + Current kWh
  + Current Watt
  + Accum kWh (pulses)
+ Retains the last measured value even if the board is rebooted, reset or powered off
+ Debug info via serial port
  + IP address
  + MAC address
  + Current kWh
  + Current Watt
  + Accum kWh (pulses)
+ Visual confirmation of operation using the internal blue LED



### To-do
+ ESP8266 Webserver to display the same information as the MQTT topics, and serial port, on a web browser
+ Use a non-invasive current sensor to get current (Amps) value and make reliable calculation for Apparent Power (kVA)
