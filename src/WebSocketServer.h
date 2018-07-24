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
	
	uint8_t countClients();
	
	// ---
	
	void setOnOpenCallback(onOpenCallback *callback);
	void setOnCloseCallback(onCloseCallback *callback);
	void setOnMessageCallback(onMessageCallback *callback);
	void setOnErrorCallback(onErrorCallback *callback);
	
private:
	void _heartbeat();

	WebSocket *_getWebSocket(EthernetClient client);
	
	bool _handleRequest(EthernetClient &client);
	void _rejectRequest(EthernetClient &client, const eWebSocketError code);
	
	void _triggerError(const eWebSocketError code);
	
private:
	EthernetServer m_Server;
	WebSocket *m_pSockets[MAX_CONNECTIONS];
	
	onOpenCallback *_onOpen;
	onCloseCallback *_onClose;
	onMessageCallback *_onMessage;
	onErrorCallback *_onError;
};

#endif