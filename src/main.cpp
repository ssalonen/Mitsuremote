/**
 * 
 * COILS
 * 0: reboot (write 1 to reboot)
 * 1: reboot (write 1 to reboot)
 * 
 * HOLDING REGISTERS:
 * READ by ESP (not written):
 * 0: POWER, integer. Values 0="OFF", 1="ON". Command for powering HVAC on/off.
 * WRITTEN BY ESP (not read):
 * 1: TIMEOUT, integer. Reseted to zero by ESP.
 * 2: SET TEMPERATURE, Celsius*10. Current set temperature.
 * 3: POWER, integer. Values 0="OFF", 1="ON". Current power status
 * 4: MODE, integer. Values: 0..4={"HEAT", "DRY", "COOL", "FAN", "AUTO"}
 * 5: FAN, integer. Values: 0..5={"AUTO", "QUIET", "1", "2", "3", "4"}
 * 6: VANE, integer. Values: 0..6={"AUTO", "1", "2", "3", "4", "5", "SWING"}
 * 7: WIDEVANE. Values: 0..6={"<<", "<", "|", ">", ">>", "<>", "SWING"}
 * 8: CONNECTED, bool. 0=false, true otherwise
 * 9: ROOM_TEMPERATURE, Celsius*10.
 * 10: OPERATING. bool. 0=false, true otherwise
 * 11: MILLIS_SINCE_LAST_COMMS, integer.
 * 
 * */

// Uncomment if in DEBUG mode. This means
// - serial is used for console
// - heatpump connection disabled
//#define DEBUG 1

#include "constants.h"

#include <memory>
#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <HeatPump.h>
#include <SoftwareSerial.h> // Dependency of ModbusIP, must be imported here
#include <ModbusIP_ESP8266.h>
#include <ArduinoOTA.h>
#ifdef ESP8266
#define WebServer ESP8266WebServer
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WebServer.h>
#endif
#include <DNSServer.h>
#include <EspHtmlTemplateProcessor.h>
#include "WebUI.h"
#include "debug_utils.h"
#include "utils.h"

#ifdef ESP8266
using Dir = fs::Dir;
#endif

// Amount of tries to do modbus operations
#define MODBUS_RETRIES 5
// Modbus retry sleep
#define MODBUS_RETRY_SLEEP 100

#define COILS_LEN 2
#define COIL_RESET_INDEX 0
#define COIL_REBOOT_INDEX 1

#define HOLDING_LEN 12
// READ registers
// 0: hvac power on (setting from PLC)

// WRITE registers:
#define HOLDING_REG_TIMEOUT_COUNTER 1
#define HOLDING_REG_TEMPERATURE_INDEX 2
#define HOLDING_REG_POWER_INDEX 3
#define HOLDING_REG_MODE_INDEX 4
#define HOLDING_REG_FAN_INDEX 5
#define HOLDING_REG_VANE_INDEX 6
#define HOLDING_REG_WIDEVANE_INDEX 7
#define HOLDING_REG_CONNECTED_INDEX 8
#define HOLDING_REG_ROOM_TEMPERATURE_INDEX 9
#define HOLDING_REG_OPERATING_INDEX 10
#define HOLDING_REG_MILLIS_SINCE_LAST_COMMS_INDEX 11

#define HOLDING_READ_COUNT 1
#define HOLDING_WRITE_COUNT (HOLDING_LEN - HOLDING_READ_COUNT)
static_assert(HOLDING_READ_COUNT == HOLDING_REG_TIMEOUT_COUNTER, "Index mismatch");

//
// do not change ordering of below arrays
//
#define POWER_SIZE 2
static const char *POWER_MAP[POWER_SIZE] = {"OFF", "ON"};
#define MODE_SIZE 5
static const char *MODE_MAP[MODE_SIZE] = {"HEAT", "DRY", "COOL", "FAN", "AUTO"};
#define FAN_SIZE 6
static const char *FAN_MAP[FAN_SIZE] = {"AUTO", "QUIET", "1", "2", "3", "4"};
#define VANE_SIZE 7
static const char *VANE_MAP[VANE_SIZE] = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
#define WIDEVANE_SIZE 7
static const char *WIDEVANE_MAP[WIDEVANE_SIZE] = {"<<", "<", "|", ">", ">>", "<>", "SWING"};
static std::unique_ptr<HeatPump> hp(new HeatPump());
static std::unique_ptr<ModbusIP> mb(new ModbusIP());
static std::unique_ptr<WebServer> httpServer;
static std::array<uint16_t, HOLDING_WRITE_COUNT> holdingDataWrite;
static std::array<uint16_t, HOLDING_READ_COUNT> holdingDataRead;

