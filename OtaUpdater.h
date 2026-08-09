#pragma once

#include <Arduino.h>

typedef struct {
  bool checked;
  bool ready;
  bool update_available;
  char version[24];
  char notes[128];
  char md5[33];
  uint32_t size;
  char error[128];
} OtaManifest;

typedef void (*OtaProgressCallback)(uint8_t percent, const char * status);

void OtaUpdater_Init(void);
bool OtaUpdater_Check(void);
bool OtaUpdater_Install(OtaProgressCallback callback);
const OtaManifest * OtaUpdater_GetManifest(void);
const char * OtaUpdater_LastError(void);
