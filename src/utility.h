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

void printf(const __FlashStringHelper *fmt, ...);

namespace net {

void generateSecKey(char output[]);
void encodeSecKey(char output[], const char *key);
void generateMask(byte *output);

} // namespace net

#ifdef _DEBUG
#  define __debugOutput printf
#else
#  define __debugOutput(...)
#endif

