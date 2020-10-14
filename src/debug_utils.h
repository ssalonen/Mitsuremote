#ifndef DEBUG_UTILS_H__
#define DEBUG_UTILS_H__

#include "constants.h"
#include <Syslog.h>
#include <WiFiUdp.h>

unsigned long __lastDebugThrottled[50];
#define DEBUG_PRINT_THROTTLED(i, msg)              \
    if (millis() - __lastDebugThrottled[i] > 1000) \
    {                                              \
        DEBUG_PRINT(msg);                          \
        __lastDebugThrottled[i] = millis();        \
    }
#define DEBUG_PRINTLN_THROTTLED(i, msg)            \
    if (millis() - __lastDebugThrottled[i] > 1000) \
    {                                              \
        DEBUG_PRINTLN(msg);                        \
        __lastDebugThrottled[i] = millis();        \
    }

WiFiUDP udpClient;
std::unique_ptr<Syslog> syslog(new Syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, ESP_NAME, SYSLOG_APP_NAME));

#ifdef SYSLOG_LOGGING_ENABLED
#define SYSLOG(x)                                        \
    if (syslog)                                          \
    {                                                    \
        syslog->logf(LOG_INFO, "%s", String(x).c_str()); \
        yield();                                         \
    }
#else
#define SYSLOG(x)
#endif

// With ESP32 we are free to use Serial for printing
// when we use UART1 or UART2 for heatpump
#ifdef SERIAL_FREE_FOR_PRINT
#define DEBUG_PRINTLN(x)   \
    {                      \
        Serial.println(x); \
        SYSLOG(x)          \
    }
#define DEBUG_PRINT(x)   \
    {                    \
        Serial.print(x); \
        SYSLOG(x)        \
    }
#else
#define DEBUG_PRINTLN(x) SYSLOG(x)
#define DEBUG_PRINT(x) SYSLOG(x)
#endif

class DEBUG_SCOPE
{
public:
    DEBUG_SCOPE(const char *s)
    {
        this->s = s;
        DEBUG_PRINT(s);
        DEBUG_PRINTLN(" START");
    }
    ~DEBUG_SCOPE()
    {
        DEBUG_PRINT(s);
        DEBUG_PRINTLN(" END");
    }

private:
    const char *s;
};

#endif // DEBUG_UTILS_H__
