#include "config.h"
#include "utility.h"
#include "WebSocketClient.h"

#ifdef _DEBUG
# include <SPI.h>
#endif

WebSocketClient::WebSocketClient() {	
	m_eReadyState = CLOSED;
	
	_onOpen = NULL;
	_onClose = NULL;
	_onMessage = NULL;
	_onError = NULL;
}

WebSocketClient::~WebSocketClient() {
	close();

	_onOpen = NULL;
	_onClose = NULL;
	_onMessage = NULL;
	_onError = NULL;
}

bool WebSocketClient::open(const char *host, uint16_t port, char path[]) {
	close();
	
	if (!m_Client.connect(host, port)) {
		_triggerError(CONNECTION_ERROR);
		_terminate();
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
		_triggerError(CONNECTION_TIMED_OUT);
		_terminate();
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
					_triggerError(BAD_REQUEST);
					_terminate();
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

					if (strcmp_P(value, (PGM_P)F("websocket")) != 0) {
						_triggerError(UPGRADE_REQUIRED);
						_terminate();
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
						_triggerError(UPGRADE_REQUIRED);
						_terminate();
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
						_triggerError(BAD_REQUEST);
						_terminate();
						return false;
					}

					hasAcceptKey = true;
				}
				
				//
				// [5] Last one (empty):
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
		_triggerError(UPGRADE_REQUIRED);
		_terminate();
		return false;
	}

	if (!hasConnection) {
		_triggerError(UPGRADE_REQUIRED);
		_terminate();
		return false;
	}

	if (!hasAcceptKey) {
		_triggerError(BAD_REQUEST);	
		_terminate();
		return false;
	}
	
	m_eReadyState = OPEN;
	
	if (_onOpen) _onOpen(*this);
	return true;
}

void WebSocketClient::close(const eWebSocketCloseEvent code, const char *reason, uint16_t length) {
	if (m_eReadyState == OPEN) {
		char buffer[32] = { (code >> 8) & 0xFF, code & 0xFF, '\0' };
		memcpy(&buffer[2], reason, sizeof(char) * length);
		
		_send(CONNECTION_CLOSE_FRAME, buffer, 2 + length);
		m_eReadyState = CLOSING;
	}
}

bool WebSocketClient::isOpen() {
	return m_eReadyState == OPEN;
}

void WebSocketClient::send(const eWebSocketDataType dataType, const char *message, uint16_t length, bool mask) {
	if (m_eReadyState != OPEN) return;
	
	uint8_t opcode = (dataType == TEXT) ? TEXT_FRAME : BINARY_FRAME;
	
	if (mask) {
		randomSeed(analogRead(0));
		
		byte maskingKey[4] = { 0x00 };
		for (uint8_t i = 0; i < 4; i++)
			maskingKey[i] = random(0xFF);
		
		_send(opcode, message, length, maskingKey);
	}
	else {
		_send(opcode, message, length);
	}
}

void WebSocketClient::ping() {
	if (m_eReadyState != OPEN) return; 
		
	_send(PING_FRAME, NULL, 0);
	m_NumPings++;
	
	if (m_NumPings > 5) {
		__debugOutput(F("Server is dead, disconnecting\n"));

		if (_onClose)
			_onClose(*this, ABNORMAL_CLOSURE, NULL, 0);

		_terminate();
	}
}

