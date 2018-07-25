#ifndef __WEBSOCKETCLIENT_UTILITY_DOT_H_INCLUDED_
#define __WEBSOCKETCLIENT_UTILITY_DOT_H_INCLUDED_

#include "config.h"
#include <Arduino.h>

void generateSecKey(char output[]);
void encodeSecKey(char output[], const char *key);

void printf(const __FlashStringHelper *fmt, ...);

#ifdef _DEBUG
# define __debugOutput printf
#else
#define __debugOutput(...)
#endif

#endif