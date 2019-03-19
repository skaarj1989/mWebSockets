#include "WebSocketServer.h"
#include "utility.h"

WebSocketServer::WebSocketServer(uint16_t port)
	: server_(port), verifyClient_(NULL),
		onOpen_(NULL), onClose_(NULL), onMessage_(NULL), onError_(NULL)
{
	for (byte i = 0; i < MAX_CONNECTIONS; i++)
		sockets_[i] = NULL;
}

WebSocketServer::~WebSocketServer() {
	shutdown();
}

void WebSocketServer::begin() {
	server_.begin();
}

void WebSocketServer::shutdown() {
	for (byte i = 0; i < MAX_CONNECTIONS; i++)
		if (sockets_[i]) {
			sockets_[i]->close(GOING_AWAY, NULL, 0, true);
			sockets_[i] = NULL;
		}
}

void WebSocketServer::listen() {
	NetClient client = server_.available();

	if (client) {
		WebSocket *socket = _getWebSocket(client);
		
		if (!socket) {

			// New client ...
			
			for (byte i = 0; i < MAX_CONNECTIONS; i++) {
				if (!sockets_[i] || sockets_[i]->getReadyState() == WebSocketReadyState::CLOSED) {
					if (sockets_[i]) {
						delete sockets_[i];
						sockets_[i] = NULL;
					}
					
					// Found free room ...
					
					if (_handleRequest(client)) {
						socket = new WebSocket(client);
						sockets_[i] = socket;
						
						// Install callbacks:
						
						socket->onOpen_ = onOpen_;
						socket->onClose_ = onClose_;
						socket->onMessage_ = onMessage_;
						socket->onError_ = onError_;
						
						if (onOpen_) socket->onOpen_(*socket);
						
						//__debugOutput(F("client placed in room = %d\n"), i);
					}
					
					break;
				}
				
				if (i == MAX_CONNECTIONS - 1)
					_rejectRequest(client, SERVICE_UNAVAILABLE);
			}
		}
		
		// ---
		
		if (socket) socket->_handleFrame();
	}
	
	_heartbeat();
}

void WebSocketServer::broadcast(const WebSocketDataType dataType, const char *message, uint16_t length) {
	for (byte i = 0; i < MAX_CONNECTIONS; i++) {
		WebSocket *socket = sockets_[i];
		if (!socket || socket->getReadyState() != WebSocketReadyState::OPEN)
			continue;
		
		socket->send(dataType, message, length);
	}
}

uint8_t WebSocketServer::countClients() {
	byte count = 0;
	
	for (byte i = 0; i < MAX_CONNECTIONS; i++)
		if (sockets_[i] && sockets_[i]->client_.connected())
			count++;
	
	return count;
}

void WebSocketServer::setVerifyClientCallback(verifyClientCallback *callback) {
	verifyClient_ = callback;
}

void WebSocketServer::setOnOpenCallback(onOpenCallback *callback) {
	onOpen_ = callback;
}

void WebSocketServer::setOnCloseCallback(onCloseCallback *callback) {
	onClose_ = callback;
}

void WebSocketServer::setOnMessageCallback(onMessageCallback *callback) {
	onMessage_ = callback;
}

void WebSocketServer::setOnErrorCallback(onErrorCallback *callback) {
	onError_ = callback;
}

//
// Private functions:
//

WebSocket *WebSocketServer::_getWebSocket(NetClient client) {
	for (byte i = 0; i < MAX_CONNECTIONS; i++) {
		WebSocket *socket = sockets_[i];
		if (socket && socket->client_ == client)
			return socket;
	}
	
	return NULL;
}

