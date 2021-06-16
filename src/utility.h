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

IPAddress fetchRemoteIp(const NetClient &);

} // namespace net
