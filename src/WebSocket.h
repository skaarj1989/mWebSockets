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
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
	WebSocket(WiFiClient client);		// private constructor for server init
#else
	WebSocket(EthernetClient client);		// private constructor for server init
#endif

	bool _poll(uint16_t maxAttempts, uint8_t time = 1);
	int _read();
	void _read(uint8_t *buf, size_t size);
	
	bool _readHeader(webSocketHeader_t &header);
	void _readData(const webSocketHeader_t &header, char *payload);
	void _handleFrame();
	
	void _send(uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length);
	
	void _triggerError(const eWebSocketError code);
	
protected:
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
	WiFiClient m_Client;
#else
	EthernetClient m_Client;
#endif

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