static unsigned long prevHeatpumpComms;
static unsigned long prevModbusWrite;
static unsigned long prevModbusRead;
static unsigned long prevWifiConnected;
static bool lastCommandPower;
// At boot, we take the power on/off command from the PLC
static bool hvacCommandsPending = true;
static bool otaInProgress;
static HardwareSerial heatpumpSerial(HEATPUMP_UART);

#define IMPL_MAP_HELPERS(FROM_STR_FUN, FROM_NUM_FUN, VALUES_SIZE, VALUES) \
  uint16_t FROM_STR_FUN(const char *lookupValue)                          \
  {                                                                       \
    if (lookupValue == NULL)                                              \
    {                                                                     \
      return -1;                                                          \
    }                                                                     \
    for (int i = 0; i < VALUES_SIZE; i++)                                 \
    {                                                                     \
      if (streq(lookupValue, VALUES[i]))                                  \
      {                                                                   \
        return i;                                                         \
      }                                                                   \
    }                                                                     \
    return -1;                                                            \
  }                                                                       \
  const char *FROM_NUM_FUN(const uint16_t index)                          \
  {                                                                       \
    if (index >= VALUES_SIZE)                                             \
    {                                                                     \
      return NULL;                                                        \
    }                                                                     \
    else                                                                  \
    {                                                                     \
      return VALUES[index];                                               \
    }                                                                     \
  }
IMPL_MAP_HELPERS(fromStrPower, fromIndexPower, POWER_SIZE, POWER_MAP);
IMPL_MAP_HELPERS(fromStrMode, fromIndexMode, MODE_SIZE, MODE_MAP);
IMPL_MAP_HELPERS(fromStrFan, fromIndexFan, FAN_SIZE, FAN_MAP);
IMPL_MAP_HELPERS(fromStrVane, fromIndexVane, VANE_SIZE, VANE_MAP);
IMPL_MAP_HELPERS(fromStrWideVane, fromIndexWideVane, WIDEVANE_SIZE, WIDEVANE_MAP);
void restart()
{
  ESP.restart();
  while (true)
  {
    // wait for reboot
    delay(1000);
  }
}

// Callback function to read corresponding DI
uint16_t coilRead(TRegister *reg, uint16_t val)
{
  return 0;
}
// Callback function to write-protect DI
uint16_t coilWrite(TRegister *reg, uint16_t val)
{
  uint8_t address = reg->address.address;
  if (val == 0)
    return reg->value;

  switch (address)
  {
  case COIL_RESET_INDEX:
    DEBUG_PRINT("Reset via modbus");
    restart();
    return 0;
    break;
  case COIL_REBOOT_INDEX:
    DEBUG_PRINT("Reboot via modbus");
    restart();
    return 0;
    break;
  default:
    break;
  }
  return -1;
}

uint16_t getHoldingRegister(uint8_t address)
{
  switch (address)
  {
  case HOLDING_REG_TIMEOUT_COUNTER:
    return 0;
    break;
  case HOLDING_REG_TEMPERATURE_INDEX:
    return static_cast<uint16_t>(round(hp->getTemperature() * 10));
    break;
  case HOLDING_REG_POWER_INDEX:
    return fromStrPower(hp->getPowerSetting());
    break;
  case HOLDING_REG_MODE_INDEX:
    return fromStrMode(hp->getModeSetting());
    break;
  case HOLDING_REG_FAN_INDEX:
    return fromStrFan(hp->getFanSpeed());
    break;
  case HOLDING_REG_VANE_INDEX:
    return fromStrFan(hp->getVaneSetting());
    break;
  case HOLDING_REG_WIDEVANE_INDEX:
    return fromStrWideVane(hp->getWideVaneSetting());
    break;
  case HOLDING_REG_CONNECTED_INDEX:
    return static_cast<uint16_t>(hp->getSettings().connected ? UINT16_C(1) : UINT16_C(0));
    break;
  case HOLDING_REG_ROOM_TEMPERATURE_INDEX:
    return static_cast<uint16_t>(round(hp->getRoomTemperature() * 10));
    break;
  case HOLDING_REG_OPERATING_INDEX:
    return static_cast<uint16_t>(hp->getOperating() ? UINT16_C(1) : UINT16_C(0));
    break;
  case HOLDING_REG_MILLIS_SINCE_LAST_COMMS_INDEX:
    return millis() - prevHeatpumpComms;
    break;
  default:
    return -1;
    break;
  }
  return -1;
}

