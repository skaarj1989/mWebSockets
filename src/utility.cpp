#include "config.h"
#include "utility.h"

#include <Arduino.h>
#include <SHA1.h>

#include "Base64.h"

#define _MAGIC_STRING	"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

SHA1 sha1;

void generateSecKey(char output[]) {
	char temp[17] = { '\0' };
	
	randomSeed(analogRead(0));
	for (byte i = 0; i < 16; ++i)
		temp[i] = static_cast<char>(random(1, 256));
	
	base64_encode(output, temp, 16);
}

void encodeSecKey(char output[], const char *key) {
	char buffer[64] = { '\0' };

	strcpy(buffer, key);
	strcat(buffer, _MAGIC_STRING);
	
	sha1.reset();
	sha1.update(buffer, strlen(buffer));
	
	sha1.finalize(buffer, 20);
	base64_encode(output, buffer, 20);

	sha1.clear();
}