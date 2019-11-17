#include "WebUI.h"

String template_html(unsigned long prevHeatpumpComms, std::unique_ptr<HeatPump> &hp, const heatpumpSettings settings, const String &var)
{
    if (var == "DEBUG_INFO")
    {
#ifdef DEBUG
        return "In DEBUG mode. Heatpump will not be connected.";
#else
        return "In PRODUCTION (DEBUG not defined) mode";
#endif
        return settings.connected ? "Connected to heatpump." : "Not connected to heatpump.";
    }
    else if (var == "VERSION")
    {
        return VERSION;
    }
    else if (var == "CONNECTED_INFO")
    {
        String lastCommsText = prevHeatpumpComms == 0 ? "N/A" : String((millis() - prevHeatpumpComms) / 1000);
        return settings.connected ? "Connected to heatpump. Last comms:" + lastCommsText + " seconds ago" : "Not connected to heatpump.";
    }
    else if (var == "UPTIME_SECS")
    {
        return String(millis() / 1000);
    }
    else if (var == "RATE")
    {
        return WEB_UI_REFRESH_RATE_SECS;
    }
    else if (var == "ROOMTEMP")
    {
        return String(hp->getRoomTemperature());
    }
    else if (var == "POWER")
    {
        return streq(settings.power, "ON") ? "checked" : "";
    }
    else if (var == "MODE_H")
    {
        return streq(settings.mode, "HEAT") ? "checked" : "";
    }
    else if (var == "MODE_D")
    {
        return streq(settings.mode, "DRY") ? "checked" : "";
    }
    else if (var == "MODE_C")
    {
        return streq(settings.mode, "COOL") ? "checked" : "";
    }
    else if (var == "MODE_F")
    {
        return streq(settings.mode, "FAN") ? "checked" : "";
    }
    else if (var == "MODE_A")
    {
        return streq(settings.mode, "AUTO") ? "checked" : "";
    }
    if (var == "FAN_A")
    {
        return streq(settings.fan, "AUTO") ? "checked" : "";
    }
    else if (var == "FAN_Q")
    {
        return streq(settings.fan, "QUIET") ? "checked" : "";
    }
    else if (var == "FAN_1")
    {
        return streq(settings.fan, "1") ? "checked" : "";
    }
    else if (var == "FAN_2")
    {
        return streq(settings.fan, "2") ? "checked" : "";
    }
    else if (var == "FAN_3")
    {
        return streq(settings.fan, "3") ? "checked" : "";
    }
    else if (var == "FAN_4")
    {
        return streq(settings.fan, "4") ? "checked" : "";
    }
    else if (var == "VANE_V")
    {
        return settings.vane;
    }
    else if (var == "VANE_C")
    {
        return streq(settings.vane, "AUTO") ? "rotate0" :                   //
                   streq(settings.vane, "1") ? "rotate0" :                  //
                       streq(settings.vane, "2") ? "rotate22" :             //
                           streq(settings.vane, "3") ? "rotate45" :         //
                               streq(settings.vane, "4") ? "rotate67" :     //
                                   streq(settings.vane, "5") ? "rotate90" : //
                                       streq(settings.vane, "SWING") ? "rotateV" : "";
    }
    else if (var == "VANE_T")
    {
        return streq(settings.vane, "AUTO") ? "AUTO" : "&#10143;";
    }
    if (var == "WIDEVANE_V")
    {
        return settings.wideVane;
    }
    if (var == "WIDEVANE_C")
    {
        return streq(settings.wideVane, "<<") ? "rotate157" :                //
                   streq(settings.wideVane, "<") ? "rotate124" :             //
                       streq(settings.wideVane, "|") ? "rotate90" :          //
                           streq(settings.wideVane, ">") ? "rotate57" :      //
                               streq(settings.wideVane, ">>") ? "rotate22" : //
                                   streq(settings.wideVane, "<>") ? "" :     //
                                       streq(settings.wideVane, "SWING") ? "rotateH" : "";
    }
    else if (var == "WIDEVANE_T")
    {
        return streq(settings.wideVane, "<>") ? //
                   "<div class='rotate124'>&#10143;</div>&nbsp;<div class='rotate57'>&#10143;</div>"
                                              : "&#10143;";
    }
    return String();
}

heatpumpSettings updateHeatpumpFromHttpQueryParameters(std::unique_ptr<WebServer> const &httpServer, std::unique_ptr<HeatPump> const &hp, heatpumpSettings settings)
{

    bool update = false;
    if (httpServer->hasArg("PWRCHK"))
    {
        settings.power = httpServer->arg("POWER") ? "ON" : "OFF";
        update = true;
    }
    if (httpServer->hasArg("MODE"))
    {
        settings.mode = httpServer->arg("MODE").c_str();
        update = true;
    }
    if (httpServer->hasArg("TEMP"))
    {
        settings.temperature = httpServer->arg("TEMP").toInt();
        update = true;
    }
    if (httpServer->hasArg("FAN"))
    {
        settings.fan = httpServer->arg("FAN").c_str();
        update = true;
    }
    if (httpServer->hasArg("VANE"))
    {
        settings.vane = httpServer->arg("VANE").c_str();
        update = true;
    }
    if (httpServer->hasArg("WIDEVANE"))
    {
        settings.wideVane = httpServer->arg("WIDEVANE").c_str();
        update = true;
    }
    if (update)
    {
#ifdef DEBUG
        DEBUG_PRINTLN("In debug mode, not syncing/connecting heat pump");
#else
        hp->setSettings(settings);
        hp->update();
#endif
    }
    return settings;
}