// Callback function to read corresponding holding register
uint16_t holdingRead(TRegister *reg, uint16_t val)
{
  uint8_t address = reg->address.address;
  return getHoldingRegister(address);
}
// Callback function to holding register
uint16_t holdingWrite(TRegister *reg, uint16_t val)
{
  uint8_t address = reg->address.address;
  switch (address)
  {
  case HOLDING_REG_TEMPERATURE_INDEX:
    DEBUG_PRINT("Temperature will be set to ");
    DEBUG_PRINT(val);
    DEBUG_PRINTLN("/10");
    hp->setTemperature(float(static_cast<int16_t>(val)) / 10);
    break;
  case HOLDING_REG_POWER_INDEX:
    DEBUG_PRINT("Set power to ");
    DEBUG_PRINTLN(val);
    hp->setPowerSetting(fromIndexPower(val));
    break;
  case HOLDING_REG_MODE_INDEX:
    DEBUG_PRINT("Set mode to ");
    DEBUG_PRINTLN(val);
    hp->setModeSetting(fromIndexMode(val));
    break;
  case HOLDING_REG_FAN_INDEX:
    DEBUG_PRINT("Set fan to ");
    DEBUG_PRINTLN(val);
    hp->setFanSpeed(fromIndexFan(val));
    break;
  case HOLDING_REG_VANE_INDEX:
    DEBUG_PRINT("Set vane to ");
    DEBUG_PRINTLN(val);
    hp->setVaneSetting(fromIndexVane(val));
    break;
  case HOLDING_REG_WIDEVANE_INDEX:
    DEBUG_PRINT("Set wide vane to ");
    DEBUG_PRINTLN(val);
    hp->setWideVaneSetting(fromIndexWideVane(val));
    break;
  case HOLDING_REG_CONNECTED_INDEX:
  case HOLDING_REG_ROOM_TEMPERATURE_INDEX:
  case HOLDING_REG_OPERATING_INDEX:
  case HOLDING_REG_MILLIS_SINCE_LAST_COMMS_INDEX:
  case HOLDING_REG_TIMEOUT_COUNTER:
    DEBUG_PRINTLN("Client tried to write RO field. Ignoring.");
    // Read-only
    return -2;
    break;
  default:
    DEBUG_PRINT("Client tried to write unknown address");
    DEBUG_PRINT(address);
    DEBUG_PRINTLN(". Ignoring.");
    return -1;
    break;
  }
  return val;
}

std::array<uint16_t, HOLDING_WRITE_COUNT> getHoldingRegistersToWrite()
{
  std::array<uint16_t, HOLDING_WRITE_COUNT> data;
  for (int i = 0; i < HOLDING_WRITE_COUNT; i++)
  {
    data[i] = getHoldingRegister(i + HOLDING_READ_COUNT);
  }
  return data;
}

void handleHttpHvac()
{
  DEBUG_PRINTLN("handleHttp hvac info");
  DEBUG_PRINTLN("Printing hvac page");
  if (!EspHtmlTemplateProcessor(httpServer.get()).processAndSend("/web_ui.html", [](const String &var) -> String {
        heatpumpSettings settings = hp->getSettings();
        settings = updateHeatpumpFromHttpQueryParameters(httpServer, hp, settings);
        return template_html(prevHeatpumpComms, hp, settings, var);
      }))
  {
    DEBUG_PRINTLN("ERROR templating page. Have you uploaded 'File System image' / SPIFFS which includes the web_ui.html? Listing files");
    if (!SPIFFS.begin())
    {
      DEBUG_PRINTLN(" ERROR: An Error has occurred while mounting SPIFFS");
      return;
    }
#ifdef ESP8266
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
#elif defined(ESP32)
    File dir = SPIFFS.open("/");
    while (dir.openNextFile())
#endif
    {
      DEBUG_PRINT(" SPIFFS: ");
#ifdef ESP8266
      DEBUG_PRINTLN(dir.fileName());
#elif defined(ESP32)
      DEBUG_PRINT(dir.name());
#endif
    }
    DEBUG_PRINTLN(" SPIFFS direcotry listing completed.");
  }
}

void handleHttpNotFound()
{
  httpServer->send(404, "text/plain", "404 Not Found");
}

void modbusSetup()
{
  DEBUG_SCOPE("Modbus");

  if (MODBUS_SERVER_ENABLED)
  {
    mb->server();

    mb->addCoil(0, COIL_VAL(false), COILS_LEN);
    mb->onGetCoil(0, coilRead, COILS_LEN);
    mb->onSetCoil(0, coilWrite, COILS_LEN);

    mb->addHreg(0, 0, HOLDING_LEN);
    mb->onGetHreg(0, holdingRead, HOLDING_LEN);
    mb->onSetHreg(0, holdingWrite, HOLDING_LEN);
  }
  if (MODBUS_CLIENT_ENABLED)
  {
    mb->client();
  }
}

