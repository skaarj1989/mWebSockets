/*
	https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
	https://tools.ietf.org/id/draft-ietf-hybi-thewebsocketprotocol-09.html
	https://tools.ietf.org/html/rfc6455
	https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/readyState
	https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
*/

#ifndef __WEBSOCKETSERVER_DEFINES_DOT_H_INCLUDED_
#define __WEBSOCKETSERVER_DEFINES_DOT_H_INCLUDED_

#include "utility.h"

#if defined(_USE_WIFI) && defined(ESP8266)
# include <ESP8266WiFi.h>
# include <WiFiClient.h>
# include <WiFiServer.h>
# define MAX_CONNECTIONS 8
#else
# if ETHERNET_CONTROLLER == W5100
#  include <Ethernet.h>
#  define MAX_CONNECTIONS	4
# elif ETHERNET_CONTROLLER == W5500
#  include <Ethernet2.h>
#  define MAX_CONNECTIONS	8
# elif ETHERNET_CONTROLLER == ENC28J60
#  include <UIPEthernet.h>
#  define MAX_CONNECTIONS	4
# else
#  error "Unknown controller"
# endif
#endif

enum eWebSocketReadyState {
	WSRS_CONNECTING,
	WSRS_OPEN,
	WSRS_CLOSING,
	WSRS_CLOSED
};

enum eWebSocketCloseEvent {
	NORMAL_CLOSURE = 1000,
	GOING_AWAY,
	PROTOCOL_ERROR,
	UNSUPPORTED_DATA,
	
	NO_STATUS_RECVD = 1005,
	ABNORMAL_CLOSURE,
	INVALID_FRAME_PAYLOAD_DATA,
	POLICY_VIOLATION,
	MESSAGE_TOO_BIG,
	MISSING_EXTENSION,
	INTERNAL_ERROR,
	SERVICE_RESTART,
	TRY_AGAIN_LATER,
	BAD_GATEWAY,
	TLS_HANDSHAKE
};

enum eWebSocketOpcode {
	CONTINUATION_FRAME = 0x00,
	TEXT_FRAME = 0x01,
	BINARY_FRAME = 0x02,
	CONNECTION_CLOSE_FRAME = 0x08,
	PING_FRAME = 0x09,
	PONG_FRAME = 0x0A
};

enum eWebSocketDataType {
	TEXT,
	BINARY
};

enum eWebSocketError {
	CONNECTION_ERROR,
	
// INVALID_HTTP_RESPONSE

// CONNECTION_HEADER_MISSING
// UPGRADE_HEADER_MISSING
// SEC_WEBSOCKET_ACCEPT_HEADER_MISSING

// CONNECTION_HEADER_INVALID_VALUE
// UPGRADE_HEADER_INVALID_VALUE
	
	CONNECTION_TIMED_OUT,
	CONNECTION_REFUSED,
	BAD_REQUEST,
	UPGRADE_REQUIRED,
	FRAME_LENGTH_EXCEEDED,
	UNKNOWN_OPCODE,
	UNSUPPORTED_OPCODE
};

struct webSocketHeader_t {
	bool fin;
	bool rsv1, rsv2, rsv3;
	uint8_t opcode;
	bool mask;
	byte maskingKey[4] = { 0x0 };
	uint32_t length;
};

class WebSocket;

typedef void onOpenCallback(WebSocket &ws);
typedef void onCloseCallback(WebSocket &ws, const eWebSocketCloseEvent code, const char *reason, uint16_t length);
typedef void onMessageCallback(WebSocket &ws, const eWebSocketDataType dataType, const char *message, uint16_t length);
typedef void onErrorCallback(const eWebSocketError code);

#endif