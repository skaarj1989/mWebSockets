#include "utility.h"
#include "Base64.h"
#include "SHA1.h"
#include <stdarg.h>

void printf(const __FlashStringHelper *fmt, ...) {
  char buffer[192]{};
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
  constexpr char kMagicString[]{ "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" };

  char buffer[64]{};
  strcpy(buffer, key);
  strcat(buffer, kMagicString);

  SHA1 sha1;
  sha1.reset();
  sha1.update(buffer, strlen(buffer));

  sha1.finalize(buffer, 20);
  base64_encode(output, buffer, 20);
}

void generateMask(char *output) {
  randomSeed(analogRead(0));

  for (byte i = 0; i < 4; ++i)
    output[i] = static_cast<char>(random(0xFF));
}

// https://github.com/websockets/utf-8-validate/blob/master/src/validation.c
bool isValidUTF8(const byte *s, size_t length) {
  const uint8_t *end = s + length;

  //
  // This code has been taken from utf8_check.c which was developed by
  // Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>.
  //
  // For original code / licensing please refer to
  // https://www.cl.cam.ac.uk/%7Emgk25/ucs/utf8_check.c
  //
  while (s < end) {
    if (*s < 0x80) { // 0xxxxxxx
      s++;
    } else if ((s[0] & 0xe0) == 0xc0) { // 110xxxxx 10xxxxxx
      if (s + 1 == end || (s[1] & 0xc0) != 0x80 ||
          (s[0] & 0xfe) == 0xc0 // overlong
      ) {
        break;
      } else {
        s += 2;
      }
    } else if ((s[0] & 0xf0) == 0xe0) { // 1110xxxx 10xxxxxx 10xxxxxx
      if (s + 2 >= end || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
          (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||
          (s[0] == 0xed && (s[1] & 0xe0) == 0xa0)) {
        break;
      } else {
        s += 3;
      }
    } else if ((s[0] & 0xf8) == 0xf0) { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      if (s + 3 >= end || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
          (s[3] & 0xc0) != 0x80 ||
          (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||   // overlong
          (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4 // > U+10FFFF
      ) {
        break;
      } else {
        s += 4;
      }
    } else {
      break;
    }
  }

  return s == end;
}

} // namespace net