void WebSocketClient::listen() {
	if (m_eReadyState == CLOSED) return;
	
	if (!m_Client.connected() || !m_Client.available())
		return;
	
	frame_t *pFrame = _getFrame();
	if (pFrame == NULL)
		return;
	
	if (m_eReadyState == CLOSING)
		if (pFrame->opcode != CONNECTION_CLOSE_FRAME)
			return;

	switch (pFrame->opcode) {
	case CONTINUATION_FRAME:
		_triggerError(UNSUPPORTED_OPCODE);		
		close(UNSUPPORTED_DATA);
	break;	
		
	case TEXT_FRAME:
		if (_onMessage)
			_onMessage(*this, TEXT, pFrame->data, pFrame->length);
	break;
	
	case BINARY_FRAME:
		if (_onMessage)
			_onMessage(*this, BINARY, pFrame->data, pFrame->length);
	break;

	case CONNECTION_CLOSE_FRAME: {
		uint16_t code = 0;
		for (int i = 0; i < 2; i++)
			code = (code << 8) + (pFrame->data[i] & 0xFF);
		
		const char *reason = (pFrame->length) ?
			&(pFrame->data[2]) : NULL;
		
		if (m_eReadyState == OPEN) {
			_send(pFrame->opcode, pFrame->data, pFrame->length);
			m_eReadyState = CLOSING;
		}
		
		if (_onClose)
			_onClose(*this, code, reason, pFrame->length);
	
		_terminate();
	}
	break;

	case PING_FRAME:
		_send(PONG_FRAME, pFrame->data, pFrame->length);
	break;
		
	case PONG_FRAME:
		if (m_NumPings > 0) m_NumPings--;
	break;

	default:
		_triggerError(UNKNOWN_OPCODE);
		close(PROTOCOL_ERROR);
	break;
	}

	delete[] pFrame->data;
	delete pFrame;
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

void WebSocketClient::_send(const eWebSocketOpcode opcode, const char *data, uint16_t length) {
	if (m_eReadyState != OPEN) return;
	
	m_Client.write(static_cast<byte>(opcode | 0x80));
	if (length >= 126) {
		m_Client.write(static_cast<byte>(126));
		m_Client.write(static_cast<byte>((length >> 8) & 0xFF));
		m_Client.write(static_cast<byte>((length >> 0) & 0xFF));
	} 
	else {
		m_Client.write(static_cast<byte>(length));
	}
	
	m_Client.write(data, length);
	m_Client.flush();
}

void WebSocketClient::_send(const eWebSocketOpcode opcode, const char *data, uint16_t length, byte maskingKey[]) {
	if (m_eReadyState != OPEN) return;
	
	m_Client.write(static_cast<byte>(opcode | 0x80));
	if (length >= 126) {
		m_Client.write(static_cast<byte>(126 |  0x80));
		m_Client.write(static_cast<byte>((length >> 8) & 0xFF));
		m_Client.write(static_cast<byte>((length >> 0) & 0xFF));
	} 
	else {
		m_Client.write(static_cast<byte>(length | 0x80));
	}
	
	m_Client.write(maskingKey, 4);
		
	for (uint16_t i = 0; i < length; i++)
		m_Client.write(static_cast<byte>(data[i] ^ maskingKey[i % 4]));
	
	m_Client.flush();
}

frame_t *WebSocketClient::_getFrame() {
	frame_t *pFrame = new frame_t;
	
	byte bite = m_Client.read();
	pFrame->opcode = bite & 0xF;
	pFrame->isFinal = bite & 0x80;
	
#ifdef _DUMP_FRAME		
	printf(F("------------------------------\n"));
	printf(F("bite = 0x%02x\n"), bite);
	printf(F("frame.opcode = %d\n"), pFrame->opcode);
	printf(F("frame.isFinal = %s\n"), pFrame->isFinal ? "true" : "false");
#endif 
	
	bite = m_Client.read();
	pFrame->length = bite & 0x7F;
	pFrame->isMasked = bite & 0x80;
	
#ifdef _DUMP_FRAME	
	printf(F("bite = 0x%02x\n"), bite);
	printf(F("frame.length = %d (code)\n"), pFrame->length);
	printf(F("frame.isMasked = %s\n"), pFrame->isMasked ? "true" : "false");
#endif 	
	
	// ---

	if (pFrame->length == 126) {
		pFrame->length = m_Client.read() << 8;
		pFrame->length |= m_Client.read() << 0;
		
#ifdef _DUMP_FRAME
		printf(F("frame.length = %d\n"), pFrame->length);
#endif
	}
	else if (pFrame->length == 127) {
		_triggerError(FRAME_LENGTH_EXCEEDED);
		close(MESSAGE_TOO_BIG);
		return NULL;
	}

	if (pFrame->length > _MAX_FRAME_LENGTH) {
		_triggerError(FRAME_LENGTH_EXCEEDED);
		close(MESSAGE_TOO_BIG);
		return NULL;
	}
	
	// ---

	if (pFrame->isMasked)
	for (byte i = 0; i < 4; i++)
		pFrame->mask[i] = m_Client.read();
	
#ifdef _DUMP_FRAME
	Serial.print(F("mask = "));
	for (byte i = 0; i < 4; i++) {
		Serial.print("0x");
		Serial.print(pFrame->mask[i], HEX);
		Serial.print(F(" "));
	}
	
	Serial.println();
#endif
	
	pFrame->data = new char[pFrame->length + 1];
	memset(pFrame->data, '\0', pFrame->length + 1);

#ifdef _DUMP_FRAME
	Serial.print(F("frame.data = \""));
#endif

	for (uint16_t i = 0; i < pFrame->length; i++) {
		if (pFrame->isMasked)
			pFrame->data[i] = m_Client.read() ^ pFrame->mask[i % 4];
		else
			pFrame->data[i] = m_Client.read();
		
#ifdef _DUMP_FRAME
		if (pFrame->opcode != TEXT_FRAME) {
			Serial.print("0x");
			Serial.print(static_cast<uint8_t>(pFrame->data[i]), HEX);
			if (i < pFrame->length - 1)
				 Serial.print(" ");
		}
		else
			Serial.print(pFrame->data[i]);
#endif
	}
		
#ifdef _DUMP_FRAME
	Serial.println(F("\""));
#endif

	return pFrame;
}

void WebSocketClient::_terminate() {
	m_Client.stop();
	m_eReadyState = CLOSED;
}

void WebSocketClient::_triggerError(const eWebSocketError code) {
	if (_onError) _onError(*this, code);
}