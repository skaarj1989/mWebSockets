#ifndef __WEBSOCKETSERVER_DOT_H_INCLUDED_
#define __WEBSOCKETSERVER_DOT_H_INCLUDED_

#include "WebSocket.h"

namespace net {

class WebSocketServer {
public:
	WebSocketServer(uint16_t port);
	~WebSocketServer();
	
	void begin(verifyClientCallback &&callback = nullptr);
	void shutdown();
	
	uint8_t countClients();
	
	void broadcast(const WebSocketDataType dataType, const char *message, uint16_t length);
	
	void listen();

	// ---
	
	void onConnection(onConnectionCallback &&callback) { onConnection_ = callback; }
	void onError(onErrorCallback &&callback) { onError_ = callback; }
	
private:
	WebSocket *_getWebSocket(NetClient client);
	
	bool _handleRequest(NetClient &client);
	void _rejectRequest(NetClient &client, const WebSocketError code);
	
	void _triggerError(const WebSocketError code);
	
private:
	NetServer server_;
	WebSocket *sockets_[MAX_CONNECTIONS];
	
	verifyClientCallback verifyClient_;
	onConnectionCallback onConnection_;

	onErrorCallback onError_;
};

};

#endif // __WEBSOCKETSERVER_DOT_H_INCLUDED_