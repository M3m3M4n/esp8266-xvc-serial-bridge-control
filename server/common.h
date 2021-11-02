#ifndef COMMON_H
#define COMMON_H

#define LOGLN(...) Serial.println(__VA_ARGS__)
#define LOG(...)   Serial.print(__VA_ARGS__)

#include <ESP8266WiFi.h>

extern bool serial_log_verbose;

#endif
