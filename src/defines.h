/*
	https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
	https://tools.ietf.org/id/draft-ietf-hybi-thewebsocketprotocol-09.html
	https://tools.ietf.org/html/rfc6455
	https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/readyState
	https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
*/

#ifndef __WEBSOCKETS_DEFINES_DOT_H_INCLUDED_
#define __WEBSOCKETS_DEFINES_DOT_H_INCLUDED_

#include "platform.h"

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
# define NetClient WiFiClient
# define NetServer WiFiServer
#else
# define NetClient EthernetClient
# define NetServer EthernetServer
#endif

#define VALID_UPGRADE_HEADER			0x01
#define VALID_CONNECTION_HEADER		0x02
#define VALID_SEC_KEY							0x04
#define VALID_VERSION							0x08

namespace net {

enum WebSocketReadyState {
	CONNECTING = 0,
	OPEN,
	CLOSING,
	CLOSED
};

enum WebSocketOpcode {
	CONTINUATION_FRAME = 0x00,
	
	//
	// Data Frames (non-control):
	//
	
	TEXT_FRAME = 0x01,
	BINARY_FRAME = 0x02,
	
	// Reserved non-control frames:
	// 0x03
	// 0x04
	// 0x05
	// 0x06
	// 0x07
	
	//
	// Control frames:
	//
	
	CONNECTION_CLOSE_FRAME = 0x08,
	PING_FRAME = 0x09,
	PONG_FRAME = 0x0A
	
	// Reserved control opcodes:
	// 0x0B
	// 0x0C
	// 0x0D
	// 0x0E
	// 0x0F
};

enum WebSocketDataType {
	TEXT,
	BINARY
};

enum WebSocketCloseCode {
	// 0-999 : Reserved and not used.
	
	NORMAL_CLOSURE = 1000,
	GOING_AWAY,
	PROTOCOL_ERROR,
	UNSUPPORTED_DATA,
	
	// 1004 : Reserved.
	
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
	
	// 1016-1999 : Reserved for future use by the WebSocket standard.
	// 3000-3999 : Available for use by libraries and frameworks.
	// 4000-4999 : Available for use by applications.
};

enum WebSocketError {
	CONNECTION_ERROR,	
	CONNECTION_REFUSED,
	
	//
	// Client errors:
	//
	
	BAD_REQUEST = 400,
	REQUEST_TIMEOUT = 408,
	UPGRADE_REQUIRED = 426,
	
	//
	// Server errors:
	//
	
	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	SERVICE_UNAVAILABLE = 503	
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

//
// Shared callbacks:
//

typedef void onOpenCallback(WebSocket &ws);
typedef void onCloseCallback(WebSocket &ws, const WebSocketCloseCode code, const char *reason, uint16_t length);
typedef void onMessageCallback(WebSocket &ws, const WebSocketDataType dataType, const char *message, uint16_t length);
typedef void onErrorCallback(const WebSocketError code);

//
// Server-side callbacks:
//

typedef bool verifyClientCallback(const char *header, const char *value);

};

#endif // __WEBSOCKETS_DEFINES_DOT_H_INCLUDED_