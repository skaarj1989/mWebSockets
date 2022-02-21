#include "Utility.hpp"
#include "Platform.hpp"
#include <Arduino.h>
#include <stdarg.h> // va_*

void printf(const __FlashStringHelper *fmt, ...) {
  char buffer[256]{};
  va_list args;
  va_start(args, fmt);
#if (PLATFORM_ARCH == PLATFORM_ARCHITECTURE_AVR) ||                            \
    (PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266)
  vsnprintf_P(
      buffer, sizeof(buffer), reinterpret_cast<const char *>(fmt), args);
#else
  vsnprintf(buffer, sizeof(buffer), reinterpret_cast<const char *>(fmt), args);
#endif
  va_end(args);

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
  SerialUSB.print(buffer);
#else
  Serial.print(buffer);
#endif
}
