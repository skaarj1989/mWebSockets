#pragma once

#include "Platform.hpp"
#include <WString.h>

#ifndef SAFE_DELTE
#  define SAFE_DELETE(ptr)                                                     \
    {                                                                          \
      if (ptr != nullptr) {                                                    \
        delete ptr;                                                            \
        ptr = nullptr;                                                         \
      }                                                                        \
    }
#endif
#ifndef SAFE_DELETE_ARRAY
#  define SAFE_DELETE_ARRAY(ptr)                                               \
    {                                                                          \
      if (ptr != nullptr) {                                                    \
        delete[] ptr;                                                          \
        ptr = nullptr;                                                         \
      }                                                                        \
    }
#endif

#ifdef _DEBUG
#  define __debugOutput printf
#else
#  define __debugOutput(...)
#endif

void printf(const __FlashStringHelper *fmt, ...);
