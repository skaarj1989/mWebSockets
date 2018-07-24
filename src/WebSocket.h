#ifndef __WEBSOCKET_DOT_H_INCLUDED_
#define __WEBSOCKET_DOT_H_INCLUDED_

#include "defines.h"

bool isControlFrame(const eWebSocketOpcode opcode);

class WebSocket {
public:
	friend class WebSocketServer;
	
public:
	WebSocket();
	~WebSocket();
	
	void close(const eWebSocketCloseEvent code = NORMAL_CLOSURE, const char *reason = NULL, uint16_t length = 0, bool instant = false);
	void terminate();
	
	void send(const eWebSocketDataType dataType, const char *message, uint16_t length, bool mask);
	void ping();
	
	const eWebSocketReadyState &getReadyState() const;
	
protected:
	WebSocket(EthernetClient client);

	void _send(const eWebSocketOpcode, const char *data, uint16_t length);
	void _send(const eWebSocketOpcode, const char *data, uint16_t length, byte maskingKey[]);
	
	frame_t *_getFrame(bool maskingAllowed);	
	void _handleFrame(bool maskingAllowed);
	
	void _triggerError(const eWebSocketError code);
	
protected:
	EthernetClient m_Client;
	eWebSocketReadyState m_eReadyState;
	
	uint8_t m_NumPings;
	char m_DataBuffer[_MAX_FRAME_LENGTH];
	
	onOpenCallback *_onOpen;
	onCloseCallback *_onClose;
	onMessageCallback *_onMessage;
	onErrorCallback *_onError;
};

#endif