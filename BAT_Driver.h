#pragma once
#include <Arduino.h> 

#define BAT_ADC_PIN   1
#define Measurement_offset 0.992857

extern float BAT_analogVolts;

void BAT_Init(void);
float BAT_Get_Volts(void);
void BAT_Service(uint32_t now_ms);
uint8_t BAT_Get_Percent(void);
bool BAT_Has_Reading(void);
bool BAT_Is_Low(void);
bool BAT_Is_Critical(void);
