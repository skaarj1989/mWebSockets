#ifndef __WEBSOCKET_DOT_H_INCLUDED_
#define __WEBSOCKET_DOT_H_INCLUDED_

#include "defines.h"

class WebSocket {
public:
	friend class WebSocketServer;
	
public:
	WebSocket();	// default constructor for client init
	~WebSocket();
	
	void close(const eWebSocketCloseEvent code = NORMAL_CLOSURE, const char *reason = NULL, uint16_t length = 0, bool instant = false);
	void terminate();
	
	void send(const eWebSocketDataType dataType, const char *message, uint16_t length);
	void send(const eWebSocketDataType dataType, const char *message, uint16_t length, bool mask);
	void ping();
	
	const eWebSocketReadyState &getReadyState() const;
	
protected:
	WebSocket(EthernetClient client);		// private constructor for server init

	bool _readHeader(webSocketHeader_t &header);
	void _readData(const webSocketHeader_t &header, char *payload);
	void _handleFrame();
	
	void _send(uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length);
	
	void _triggerError(const eWebSocketError code);
	
protected:
	EthernetClient m_Client;
	eWebSocketReadyState m_eReadyState;
	
	uint8_t m_NumPings;
	char m_DataBuffer[BUFFER_MAX_SIZE];
	
	bool m_bMaskEnabled;
	
	onOpenCallback *_onOpen;
	onCloseCallback *_onClose;
	onMessageCallback *_onMessage;
	onErrorCallback *_onError;
};

#endif