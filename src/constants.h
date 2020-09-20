#ifndef CONSTANTS_H__
#define CONSTANTS_H__

#include <IPAddress.h>
#include <Esp.h>

#define VERSION "2020-09-20"
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
#define ESP_NAME String("MitsuRemote-ESP_" + String(CHIP_ID) + String("v") + String(VERSION)).c_str()


#ifdef ESP8266
// One and only UART: UART 0
#define HEATPUMP_UART 0
#elif defined(ESP32)
// Use UART1 for heatpump, allowing UART0 for terminal
// UART 1, GPIO 9 & 10 for RX/TX. GPIO9="D2", GPIO10="""
#define HEATPUMP_UART 1
#endif

#if defined(DEBUG) || ( defined(ESP32) && HEATPUMP_UART != 0 )
#define SERIAL_FREE_FOR_PRINT
#endif

#include "secrets.h"

#define REMOTE_MODBUS_UNIT_ID ((uint8_t)1)
#define REMOTE_MODBUS_WRITE_INTERVAL_MILLIS 1000
#define REMOTE_MODBUS_READ_INTERVAL_MILLIS 1000

#define MODBUS_SERVER_ENABLED true
#define MODBUS_CLIENT_ENABLED true

#endif // CONSTANTS_H__
