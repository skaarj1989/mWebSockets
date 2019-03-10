#include "WebSocketServer.h"
#include "utility.h"

WebSocketServer::WebSocketServer(uint16_t port) :
	m_Server(port),
	_onOpen(NULL), _onClose(NULL), _onMessage(NULL), _onError(NULL)
{
	for (byte i = 0; i < MAX_CONNECTIONS; i++)
		m_pSockets[i] = NULL;
}

WebSocketServer::~WebSocketServer() {
	shutdown();
}

void WebSocketServer::begin() {
	m_Server.begin();
}

void WebSocketServer::shutdown() {
	for (byte i = 0; i < MAX_CONNECTIONS; i++)
		if (m_pSockets[i])
			m_pSockets[i]->close();
}

void WebSocketServer::listen() {
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
	WiFiClient client = m_Server.available();
#else
	EthernetClient client = m_Server.available();
#endif

	if (client) {
		WebSocket *pSocket = _getWebSocket(client);
		
		if (!pSocket) {

			// New client ...
			
			for (byte i = 0; i < MAX_CONNECTIONS; i++) {
				if (!m_pSockets[i]) {
					
					// Found free room ...
					
					if (_handleRequest(client)) {
						pSocket = new WebSocket(client);
						m_pSockets[i] = pSocket;
						
						// Install callbacks:
						
						pSocket->_onOpen = _onOpen;
						pSocket->_onClose = _onClose;
						pSocket->_onMessage = _onMessage;
						pSocket->_onError = _onError;
						
						if (_onOpen) pSocket->_onOpen(*pSocket);
					}
					
					break;
				}
				
				if (i == MAX_CONNECTIONS - 1) {
					__debugOutput(F("Server is full!\n"));
					_rejectRequest(client, BAD_REQUEST);
				}
			}
		}
		
		// ---
		
		if (pSocket)
			pSocket->_handleFrame();
	}
	
	_heartbeat();
}

void WebSocketServer::broadcast(const eWebSocketDataType dataType, const char *message, uint16_t length) {
	for (byte i = 0; i < MAX_CONNECTIONS; i++) {
		WebSocket *pSocket = m_pSockets[i];
		if (!pSocket) continue;
		
		pSocket->send(dataType, message, length);
	}
}

uint8_t WebSocketServer::countClients() {
	byte count = 0;
	
	for (byte i = 0; i < MAX_CONNECTIONS; i++)
		if (m_pSockets[i] && m_pSockets[i]->m_Client.connected())
			count++;
	
	return count;
}

void WebSocketServer::setOnOpenCallback(onOpenCallback *callback) {
	_onOpen = callback;
}

void WebSocketServer::setOnCloseCallback(onCloseCallback *callback) {
	_onClose = callback;
}

void WebSocketServer::setOnMessageCallback(onMessageCallback *callback) {
	_onMessage = callback;
}

void WebSocketServer::setOnErrorCallback(onErrorCallback *callback) {
	_onError = callback;
}

//
// Private functions:
//

void WebSocketServer::_heartbeat() {
	static unsigned long previousTime = 0;
	
	unsigned long currentTime = millis();
  if (currentTime - previousTime > 1000) {
    previousTime = currentTime;

		for (byte i = 0; i < MAX_CONNECTIONS; i++) {
			WebSocket *pSocket = m_pSockets[i];
			
			if (pSocket) {
				bool alive = pSocket->m_Client.connected();
				alive &= pSocket->m_eReadyState != WSRS_CLOSED;
				
				if (!alive) {
					delete pSocket;
					m_pSockets[i] = NULL;
				}
				else {
					//pSocket->ping();
				}
			}
		}
  }
}

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
WebSocket *WebSocketServer::_getWebSocket(WiFiClient client) {
#else
WebSocket *WebSocketServer::_getWebSocket(EthernetClient client) {
#endif
	for (byte i = 0; i < MAX_CONNECTIONS; i++) {
		WebSocket *pSocket = m_pSockets[i];
		if (pSocket && pSocket->m_Client == client)
			return pSocket;
	}
	
	return NULL;
}

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
bool WebSocketServer::_handleRequest(WiFiClient &client) {
#else
bool WebSocketServer::_handleRequest(EthernetClient &client) {
#endif

	bool hasUpgrade = false;
	bool hasConnection = false;
	bool hasSecKey = false;
	bool hasVersion = false;
	
	int16_t bite;
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
			buffer[lineBreakPos] = '\0';		// trim CRLF
		
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
					case 0:
						if (strcmp_P(pch, (PGM_P)F("GET")) != 0) {
							_rejectRequest(client, BAD_REQUEST);
							return false;
						}
					break;
					
					case 1:
						// path doesn't matter ...
					break;
					
					case 2:
						if (strcmp_P(pch, (PGM_P)F("HTTP/1.1")) != 0) {
							_rejectRequest(client, BAD_REQUEST);
							return false;
						}
					break;
					
					default:
						_rejectRequest(client, BAD_REQUEST);
						return false;
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
						
						// #TODO ...
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
						else {
							hasUpgrade = true;
						}
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
							hasConnection = true;
					}
					
					//
					// [5] Sec-WebSocket-Key header:
					//
					
					else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Key")) == 0) {
						value = strtok(NULL, " ");
						
						strcpy(secKey, value);
						hasSecKey = true;
					}
					
					//
					// [6] Sec-WebSocket-Version header:
					//
					
					else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Version")) == 0) {
						value = strtok(NULL, " ");
						uint8_t version = atoi(value);
						
						switch (version) {
						case 13:
							hasVersion = true;
						break;
							
						default:
							_rejectRequest(client, BAD_REQUEST);
							return false;
						}
					}
					else {
						// Don't care about other headers ...
					}
				}
				
				//
				// [7] Empty line (end of request)
				//
					
				else {
					if (!hasUpgrade || !hasConnection) {
						_rejectRequest(client, UPGRADE_REQUIRED);
						return false;
					}
					
					if (!hasSecKey || !hasVersion) {
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
					
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
					client.setNoDelay(true);
					client.setTimeout(TIMEOUT_INTERVAL);
					//delay(1);
#endif

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

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
void WebSocketServer::_rejectRequest(WiFiClient &client, const eWebSocketError code) {
#else
void WebSocketServer::_rejectRequest(EthernetClient &client, const eWebSocketError code) {
#endif
	switch (code) {
	case BAD_REQUEST:
		client.println(F("HTTP/1.1 400 Bad Request"));
	break;
	
	case UPGRADE_REQUIRED:
		client.println(F("HTTP/1.1 426 Upgrade Required"));
	break;
	
	case CONNECTION_REFUSED:
		client.println(F("HTTP/1.1 111 Connection refused"));
	break;
	
	default:
		client.println(F("HTTP/1.1 501 Not Implemented"));
	break;
	}
	
	client.stop();
	
	_triggerError(code);
}

void WebSocketServer::_triggerError(const eWebSocketError code) {
	if (_onError) _onError(code);
}