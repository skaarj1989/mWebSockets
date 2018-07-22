#ifndef __WEBSOCKETCLIENT_DOT_H_INCLUDED_
#define __WEBSOCKETCLIENT_DOT_H_INCLUDED_

/*
	https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
	https://tools.ietf.org/id/draft-ietf-hybi-thewebsocketprotocol-09.html
	https://tools.ietf.org/html/rfc6455
	https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/readyState
	https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
*/

#include "utility.h"

#if ETHERNET_CONTROLLER == W5100
# include <Ethernet.h>
#elif ETHERNET_CONTROLLER == W5500
# include <Ethernet2.h>
#elif ETHERNET_CONTROLLER == ENC28J60
#else
# error "Unknown controller"
#endif

enum eWebSocketReadyState {
	CONNECTING,
	OPEN,
	CLOSING,
	CLOSED
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
	CONTINUATION_FRAME 			= 0x00,
	TEXT_FRAME 							= 0x01,
	BINARY_FRAME 						= 0x02,
	CONNECTION_CLOSE_FRAME	= 0x08,
	PING_FRAME							= 0x09,
	PONG_FRAME							= 0x0A
};

enum eWebSocketDataType {
	TEXT,
	BINARY
};

enum eWebSocketError {
	CONNECTION_ERROR,
	CONNECTION_TIMED_OUT,
	BAD_REQUEST,
	UPGRADE_REQUIRED,
	FRAME_LENGTH_EXCEEDED,
	UNKNOWN_OPCODE,
	UNSUPPORTED_OPCODE
};

struct frame_t {
	bool isMasked;
	bool isFinal;
	eWebSocketOpcode opcode;
	byte mask[4] = { 0x00 };
	uint16_t length;
	char *data = NULL;
};

class WebSocketClient {
public:
	WebSocketClient();
	~WebSocketClient();
	
	bool open(const char *host, uint16_t port = 80, char path[] = "/");
	void close(const eWebSocketCloseEvent code = NORMAL_CLOSURE, const char *reason = NULL, uint16_t length = 0);
	bool isOpen();
	
	void listen();
	
	void send(const eWebSocketDataType dataType, const char *message, uint16_t length, bool mask = true);
	void ping();

	// ---
	
	typedef void onOpenCallback(WebSocketClient &ws);
	typedef void onCloseCallback(WebSocketClient &ws, const eWebSocketCloseEvent code, const char *reason, uint16_t length);
	typedef void onMessageCallback(WebSocketClient &ws, const eWebSocketDataType dataType, const char *message, uint16_t length);
	typedef void onErrorCallback(WebSocketClient &ws, const eWebSocketError code);
	
	void setOnOpenCallback(onOpenCallback *callback);
	void setOnCloseCallback(onCloseCallback *callback);
	void setOnMessageCallback(onMessageCallback *callback);
	void setOnErrorCallback(onErrorCallback *callback);
	
private:
	bool _poll(uint16_t maxAttempts, uint8_t time = 1);

	void _send(const eWebSocketOpcode opcode, const char *data, uint16_t length);
	void _send(const eWebSocketOpcode opcode, const char *data, uint16_t length, byte maskingKey[]);
	
	frame_t *_getFrame();
	
	void _terminate();	// abnormal closure (code 1006)
	void _triggerError(eWebSocketError code);
	
private:
	EthernetClient m_Client;
	eWebSocketReadyState m_eReadyState;
	
	uint8_t m_NumPings;
	
	onOpenCallback *_onOpen;
	onCloseCallback *_onClose;
	onMessageCallback *_onMessage;
	onErrorCallback *_onError;
};

#endif