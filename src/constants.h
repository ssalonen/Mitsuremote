#ifndef CONSTANTS_H__
#define CONSTANTS_H__

#include <IPAddress.h>
#include <Esp.h>

#define VERSION "2019-01-05"
#define SYSLOG_LOGGING_ENABLED 1

// Syslog server connection info
#define SYSLOG_SERVER "192.168.8.2"
#define SYSLOG_PORT 514
// Info for syslog messages
#define SYSLOG_APP_NAME "MitsuRemote"

// try to connect in setup for this long before 
// shutting down the pump and restarting
#define WIFI_RETRY_MILLIS 20000

#define RESET_COUNT 5
#define RESET_TIMEOUT_MILLIS 5000
#define RESET_ADDRESS 0

#ifdef ESP8266
#define CHIP_ID (ESP.getChipId())
#elif defined(ESP32)
#define CHIP_ID ((uint16_t)(ESP.getEfuseMac()>>32))
#endif
#define CONFIG_AP_NAME String("MitsuRemote-ESP_" + String(CHIP_ID) + String("v") + String(VERSION)).c_str()

#include "secrets.h"

#define REMOTE_MODBUS_UNIT_ID ((uint8_t)1)
#define REMOTE_MODBUS_WRITE_INTERVAL_MILLIS 1000
#define REMOTE_MODBUS_READ_INTERVAL_MILLIS 1000

#define MODBUS_SERVER_ENABLED true
#define MODBUS_CLIENT_ENABLED true

#endif // CONSTANTS_H__
