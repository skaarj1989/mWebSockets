#ifndef __WEBSOCKETS_CONFIG_DOT_H_INCLUDED_
#define __WEBSOCKETS_CONFIG_DOT_H_INCLUDED_

//
// Serial output:
//

// Enables __debugOutput function
#define _DEBUG
// Handshake request/response on Serial output
#define _DUMP_HANDSHAKE
// Frame header on Serial output
#define _DUMP_HEADER
// Frame data on Serial output
#define _DUMP_FRAME_DATA

//
//
//

#define TIMEOUT_INTERVAL	5000
#define BUFFER_MAX_SIZE		128

// 
// ETHERNET_CONTROLLER_W5100
// ETHERNET_CONTROLLER_W5500
// ETHERNET_CONTROLLER_ENC28J60
// NETWORK_CONTROLLER_WIFI

#define NETWORK_CONTROLLER	ETHERNET_CONTROLLER_W5100

#endif // __WEBSOCKETS_CONFIG_DOT_H_INCLUDED_