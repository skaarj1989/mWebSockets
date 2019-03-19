#ifndef __WEBSOCKET_DOT_H_INCLUDED_
#define __WEBSOCKET_DOT_H_INCLUDED_

#include "defines.h"

class WebSocket {
	friend class WebSocketServer;
	
public:
	WebSocket();	// default constructor for client init
	~WebSocket();
	
	void close(const WebSocketCloseCode code = NORMAL_CLOSURE, const char *reason = NULL, uint16_t length = 0, bool instant = false);
	bool terminate();
	
	void send(const WebSocketDataType dataType, const char *message, uint16_t length);
	void send(const WebSocketDataType dataType, const char *message, uint16_t length, bool mask);
	void ping();
	
	const WebSocketReadyState &getReadyState() const;
	bool isAlive();
	
protected:
	WebSocket(NetClient client); // private constructor for server init

	bool _waitForResponse(uint16_t maxAttempts, uint8_t time = 1);
	
	int _read();
	bool _read(uint8_t *buf, size_t size);
	
	bool _readHeader(webSocketHeader_t &header);
	void _readData(const webSocketHeader_t &header, char *payload);
	
	void _send(uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length);
	
	void _handleFrame();
	
	void _triggerError(const WebSocketError code);
	
protected:
	NetClient client_;

	WebSocketReadyState readyState_;
	
	char dataBuffer_[BUFFER_MAX_SIZE];
	bool maskEnabled_;
	
	onOpenCallback *onOpen_;
	onCloseCallback *onClose_;
	onMessageCallback *onMessage_;
	onErrorCallback *onError_;
};

#endif // __WEBSOCKET_DOT_H_INCLUDED_