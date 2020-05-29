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

constexpr auto kBufferMaxSize = 256;
constexpr auto kTimeoutInterval = 5000;

// ETHERNET_CONTROLLER_W5100
// ETHERNET_CONTROLLER_W5500
// ETHERNET_CONTROLLER_ENC28J60
// NETWORK_CONTROLLER_WIFI
#define NETWORK_CONTROLLER ETHERNET_CONTROLLER_W5100