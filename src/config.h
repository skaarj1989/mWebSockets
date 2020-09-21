#pragma once

/** @file */

/**
 * @def _DEBUG Enables __debugOutput function.
 * @def _DUMP_HANDSHAKE Prints any handshake request/response on Serial output.
 * @def _DUMP_HEADER Prints frame header on Serial output.
 * @def _DUMP_FRAME_DATA Prints frame data on Serial output.
*/

/**
 * @def NETWORK_CONTROLLER
 * @brief Specifies network controller, available values:
 *  - ETHERNET_CONTROLLER_W5X00
 *  - ETHERNET_CONTROLLER_ENC28J60
 *  - NETWORK_CONTROLLER_WIFI
 */

//#define _DEBUG
//#define _DUMP_HANDSHAKE
//#define _DUMP_HEADER
//#define _DUMP_FRAME_DATA

#ifndef NETWORK_CONTROLLER
#  define NETWORK_CONTROLLER ETHERNET_CONTROLLER_W5X00
#endif

/** Maximum size of data buffer - frame payload (in bytes). */
constexpr uint16_t kBufferMaxSize{ 256 };
/** Maximum time to wait for endpoint response (in milliseconds). */
constexpr uint16_t kTimeoutInterval{ 5000 };
