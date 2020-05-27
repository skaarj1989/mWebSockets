#include "utility.h"
#include <Base64.h>
#include <SHA1.h>

void printf(const __FlashStringHelper *fmt, ...) {
  char buffer[128]{};
  va_list args;
  va_start(args, fmt);

#if (PLATFORM_ARCH == PLATFORM_ARCHITECTURE_AVR) ||                            \
  (PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266)
  vsnprintf_P(buffer, sizeof(buffer), (const char *)fmt, args);
#else
  vsnprintf(buffer, sizeof(buffer), (const char *)fmt, args);
#endif

  va_end(args);

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
  // MSVC with VisualMicro complains about SerialUSB even if above condition is
  // false (just comment below line in case of problems)
  SerialUSB.print(buffer);
#else
  Serial.print(buffer);
#endif
}

namespace net {

void generateSecKey(char output[]) {
  char temp[17]{};

  randomSeed(analogRead(0));
  for (byte i = 0; i < 16; ++i)
    temp[i] = static_cast<char>(random(0xFF));

  base64_encode(output, temp, 16);
}

void encodeSecKey(char output[], const char *key) {
  constexpr auto kMagicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  char buffer[64]{};
  strcpy(buffer, key);
  strcat(buffer, kMagicString);

  SHA1 sha1;
  sha1.reset();
  sha1.update(buffer, strlen(buffer));

  sha1.finalize(buffer, 20);
  base64_encode(output, buffer, 20);
}

void generateMask(byte *output) {
  randomSeed(analogRead(0));

  for (uint8_t i = 0; i < 4; i++)
    output[i] = random(0xFF);
}

} // namespace net