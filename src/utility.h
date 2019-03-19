#ifndef __WEBSOCKETS_UTILITY_DOT_H_INCLUDED_
#define __WEBSOCKETS_UTILITY_DOT_H_INCLUDED_

#include "platform.h"
#include <Arduino.h>

namespace net {
	
void generateSecKey(char output[]);
void encodeSecKey(char output[], const char *key);

};

void printf(const __FlashStringHelper *fmt, ...);

#ifdef _DEBUG
# define __debugOutput printf
#else
#define __debugOutput(...)
#endif

#endif // __WEBSOCKETS_UTILITY_DOT_H_INCLUDED_