#pragma once

#include <Arduino.h>

void SetupPortal_Begin(void);
void SetupPortal_Handle(void);
bool SetupPortal_IsActive(void);
bool SetupPortal_ShouldReboot(void);
const char * SetupPortal_ApSsid(void);
const char * SetupPortal_ApIp(void);
