#pragma once

#include "platform.h"

#define SAFE_DELETE(ptr)                                                       \
  {                                                                            \
    if (ptr != nullptr) {                                                      \
      delete ptr;                                                              \
      ptr = nullptr;                                                           \
    }                                                                          \
  }

#define SAFE_DELETE_ARRAY(ptr)                                                 \
  {                                                                            \
    if (ptr != nullptr) {                                                      \
      delete[] ptr;                                                            \
      ptr = nullptr;                                                           \
    }                                                                          \
  }

#ifdef _DEBUG
#  define __debugOutput printf
#else
#  define __debugOutput(...)
#endif

void printf(const __FlashStringHelper *fmt, ...);

namespace net {

void generateSecKey(char output[]);
void encodeSecKey(char output[], const char *key);
void generateMask(char *output);

bool isValidUTF8(const byte *str, size_t length);

} // namespace net