bool WebSocketServer::_handleRequest(NetClient &client) {
	uint8_t flags = 0x0;
	
	int16_t bite = -1;
	byte currentLine = 0;
	byte counter = 0;
	
	//
	// Read client request:
	//
	// [1] GET /chat HTTP/1.1
	// [2] Host: example.com:8000
	// [3] Upgrade: websocket
	// [4] Connection: Upgrade
	// [5] Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
	// [6] Sec-WebSocket-Version: 13
	// [7] 
	//
	
	char buffer[132] = { '\0' };
	char secKey[32] = { '\0' };
	
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
	while(!client.available()) {
		delay(10);
	}
#endif

	while ((bite = client.read()) != -1) {
		buffer[counter++] = bite;
		
		if (static_cast<char>(bite) == '\n') {
			uint8_t lineBreakPos = strcspn(buffer, "\r\n");
			buffer[lineBreakPos] = '\0';
		
#ifdef _DUMP_HANDSHAKE		
			printf(F("[Line #%d] %s\n"), currentLine, buffer);
#endif

			//
			// [1] GET method
			//
			
			if (currentLine == 0) {
				char *pch = strtok(buffer, " ");
				for (byte i = 0; pch != NULL; i++) {
					switch (i) {
						case 0: {
							if (strcmp_P(pch, (PGM_P)F("GET")) != 0) {
								_rejectRequest(client, BAD_REQUEST);
								return false;
							}
							
							break;
						}
						case 1: {
							break; // path doesn't matter ...
						}						
						case 2: {
							if (strcmp_P(pch, (PGM_P)F("HTTP/1.1")) != 0) {
								_rejectRequest(client, BAD_REQUEST);
								return false;
							}
							
							break;
						}
						default: {
							_rejectRequest(client, BAD_REQUEST);
							return false;
						}
					}
					
					pch = strtok(NULL, " ");
				}
			}
			else {
				if (lineBreakPos > 0) {
					char *header = strtok(buffer, ":");
					char *value = NULL;
					
					//
					// [2] Host header:
					//
					
					if (strcmp_P(header, (PGM_P)F("Host")) == 0) {
						value = strtok(NULL, " ");
						
						// #TODO ... or not
					}
					
					//
					// [3] Upgrade header:
					//
					
					else if (strcmp_P(header, (PGM_P)F("Upgrade")) == 0) {
						value = strtok(NULL, " ");
						
						if (strcasecmp_P(value, (PGM_P)F("websocket")) != 0) {
							_rejectRequest(client, BAD_REQUEST);
							return false;
						}
						else
							flags |= VALID_UPGRADE_HEADER;
					}
					
					//
					// [4] Connection header:
					//
					
					else if (strcmp_P(header, (PGM_P)F("Connection")) == 0) {
						value = strtok(NULL, " ");
						
						if (strcmp_P(value, (PGM_P)F("Upgrade")) != 0) {
							_rejectRequest(client, UPGRADE_REQUIRED);
							return false;
						}
						else
							flags |= VALID_CONNECTION_HEADER;
					}
					
					//
					// [5] Sec-WebSocket-Key header:
					//
					
					else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Key")) == 0) {
						value = strtok(NULL, " ");
						
						strcpy(secKey, value);
						flags |= VALID_SEC_KEY;
					}
					
					//
					// [6] Sec-WebSocket-Version header:
					//
					
					else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Version")) == 0) {
						value = strtok(NULL, " ");
						uint8_t version = atoi(value);
						
						switch (version) {
							case 13: {
								flags |= VALID_VERSION;
								break;
							}
							default: {
								_rejectRequest(client, BAD_REQUEST);
								return false;
							}
						}
					}
					
					//
					// [ ] Other headers
					//
					
					else {
						if (verifyClient_ && !verifyClient_(header, value)) {
							_rejectRequest(client, CONNECTION_REFUSED);
							return false;
						}
					}
				}
				
				//
				// [7] Empty line (end of request)
				//
					
				else {
					if (!(flags & (VALID_UPGRADE_HEADER | VALID_CONNECTION_HEADER))) {
						_rejectRequest(client, UPGRADE_REQUIRED);
						return false;
					}
					
					if (!(flags & (VALID_SEC_KEY | VALID_VERSION))) {
						_rejectRequest(client, BAD_REQUEST);
						return false;
					}
					
					//
					// Send response:
					//
					// [1] HTTP/1.1 101 Switching Protocols
					// [2] Upgrade: websocket
					// [3] Connection: Upgrade
					// [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
					// [5] 
					//
					
					char acceptKey[32] = { '\0' };
					encodeSecKey(acceptKey, secKey);
					
					char secWebSocketAccept[64] = { '\0' };
					strcpy_P(secWebSocketAccept, (PGM_P)F("Sec-WebSocket-Accept: "));
					strcat(secWebSocketAccept, acceptKey);
					
					client.println(F("HTTP/1.1 101 Switching Protocols"));
					client.println(F("Upgrade: websocket"));
					client.println(F("Connection: Upgrade"));
					client.println(secWebSocketAccept);
					client.println();

					return true;
				}
			}
			
			memset(buffer, '\0', sizeof(buffer));
			counter = 0;
			currentLine++;
		}
	}
	
	_rejectRequest(client, BAD_REQUEST);
	return false;
}

void WebSocketServer::_rejectRequest(NetClient &client, const WebSocketError code) {
	switch (code) {
		case CONNECTION_REFUSED: {
			client.println(F("HTTP/1.1 111 Connection refused"));
			break;
		}
		case BAD_REQUEST: {
			client.println(F("HTTP/1.1 400 Bad Request"));
			break;
		}		
		case UPGRADE_REQUIRED: {
			client.println(F("HTTP/1.1 426 Upgrade Required"));
			break;
		}		
		case SERVICE_UNAVAILABLE: {
			client.println(F("HTTP/1.1 503 Service Unavailable"));
			break;
		}
		default: {
			client.println(F("HTTP/1.1 501 Not Implemented"));
			break;
		}
	}
	
	client.stop();
	_triggerError(code);
}

void WebSocketServer::_heartbeat() {
	static unsigned long previousTime = 0;
	
	unsigned long currentTime = millis();
  if (currentTime - previousTime > 1000) {
    previousTime = currentTime;

		for (byte i = 0; i < MAX_CONNECTIONS; i++) {
			WebSocket *socket = sockets_[i];
			if (socket) {
				if (!socket->isAlive()) {
					delete socket;
					sockets_[i] = NULL;
				}
				else {
					//socket->ping();
				}
			}
		}
  }
}

void WebSocketServer::_triggerError(const WebSocketError code) {
	if (onError_) onError_(code);
}