#ifndef __WEBSOCKETCLIENT_DOT_H_INCLUDED_
#define __WEBSOCKETCLIENT_DOT_H_INCLUDED_

#include <Ethernet.h>			// Ethernet Shield W5100
//#include <Ethernet2.h>		// Ethernet Shield v2 W5500

class WebSocketClient {
public:
	WebSocketClient();
	~WebSocketClient();
	
	bool open(const char *hostname, uint16_t port = 80, char path[] = "/");
	void close();
	
	bool isOpen();
	
	void send(const char *message, uint16_t length, bool mask = true);
	
	void ping();
	
	void listen();
	
	typedef void onOpenCallback(WebSocketClient &ws);
	typedef void onCloseCallback(WebSocketClient &ws);
	typedef void onMessageCallback(WebSocketClient &ws, const char *message, uint16_t length);
	
	void setOnOpenCallback(onOpenCallback *callback);
	void setOnCloseCallback(onCloseCallback *callback);
	void setOnMessageCallback(onMessageCallback *callback);
	
private:
	bool _poll(uint16_t maxAttempts, uint8_t time = 1);
	
	void _send(uint8_t opcode, const char *data, uint16_t length);
	void _send(uint8_t opcode, const char *data, uint16_t length, byte maskingKey[]);
	
private:
	EthernetClient m_Client;
	
	bool m_bConnected;
	
	onOpenCallback *_onOpen;
	onCloseCallback *_onClose;
	onMessageCallback *_onMessage;
};

#endif