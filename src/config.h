#pragma once

/// Enables __debugOutput function
#define _DEBUG
/// Handshake request/response on Serial output
#define _DUMP_HANDSHAKE
/// Frame header on Serial output
#define _DUMP_HEADER
/// Frame data on Serial output
#define _DUMP_FRAME_DATA

//
//
//

/** Maximum size of data buffer. */
constexpr uint16_t kBufferMaxSize = 256;
/** Maximum time to wait for endpoint response (in milliseconds). */
constexpr uint16_t kTimeoutInterval = 5000;

// ETHERNET_CONTROLLER_W5100
// ETHERNET_CONTROLLER_W5500
// ETHERNET_CONTROLLER_ENC28J60
// NETWORK_CONTROLLER_WIFI
#define NETWORK_CONTROLLER ETHERNET_CONTROLLER_W5100