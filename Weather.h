#pragma once

#include <Arduino.h>

extern bool WEATHER_Loading;
extern bool WEATHER_Ready;
extern float WEATHER_TemperatureC;
extern char WEATHER_Description[32];
extern char WEATHER_UpdatedAt[24];

void Weather_RequestUpdate(void);
