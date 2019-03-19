#include "WebSocketClient.h"
#include "utility.h"

namespace net {

WebSocketClient::WebSocketClient() {
}

WebSocketClient::~WebSocketClient() {
	close();
}

bool WebSocketClient::open(const char *host, uint16_t port, const char *path) {
	close(GOING_AWAY, NULL, 0, true);	// close if already open
	
	if (!client_.connect(host, port)) {
		__debugOutput(F("Error in connection establishment: net::ERR_CONNECTION_REFUSED\n"));
		_triggerError(CONNECTION_ERROR);
		return false && terminate();
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
	client_.println(buffer);
	
	snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Host: %s:%d"), host, port);
	client_.println(buffer);
	
	client_.println(F("Upgrade: websocket"));
	client_.println(F("Connection: Upgrade"));
	
	char key[26] = { '\0' };
	generateSecKey(key);
	
	snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Key: %s"), key);
	client_.println(buffer);
	
	client_.println(F("Sec-WebSocket-Version: 13\r\n"));
	client_.flush();
	
	//
	// Wait for response (5sec by default):
	//
	
	if (!_waitForResponse(TIMEOUT_INTERVAL)) {
		__debugOutput(F("Error in connection establishment: net::ERR_CONNECTION_TIMED_OUT\n"));
		_triggerError(REQUEST_TIMEOUT);
		return false && terminate();
	}
	
	// ---
	
	uint8_t flags = 0x0;
	
	int16_t bite = -1;
	byte currentLine = 0;
	byte counter = 0;
	
	memset(buffer, '\0', sizeof(buffer));
	
	//
	// Read response (server-side handshake):
	//
	// [1] HTTP/1.1 101 Switching Protocols
	// [2] Upgrade: websocket
	// [3] Connection: Upgrade
	// [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
	// [5] 
	//
	
	while ((bite = _read()) != -1) {
		buffer[counter++] = bite;
		
		if (static_cast<char>(bite) == '\n') {
			uint8_t lineBreakPos = strcspn(buffer, "\r\n");
			buffer[lineBreakPos] = '\0';

#ifdef _DUMP_HANDSHAKE
			printf(F("[Line #%d] %s\n"), currentLine, buffer);
#endif
			
			if (currentLine == 0) {
				if (strncmp_P(buffer, (PGM_P)F("HTTP/1.1 101"), 12) != 0) {
					__debugOutput(F("Error during WebSocket handshake: net::ERR_INVALID_HTTP_RESPONSE\n"));
					_triggerError(BAD_REQUEST);
					return false && terminate();
				}
			}
			else {
				if (lineBreakPos > 0) {
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
							return false && terminate();
						}

						flags |= VALID_UPGRADE_HEADER;
					}
					
					//
					// [3] Connection header:
					//
					
					else if (strcmp_P(header, (PGM_P)F("Connection")) == 0) {
						value = strtok(NULL, " ");

						if (strcmp_P(value, (PGM_P)F("Upgrade")) != 0) {
							__debugOutput(F("Error during WebSocket handshake: 'Connection' header value is not 'Upgrade': %s\n"), value);
							_triggerError(UPGRADE_REQUIRED);
							return false && terminate();
						}

						flags |= VALID_CONNECTION_HEADER;
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
							return false && terminate();
						}

						flags |= VALID_SEC_KEY;
					}
					else {
						// don't care about other headers ...
					}
				}
				
				//
				// [5] Empty line (end of response)
				//
				
				else
					break;
			}

			memset(buffer, '\0', sizeof(buffer));
			counter = 0;
			currentLine++;
		}
	}

	if (!(flags & VALID_UPGRADE_HEADER)) {
		__debugOutput(F("Error during WebSocket handshake: 'Upgrade' header is missing\n"));
		_triggerError(UPGRADE_REQUIRED);
		return false && terminate();
	}

	if (!(flags & VALID_CONNECTION_HEADER)) {
		__debugOutput(F("Error during WebSocket handshake: 'Connection' header is missing\n"));
		_triggerError(UPGRADE_REQUIRED);
		return false && terminate();
	}

	if (!(flags & VALID_SEC_KEY)) {
		__debugOutput(F("Error during WebSocket handshake: 'Sec-WebSocket-Accept' header is missing\n"));
		_triggerError(BAD_REQUEST);
		return false && terminate();
	}
	
	readyState_ = WebSocketReadyState::OPEN;
	
	if (onOpen_) onOpen_(*this);
	return true;
}

void WebSocketClient::listen() {
	if (!client_.connected()) {
		if (readyState_ == WebSocketReadyState::OPEN) {
			if (onClose_)
				onClose_(*this, ABNORMAL_CLOSURE, NULL, 0);
			
			terminate();
		}
		
		return;
	}
	
	_handleFrame();
}

// ---

void WebSocketClient::setOnOpenCallback(onOpenCallback *callback) {
	onOpen_ = callback;
}

void WebSocketClient::setOnCloseCallback(onCloseCallback *callback) {
	onClose_ = callback;
}

void WebSocketClient::setOnMessageCallback(onMessageCallback *callback) {
	onMessage_ = callback;
}

void WebSocketClient::setOnErrorCallback(onErrorCallback *callback) {
	onError_ = callback;
}

};