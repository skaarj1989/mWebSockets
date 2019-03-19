#include "utility.h"

#include <Arduino.h>

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
# include <cstdarg>
#endif

#include "Base64.h"
#include <SHA1.h>

#define MAGIC_STRING	"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

namespace net {

SHA1 sha1;

void generateSecKey(char output[]) {
	char temp[17] = { '\0' };
	
	randomSeed(analogRead(0));
	for (byte i = 0; i < 16; ++i)
		temp[i] = static_cast<char>(random(0xFF));
	
	base64_encode(output, temp, 16);
}

void encodeSecKey(char output[], const char *key) {
	char buffer[64] = { '\0' };

	strcpy(buffer, key);
	strcat(buffer, MAGIC_STRING);
	
	sha1.reset();
	sha1.update(buffer, strlen(buffer));
	
	sha1.finalize(buffer, 20);
	base64_encode(output, buffer, 20);

	sha1.clear();
}

};

void printf(const __FlashStringHelper *fmt, ... ){
  char buffer[128];
  va_list args;
  va_start (args, fmt);

#if (PLATFORM_ARCH == PLATFORM_ARCHITECTURE_AVR) || (PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266)
	vsnprintf_P(buffer, sizeof(buffer), (const char *)fmt, args);
#else
	vsnprintf(buffer, sizeof(buffer), (const char *)fmt, args);
#endif	

  va_end(args);

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
	SerialUSB.print(buffer);
#else
  Serial.print(buffer);
#endif
}