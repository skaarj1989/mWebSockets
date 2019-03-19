#ifndef __WEBSOCKETSERVER_DOT_H_INCLUDED_
#define __WEBSOCKETSERVER_DOT_H_INCLUDED_

#include "WebSocket.h"

class WebSocketServer {
public:
	WebSocketServer(uint16_t port);
	~WebSocketServer();
	
	void begin();
	void shutdown();
	
	void listen();
	void broadcast(const WebSocketDataType dataType, const char *message, uint16_t length);
	
	uint8_t countClients();
	
	// ---
	
	void setVerifyClientCallback(verifyClientCallback *callback);
	
	void setOnOpenCallback(onOpenCallback *callback);
	void setOnCloseCallback(onCloseCallback *callback);
	void setOnMessageCallback(onMessageCallback *callback);
	void setOnErrorCallback(onErrorCallback *callback);
	
private:
	WebSocket *_getWebSocket(NetClient client);
	bool _handleRequest(NetClient &client);
	void _rejectRequest(NetClient &client, const WebSocketError code);
	
	void _heartbeat();
	
	void _triggerError(const WebSocketError code);
	
private:
	NetServer server_;
	WebSocket *sockets_[MAX_CONNECTIONS];
	
	verifyClientCallback *verifyClient_;

	onOpenCallback *onOpen_;
	onCloseCallback *onClose_;
	onMessageCallback *onMessage_;
	onErrorCallback *onError_;
};

#endif // __WEBSOCKETSERVER_DOT_H_INCLUDED_