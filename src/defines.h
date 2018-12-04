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

enum eWebSocketReadyState {
	WSRS_CONNECTING,
	WSRS_OPEN,
	WSRS_CLOSING,
	WSRS_CLOSED
};

enum eWebSocketCloseEvent {
	NONE = 0,
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