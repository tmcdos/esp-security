#ifndef CGIWIFI_H
#define CGIWIFI_H

#include "httpd.h"

enum { wifiIsDisconnected, wifiIsConnected, wifiGotIP };
typedef void(*WifiStateChangeCb)(uint8_t wifiStatus);

int cgiWiFiScan(HttpdConnData *connData);
int cgiWifiInfo(HttpdConnData *connData);
int cgiWiFi(HttpdConnData *connData);
int cgiWiFiConnect(HttpdConnData *connData);
int cgiWiFiSetMode(HttpdConnData *connData);
int cgiWiFiSetPass(HttpdConnData *connData);
int cgiWiFiConnStatus(HttpdConnData *connData);
int cgiWiFiSpecial(HttpdConnData *connData);
int cgiApSettingsChange(HttpdConnData *connData);
int cgiApSettingsInfo(HttpdConnData *connData);
void configWifiIP();
void wifiInit(void);
void wifiAddStateChangeCb(WifiStateChangeCb cb);
int checkString(char *str);

extern uint8_t wifiState;

#endif
