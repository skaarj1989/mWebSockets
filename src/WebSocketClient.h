#ifndef __WEBSOCKETCLIENT_DOT_H_INCLUDED_
#define __WEBSOCKETCLIENT_DOT_H_INCLUDED_

#include "WebSocket.h"

namespace net {

class WebSocketClient : public WebSocket {
public:
	WebSocketClient();
	~WebSocketClient();
	
	bool open(const char *host, uint16_t port = 3000, const char *path = "/");
	void listen();
	
	void onOpen(onOpenCallback &&callback) { onOpen_ = callback; }
	
private:
	onOpenCallback onOpen_;
};

};

#endif // __WEBSOCKETCLIENT_DOT_H_INCLUDED_