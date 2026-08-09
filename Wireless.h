#pragma once
#include "WiFi.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <time.h>

extern bool WIFI_Connection;
extern uint8_t WIFI_NUM;
extern uint8_t BLE_NUM;
extern bool Scan_finish;
extern bool TIME_Synced;
extern char WIFI_IP[32];

typedef void (*WirelessStatusCallback)(const char * line1, const char * line2, const char * line3);

void Wireless_Test2();
bool Wireless_GetLocalTime(struct tm * timeinfo);
bool Wireless_EnsureConnected();
void Wireless_ServiceConnectionState(void);
bool Wireless_StartReconnect(void);
bool Wireless_ReconnectInProgress(void);
bool Wireless_ConnectSavedWithStatus(WirelessStatusCallback callback);
int Wireless_ScanNearbyWiFi(void);
void Wireless_SyncTimeNow(void);
