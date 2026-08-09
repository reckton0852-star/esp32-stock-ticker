#include "BAT_Driver.h"

float BAT_analogVolts = 0;
static uint8_t battery_percent = 0;
static bool battery_has_reading = false;
static uint32_t last_battery_sample_ms = 0;
static const uint32_t BATTERY_SAMPLE_INTERVAL_MS = 60000;

typedef struct {
  float voltage;
  uint8_t percent;
} BatteryCurvePoint;

static const BatteryCurvePoint BATTERY_CURVE[] = {
  {3.40f, 0},
  {3.50f, 5},
  {3.60f, 10},
  {3.70f, 25},
  {3.80f, 45},
  {3.90f, 65},
  {4.00f, 80},
  {4.10f, 90},
  {4.20f, 100},
};

static uint8_t voltage_to_percent(float voltage)
{
  if(voltage <= BATTERY_CURVE[0].voltage) {
    return BATTERY_CURVE[0].percent;
  }

  const size_t point_count = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);
  for(size_t i = 1; i < point_count; i++) {
    if(voltage <= BATTERY_CURVE[i].voltage) {
      const BatteryCurvePoint * low = &BATTERY_CURVE[i - 1];
      const BatteryCurvePoint * high = &BATTERY_CURVE[i];
      float ratio = (voltage - low->voltage) / (high->voltage - low->voltage);
      float percent = low->percent + ratio * (high->percent - low->percent);
      return (uint8_t)(percent + 0.5f);
    }
  }

  return 100;
}

static float sample_battery_voltage(void)
{
  uint32_t millivolts = 0;
  for(uint8_t i = 0; i < 8; i++) {
    millivolts += analogReadMilliVolts(BAT_ADC_PIN);
  }

  float voltage = ((millivolts / 8.0f) * 3.0f / 1000.0f) / Measurement_offset;
  if(battery_has_reading) {
    voltage = BAT_analogVolts * 0.75f + voltage * 0.25f;
  }
  return voltage;
}

void BAT_Init(void)
{
  analogReadResolution(12);
  BAT_Get_Volts();
}

float BAT_Get_Volts(void)
{
  BAT_analogVolts = sample_battery_voltage();
  if(BAT_analogVolts < 2.80f || BAT_analogVolts > 4.50f) {
    battery_percent = 0;
    battery_has_reading = false;
    last_battery_sample_ms = millis();
    return BAT_analogVolts;
  }
  battery_percent = voltage_to_percent(BAT_analogVolts);
  battery_has_reading = true;
  last_battery_sample_ms = millis();
  return BAT_analogVolts;
}

void BAT_Service(uint32_t now_ms)
{
  if(last_battery_sample_ms == 0 || now_ms - last_battery_sample_ms >= BATTERY_SAMPLE_INTERVAL_MS) {
    BAT_Get_Volts();
  }
}

uint8_t BAT_Get_Percent(void)
{
  return battery_percent;
}

bool BAT_Has_Reading(void)
{
  return battery_has_reading;
}

bool BAT_Is_Low(void)
{
  return battery_has_reading && battery_percent < 20;
}

bool BAT_Is_Critical(void)
{
  return battery_has_reading && battery_percent < 10;
}