void arduinoOTASetup()
{
  DEBUG_SCOPE("ArduinoOTA");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.onStart([]() {
    DEBUG_PRINTLN("OTA: Start");
    otaInProgress = true;
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_PRINTLN("\nOTA: End");
    otaInProgress = false;
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINTLN("OTA: Progress: " + String(progress / (total / 100)) + "%");
    otaInProgress = true;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR)
    {
      DEBUG_PRINTLN("OTA: Auth Failed (" + String(error) + ")");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      DEBUG_PRINTLN("OTA: Begin Failed (" + String(error) + ")");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      DEBUG_PRINTLN("OTA: Connect Failed (" + String(error) + ")");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      DEBUG_PRINTLN("OTA: Receive Failed (" + String(error) + ")");
    }
    else if (error == OTA_END_ERROR)
    {
      DEBUG_PRINTLN("OTA: End Failed (" + String(error) + ")");
    }
    else
    {
      DEBUG_PRINTLN("OTA: Unknown Failed (" + String(error) + ")");
    }
    otaInProgress = false;
  });

  ArduinoOTA.begin();
}

void connectWifiOrRestart(bool setup)
{
  if (setup)
  {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  prevWifiConnected = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    DEBUG_PRINTLN("Connecting...");
    if (millis() - prevWifiConnected > WIFI_RETRY_MILLIS)
    {
      DEBUG_PRINTLN("Wifi seems to be down. Shuttinng down heat pump and restarting ESP");
      bool heatPumpConnected = hp->isConnected();
      if (!heatPumpConnected)
      {
        heatPumpConnected = hp->connect(&heatpumpSerial);
      }
      bool updated = false;
      if (heatPumpConnected)
      {
        hp->setPowerSetting(false);
        updated = hp->update();
      }
      DEBUG_PRINTLN("Managed to shutdown the pump: " + String(heatPumpConnected && updated) + " (connected " + String(heatPumpConnected) + ")");
      ESP.restart();
      while (true)
      {
        // wait for reboot
        delay(1000);
      }
    }
  }
  // Wifi is connected
  prevWifiConnected = millis();
}

void setup()
{
  WiFi.mode(WIFI_STA);

#if defined(SERIAL_FREE_FOR_PRINT)
  Serial.begin(74880);
#endif
  // Listen for remote controller updates ("external" updates)
  // update internal state of 'hp'
  {
    DEBUG_SCOPE("hp init");
    hp->enableAutoUpdate();
    hp->enableExternalUpdate();
  }

  connectWifiOrRestart(true);

  arduinoOTASetup();
  // Start local webserver
  if (HTTP_SERVER_ENABLED)
  {
    DEBUG_PRINT("Starting HTTP Server ");
    DEBUG_PRINTLN(WiFi.localIP().toString());
    httpServer.reset(new WebServer(WiFi.localIP(), 80));
    httpServer->on("/", handleHttpHvac);
    httpServer->onNotFound(handleHttpNotFound);
    httpServer->begin();
  }
  else
  {
    DEBUG_PRINTLN("HTTP Server disabled.");
  }

  modbusSetup();
}

void maybeReconnectModbus()
{
  if (!mb->isConnected(REMOTE_MODBUS_IP))
  {
    bool connected = mb->connect(REMOTE_MODBUS_IP, REMOTE_MODBUS_PORT);
    DEBUG_PRINTLN("Modbus client not connected. Trying to connect... Success: " + String(connected));
  }
}

boolean modbusRead()
{
  bool readSuccess = false;
  for (int i = 0; i < MODBUS_RETRIES; i++)
  {
    maybeReconnectModbus();
    readSuccess = mb->readHreg(REMOTE_MODBUS_IP, 0, holdingDataRead.begin(), holdingDataRead.size(), nullptr, REMOTE_MODBUS_UNIT_ID);
    if (readSuccess)
    {
      prevModbusRead = millis();
      break;
    }
    delay(MODBUS_RETRY_SLEEP);
  }
  return readSuccess;
}

