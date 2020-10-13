
## Introduction

This is a small ESP8266 and ESP32 compatible program to make your Mitsubishi HVAC speak Modbus.

For additional resources, consult the following resources
- https://nicegear.nz/blog/hacking-a-mitsubishi-heat-pump-air-conditioner/
- https://github.com/SwiCago/HeatPump/
- https://github.com/rjdekker/MHI2MQTT/blob/master/README.md

I'm personally using dev version of ESP8266, which can power using 5V directly. No issues so far.

For dummies, this the connection:

- HVAC DC 12V (unconnected)
- HVAC DC 5V <-> ESP 5V
- HVAC GND <-> ESP GND
- HVAC TX <-> ESP RX
- HVAC RX <-> ESP TX

The CN105 cable I bought from [usastore.revolectrix.com](http://www.usastore.revolectrix.com/Products_2/Cellpro-4s-Charge-Adapters_2/Cellpro-JST-PA-Battery-Pigtail-10-5-Position).

## Getting started

1. Rename `secrets.h.example` to `secrets.h`. Replace with Wifi credentials
2. Check `constants.h` and modify settings accordingly
3. Upload SPIFFS / File System Image
4. Upload code

Remote upload works as well, uncomment relevant lines in `platformio.ini`.

## Operation

The programs kicks off a Modbus TCP server. In addition, it acts as Modbus client, writing the data to predefined Modbus server every ~1s. In my experience, at least with ESP 8266 the Modbus client is more reliable method of integration. You can enable/disable Modbus features in `constants.h`. Currently client is necessary to control the heatpump via Modbus.

The ON/OFF command is read from the remote Modbus server, and heatpump is commanded accordingly.

Please find the definition of Modbus data in `main.cpp` comments.

The ESP logs its operatoin via UDP. You can use wireshark or tcpdump to listen for the data. Example: `tcpdump -nnASs 1514 src 192.168.1.167 and port 514`

The program also opens up a simple web server for controlling the heatpump. With ESP8266 this is quite unreliable in practice.
