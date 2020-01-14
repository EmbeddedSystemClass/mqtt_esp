#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H

#include "freertos/FreeRTOS.h"

#define BIT_THERMOSTAT 1
#define BIT_WATER_SENSOR (1 << 1)
#define BIT_ROOM_SENSOR (1 << 2)

enum ThermostatMode {
  TERMOSTAT_MODE_OFF = BIT_THERMOSTAT, //1
  TERMOSTAT_MODE_SUMMER = BIT_WATER_SENSOR | BIT_THERMOSTAT, //3
  TERMOSTAT_MODE_SPRING_AUTUMN = BIT_WATER_SENSOR | BIT_ROOM_SENSOR | BIT_THERMOSTAT, //7
  TERMOSTAT_MODE_WINTER = BIT_ROOM_SENSOR | BIT_THERMOSTAT, //5
};

enum HoldOffMode {
  HOLD_OFF_DISABLED = 1,
  HOLD_OFF_ENABLED,
};
enum ThermostatState {
  THERMOSTAT_STATE_ON = 1,
  THERMOSTAT_STATE_OFF,

};
struct ThermostatCmdMessage {
  enum HoldOffMode holdOffMode;
  enum ThermostatState thermostatState;
};

struct TemperatureCmdMessage {
  short temperature;
};

struct ThermostatCfgMessage {
  short circuitTargetTemperature;
  short waterTargetTemperature;
  short waterTemperatureSensibility;
  short room0TargetTemperature;
  short room0TemperatureSensibility;
  enum ThermostatMode thermostatMode;
};

struct ThermostatSensorsMessage {
  short wtemperature;
  short ctemperature;
};

struct ThermostatRoomMessage {
  short temperature;
};

union ThermostatData {
  struct ThermostatCfgMessage cfgData;
  struct ThermostatCmdMessage cmdData;
  struct ThermostatSensorsMessage sensorsData;
  struct ThermostatRoomMessage roomData;
};

#define THERMOSTAT_CMD_MSG 1
#define THERMOSTAT_CFG_MSG 2
#define THERMOSTAT_SENSORS_MSG 3
#define THERMOSTAT_ROOM_0_MSG 4
#define THERMOSTAT_LIFE_TICK 5

struct ThermostatMessage {
  unsigned char msgType;
  union ThermostatData data;
};

void publish_thermostat_data();

esp_err_t read_thermostat_nvs(const char * tag, int * value);

void handle_thermostat_cmd_task(void* pvParameters);
void read_nvs_thermostat_data(void);

#define SENSOR_LIFETIME 10

#endif /* APP_THERMOSTAT_H */
