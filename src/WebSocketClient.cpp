#include "WebSocketClient.h"

WebSocketClient::WebSocketClient() {
}

WebSocketClient::~WebSocketClient() {
	close();
}

bool WebSocketClient::open(const char *host, uint16_t port, char path[]) {
	close(GOING_AWAY, NULL, 0, true);	// Close if already open
	
	if (!m_Client.connect(host, port)) {
		__debugOutput(F("Error in connection establishment: net::ERR_CONNECTION_REFUSED\n"));
		_triggerError(CONNECTION_ERROR);
		terminate();
		return false;
	}
	
	//
	// Send request (client handshake):
	//
	// [1] GET /chat HTTP/1.1
	// [2] Host: example.com:8000
	// [3] Upgrade: websocket
	// [4] Connection: Upgrade
	// [5] Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
	// [6] Sec-WebSocket-Version: 13
	// [7] 
	//
	
	char buffer[128] = { '\0' };
	
	snprintf_P(buffer, sizeof(buffer), (PGM_P)F("GET %s HTTP/1.1"), path);
	m_Client.println(buffer);
	
	snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Host: %s:%d"), host, port);
	m_Client.println(buffer);
	
	m_Client.println(F("Upgrade: websocket"));
	m_Client.println(F("Connection: Upgrade"));
	
	char key[26] = { '\0' };
	generateSecKey(key);
	
	snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Key: %s"), key);
	m_Client.println(buffer);
	
	m_Client.println(F("Sec-WebSocket-Version: 13\r\n"));
	m_Client.flush();
	
	//
	// Wait for response (5sec by default):
	//
	
	if (!_poll(5000)) {
		__debugOutput(F("Error in connection establishment: net::ERR_CONNECTION_TIMED_OUT\n"));
		_triggerError(CONNECTION_TIMED_OUT);
		terminate();
		return false;
	}
	
	bool hasUpgrade = false;
	bool hasConnection = false;
	bool hasAcceptKey = false;

	int16_t bite;
	byte currentLine = 0;
	byte counter = 0;
	
	memset(buffer, '\0', sizeof(buffer));
	
	//
	// Read response (server handshake):
	//
	// [1] HTTP/1.1 101 Switching Protocols
	// [2] Upgrade: websocket
	// [3] Connection: Upgrade
	// [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
	// [5] 
	//
	
	while ((bite = m_Client.read()) != -1) {
		buffer[counter++] = bite;

		if (static_cast<char>(bite) == '\n') {
			buffer[strcspn(buffer, "\r\n")] = '\0'; // trim CRLF

#ifdef _DUMP_HANDSHAKE
			printf(F("[Line #%d] %s\n"), currentLine, buffer);
#endif
			
			if (currentLine == 0) {
				if (strncmp_P(buffer, (PGM_P)F("HTTP/1.1 101"), 12) != 0) {
					__debugOutput(F("Error during WebSocket handshake: net::ERR_INVALID_HTTP_RESPONSE\n"));
					_triggerError(BAD_REQUEST);
					terminate();
					return false;
				}
			}
			else {
				char *header = strtok(buffer, ":");
				char *value = NULL;

				//
				// [2] Upgrade header:
				//
				
				if (strcmp_P(header, (PGM_P)F("Upgrade")) == 0) {
					value = strtok(NULL, " ");

					if (strcasecmp_P(value, (PGM_P)F("websocket")) != 0) {
						__debugOutput(F("Error during WebSocket handshake: 'Upgrade' header value is not 'websocket': %s\n"), value);
						_triggerError(UPGRADE_REQUIRED);
						terminate();
						return false;
					}

					hasUpgrade = true;
				}
				
				//
				// [3] Connection header:
				//
				
				else if (strcmp_P(header, (PGM_P)F("Connection")) == 0) {
					value = strtok(NULL, " ");

					if (strcmp_P(value, (PGM_P)F("Upgrade")) != 0) {
						__debugOutput(F("Error during WebSocket handshake: 'Connection' header value is not 'Upgrade': %s\n"), value);
						_triggerError(UPGRADE_REQUIRED);
						terminate();
						return false;
					}

					hasConnection = true;
				}
				
				//
				// [4] Sec-WebSocket-Accept header:
				//
				
				else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Accept")) == 0) {
					value = strtok(NULL, " ");

					char encodedKey[32] = { '\0' };
					encodeSecKey(encodedKey, key);
					
					if (strcmp(value, encodedKey) != 0) {
						__debugOutput(F("Error during WebSocket handshake: Incorrect 'Sec-WebSocket-Accept' header value\n"));
						_triggerError(BAD_REQUEST);
						terminate();
						return false;
					}

					hasAcceptKey = true;
				}
				
				//
				// [5] Empty line (end of response)
				//
				
				else if (strcmp(header, 0) == 0) {
					break;
				}
				else {
					// Don't care about other headers ...
				}
			}

			memset(buffer, '\0', sizeof(buffer));
			counter = 0;
			currentLine++;
		}
	}

	if (!hasUpgrade) {
		__debugOutput(F("Error during WebSocket handshake: 'Upgrade' header is missing\n"));
		_triggerError(UPGRADE_REQUIRED);
		terminate();
		return false;
	}

	if (!hasConnection) {
		__debugOutput(F("Error during WebSocket handshake: 'Connection' header is missing\n"));
		_triggerError(UPGRADE_REQUIRED);
		terminate();
		return false;
	}

	if (!hasAcceptKey) {
		__debugOutput(F("Error during WebSocket handshake: 'Sec-WebSocket-Accept' header is missing\n"));
		_triggerError(BAD_REQUEST);	
		terminate();
		return false;
	}
	
	m_eReadyState = OPEN;
	
	if (_onOpen) _onOpen(*this);
	return true;
}

void WebSocketClient::listen() {
	_handleFrame();
}

// ---

void WebSocketClient::setOnOpenCallback(onOpenCallback *callback) {
	_onOpen = callback;
}

void WebSocketClient::setOnCloseCallback(onCloseCallback *callback) {
	_onClose = callback;
}

void WebSocketClient::setOnMessageCallback(onMessageCallback *callback) {
	_onMessage = callback;
}

void WebSocketClient::setOnErrorCallback(onErrorCallback *callback) {
	_onError = callback;
}

//
// Private functions:
//


bool WebSocketClient::_poll(uint16_t maxAttempts, uint8_t time) {
	uint16_t attempts = 0;
	
	while (!m_Client.available() && attempts < maxAttempts) {
		attempts++;
		delay(time);
	}
	
	return attempts < maxAttempts;
}