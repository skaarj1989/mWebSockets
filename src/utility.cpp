#include "utility.h"

#ifdef _DEBUG
# include <SPI.h>
#endif

#include <Arduino.h>
#include <SHA1.h>

#include "Base64.h"

#define MAGIC_STRING	"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

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

void printf(const __FlashStringHelper *fmt, ... ){
  char buf[128];
  va_list args;
  va_start (args, fmt);
# ifdef __AVR__
  vsnprintf_P(buf, sizeof(buf), (const char *)fmt, args);
# else
  vsnprintf(buf, sizeof(buf), (const char *)fmt, args);
# endif
  va_end(args);
  Serial.print(buf);
}