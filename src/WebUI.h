#ifndef WebUI_H__
#define WebUI_H__

#include <memory>
#include <Arduino.h>
#include <HeatPump.h>
#ifdef ESP8266
#define WebServer ESP8266WebServer
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WebServer.h>
#endif
#include "utils.h"
#include "constants.h"

#define WEB_UI_REFRESH_RATE_SECS "10"

///
/// HTTP Server
/// Adapted from https://github.com/SwiCago/HeatPump/blob/master/examples/HP_cntrl_Fancy_web/HP_cntrl_Fancy_web.ino
///

String template_html(unsigned long prevHeatpumpComms, std::unique_ptr<HeatPump> &hp, const heatpumpSettings settings, const String &var);
heatpumpSettings updateHeatpumpFromHttpQueryParameters(std::unique_ptr<WebServer> const &httpServer, std::unique_ptr<HeatPump> const &hp, heatpumpSettings settings);

#endif // WebUI_H__
