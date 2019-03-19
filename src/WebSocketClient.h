#ifndef __WEBSOCKETCLIENT_DOT_H_INCLUDED_
#define __WEBSOCKETCLIENT_DOT_H_INCLUDED_

#include "WebSocket.h"

class WebSocketClient : public WebSocket {
public:
	WebSocketClient();
	~WebSocketClient();
	
	bool open(const char *host, uint16_t port = 3000, const char *path = "/");
	void listen();

	// ---
	
	void setOnOpenCallback(onOpenCallback *callback);
	void setOnCloseCallback(onCloseCallback *callback);
	void setOnMessageCallback(onMessageCallback *callback);
	void setOnErrorCallback(onErrorCallback *callback);
};

#endif // __WEBSOCKETCLIENT_DOT_H_INCLUDED_