boolean modbusWrite()
{
  holdingDataWrite = getHoldingRegistersToWrite();
  bool writeSuccess = false;
  for (int i = 0; i < MODBUS_RETRIES; i++)
  {
    maybeReconnectModbus();
    writeSuccess = mb->writeHreg(REMOTE_MODBUS_IP, /* offset */ HOLDING_READ_COUNT, holdingDataWrite.begin(), holdingDataWrite.size(), nullptr, REMOTE_MODBUS_UNIT_ID);
    if (writeSuccess)
    {
      String dataWritten = "Modbus data written: ";
      for (auto it = holdingDataWrite.begin(), end = holdingDataWrite.end(); it != end; it++)
      {
        dataWritten += String(*it);
        if (it + 1 != end)
        {
          dataWritten += ", ";
        }
      }
      DEBUG_PRINTLN(dataWritten);
      prevModbusWrite = millis();
      return true;
    }
    else
    {
      mb->disconnect(REMOTE_MODBUS_IP);
    }
    delay(MODBUS_RETRY_SLEEP);
  }
  return false;
}

void modbusLoop()
{
  if (MODBUS_CLIENT_ENABLED)
  {
    yield();
    maybeReconnectModbus();
    //
    // have a chance to progress on the background with other modbus activities
    //
    yield();
    mb->task();
    yield();

    //
    // read commands
    //
    if (mb->isConnected(REMOTE_MODBUS_IP) && millis() - prevModbusRead > REMOTE_MODBUS_READ_INTERVAL_MILLIS)
    {
      bool prevPowerOn = lastCommandPower;
      bool readSuccess = modbusRead();
      bool newCommand = holdingDataRead[0] != 0;
      if (readSuccess)
      {
        if (prevModbusRead == 0 || lastCommandPower != newCommand)
        {
          hvacCommandsPending = true;
        }
        lastCommandPower = newCommand;
      }
      else
      {
        mb->disconnect(REMOTE_MODBUS_IP);
      }
      DEBUG_PRINTLN_THROTTLED(2, "Modbus client connected. Reading registers, success: " + String(readSuccess) + String(". Last command power=") + String(newCommand) //
                                     + String(", before that ") + String(prevPowerOn)                                                                                 //
                                     + String(", pending write: ") + String(hvacCommandsPending));
    }
    else
    {
      DEBUG_PRINTLN_THROTTLED(6, "(read skipped)");
    }
    //
    // have a chance to progress on the background with other modbus activities
    //
    yield();
    mb->task();
    yield();
    //
    // write current state
    //
    if (mb->isConnected(REMOTE_MODBUS_IP) && millis() - prevModbusWrite > REMOTE_MODBUS_WRITE_INTERVAL_MILLIS)
    {
      bool writeSuccess = modbusWrite();
      DEBUG_PRINT_THROTTLED(3, "Modbus client connected. Wrote registers, success: " + String(writeSuccess));
    }
    else
    {
      DEBUG_PRINTLN_THROTTLED(4, "(write skipped)");
    }
  }
  // TODO: simplify above (NOTE ONE BUG FOUND ALREADY)
  //
  // have a chance to progress on the background with other modbus activities
  //
  yield();
  mb->task();
  yield();
}

void loop()
{
  DEBUG_PRINT_THROTTLED(0, "loop, uptime in secs: " + String(float(millis() / 1000.)));
  connectWifiOrRestart(false);
  yield();
  if (!otaInProgress && WiFi.status() != WL_CONNECTED)
  {
    DEBUG_PRINT("Wifi not connected, restarting!");
    ESP.restart();
    while (true)
    {
      // wait for reboot
      delay(1000);
    }
  }
  modbusLoop();
  yield();
  ArduinoOTA.handle();
  yield();
  if (httpServer)
  {
    httpServer->handleClient();
  }
  yield();
#ifdef DEBUG
  DEBUG_PRINTLN_THROTTLED(5, "In debug mode, not syncing/connecting heat pump");
#else
  if (!hp->isConnected())
  {
    hp->connect(&heatpumpSerial);
  }
  yield();
  bool updated = false;
  if (hp->isConnected())
  {
    bool upToDatePowerCommandAvailable = (prevModbusRead != 0) && (millis() - prevModbusRead < 60000);
    if (hvacCommandsPending && upToDatePowerCommandAvailable)
    {
      DEBUG_PRINTLN("Setting power to " + String(lastCommandPower));
      hp->setPowerSetting(lastCommandPower);
    }
    updated = hp->update();
  }
#endif
  yield();
  if (updated)
  {
    prevHeatpumpComms = millis();
    bool powerOnCurrently = hp->getPowerSettingBool();
    if (powerOnCurrently == lastCommandPower)
    {
      if (hvacCommandsPending)
      {
        DEBUG_PRINTLN("Power successfully set to " + String(lastCommandPower) +
                      ". Reseting hvacCommandsPending flag");
      }
      // HVAC state is synchronized to target state
      hvacCommandsPending = false;
    }
  }
  else
  {
    DEBUG_PRINTLN("Failed to update() heatpump");
  }
}