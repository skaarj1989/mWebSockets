#pragma once

/** @file */

#include "Platform.hpp"

/**
 * @def _DEBUG Enables __debugOutput function.
 * @def _DUMP_HANDSHAKE Prints any handshake request/response on Serial output.
 * @def _DUMP_HEADER Prints frame header on Serial output.
 * @def _DUMP_FRAME_DATA Prints frame data on Serial output.
 */

#define _DEBUG
#define _DUMP_HANDSHAKE
//#define _DUMP_HEADER
//#define _DUMP_FRAME_DATA

/** Maximum size of data buffer - frame payload (in bytes). */
constexpr uint16_t kBufferMaxSize{128};
/** Maximum time to wait for endpoint response (in milliseconds). */
constexpr uint16_t kTimeoutInterval{5000};
