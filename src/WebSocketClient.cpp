#include "config.h"
#include "utility.h"
#include "WebSocketClient.h"

#ifdef _DEBUG
# include <SPI.h>
#endif

WebSocketClient::WebSocketClient() {	
	m_bConnected = false;
	
	_onOpen = NULL;
	_onClose = NULL;
	_onMessage = NULL;
}

WebSocketClient::~WebSocketClient() {
	close();

	_onOpen = NULL;
	_onClose = NULL;
	_onMessage = NULL;
}

bool WebSocketClient::open(const char *hostname, uint16_t port, char path[]) {
	close();
	
	int result = m_Client.connect(hostname, port);
	if (!result) {
#ifdef _DEBUG
		Serial.print(F("[ERROR] Can't connect to: ")); Serial.print(hostname); Serial.print(F(":")); Serial.println(port);
#endif
		return false;
	}
		
	char buffer[128] = { '\0' };

	//
	// Send request (client handshake):
	//
	
	snprintf_P(buffer, sizeof(buffer), (PGM_P)F("GET %s HTTP/1.1"), path);
	m_Client.println(buffer);
	
	snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Host: %s:%d"), hostname, port);
	m_Client.println(buffer);
	
	m_Client.println(F("Upgrade: websocket"));
	m_Client.println(F("Connection: Upgrade"));
	
	char key[26] = { '\0' };
	generateSecKey(key);
	
	snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Key: %s"), key);
	m_Client.println(buffer);
	
	m_Client.println(F("Sec-WebSocket-Version: 13\r\n"));
	
	//
	// Wait for response (300ms by default):
	//
	
	if (!_poll(300)) {
#ifdef _DEBUG
		Serial.println(F("[ERROR] Timeout"));
#endif
		
		m_Client.stop();
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
	
	while ((bite = m_Client.read()) != -1) {
		buffer[counter++] = bite;

		if (static_cast<char>(bite) == '\n') {
			buffer[strcspn(buffer, "\r\n")] = '\0';

#ifdef _DUMP_HANDSHAKE
			Serial.print(F("[Line #")); Serial.print(currentLine); Serial.print(F("] "));
			Serial.println(buffer);
#endif
			
			if (currentLine == 0) {
				if (strncmp_P(buffer, (PGM_P)F("HTTP/1.1 101"), 12) != 0) {
#ifdef _DEBUG
					Serial.print(F("[ERROR] Invalid HTTP Resonse: "));
					Serial.println(buffer);
#endif
					m_Client.stop();
					return false;
				}
			}
			else {
				char *header = strtok(buffer, ":");
				char *value = NULL;

				if (strcmp_P(header, (PGM_P)F("Upgrade")) == 0) {
					value = strtok(NULL, " ");

					if (strcmp_P(value, (PGM_P)F("websocket")) != 0) {
#ifdef _DEBUG
						Serial.print(F("[ERROR] Invalid \'Upgrade\' header: "));
						Serial.println(value);
#endif
						m_Client.stop();
						return false;
					}

					hasUpgrade = true;
				}
				else if (strcmp_P(header, (PGM_P)F("Connection")) == 0) {
					value = strtok(NULL, " ");

					if (strcmp_P(value, (PGM_P)F("Upgrade")) != 0) {
#ifdef _DEBUG
						Serial.print(F("[ERROR] Invalid \'Connection\' header: "));
						Serial.println(value);
#endif
						m_Client.stop();
						return false;
					}

					hasConnection = true;
				}
				else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Accept")) == 0) {
					value = strtok(NULL, " ");

					char encodedKey[32] = { '\0' };
					encodeSecKey(encodedKey, key);
					
					if (strcmp(value, encodedKey) != 0) {
#ifdef _DEBUG
						Serial.print(F("[ERROR] Incorrect \'Sec-WebSocket-Accept\': ")); Serial.println(value);
						Serial.print(F("\tclient key = ")); Serial.println(key);
						Serial.print(F("\tencoded key = ")); Serial.println(encodedKey);
#endif
						m_Client.stop();
						return false;
					}

					hasAcceptKey = true;
				}
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
#ifdef _DEBUG
		Serial.println(F("[ERROR] Missing \'Upgrade\' header"));
#endif
		m_Client.stop();
		return false;
	}

	if (!hasConnection) {
#ifdef _DEBUG
		Serial.println(F("[ERROR] Missing \'Connection\' header"));
#endif
		m_Client.stop();
		return false;
	}

	if (!hasAcceptKey) {
#ifdef _DEBUG
		Serial.println(F("[ERROR] Missing \'Sec-WebSocket-Accept\' header"));
#endif
		m_Client.stop();
		return false;
	}

	m_bConnected = true;
	if (_onOpen) _onOpen(*this);
	
	return true;
}

void WebSocketClient::close() {
	if (m_bConnected) {
		m_Client.write((uint8_t) 0x88);		// terminate opcode
		m_Client.write((uint8_t) 0x00);
		
		m_Client.flush();
		delay(10);
	}

	m_Client.stop();
	
	m_bConnected = false;
}

bool WebSocketClient::isOpen() {
	return m_bConnected;
}

void WebSocketClient::send(const char *message, uint16_t length, bool mask) {
	if (!m_bConnected)
		return;
	
	if (mask) {
		byte maskingKey[4] = { 0xDB, 0x32, 0xE1, 0x79 }; // #TODO: random generated mask
		_send(0x01, message, length, maskingKey);
	}
	else {
		_send(0x01, message, length);
	}
}

void WebSocketClient::ping() {
	_send(0x09, "", 0);
}

void WebSocketClient::listen() {
	if (!m_bConnected || !m_Client.available())
		return;
	
	byte bite = m_Client.read();
	bool fin = bite & 0x80;
	int16_t opcode = bite & 0xF;
	
	bite = m_Client.read();
	bool mask = bite & 0x80;
	uint16_t length = bite & 0x7F;
	
	// ---

	if (length == 126) {
		length = m_Client.read() << 8;
		length |= m_Client.read() << 0;
	}
	else if (length == 127) {
#ifdef _DEBUG
		Serial.println(F("[ERROR] Frame length exceeded"));
#endif 
		close();
		return;
	}

	if (length > _MAX_FRAME_LENGTH) {
#ifdef _DEBUG
		Serial.println(F("[ERROR] Frame length exceeded"));
#endif 
		close();
		return;
	}
	
	// ---

	byte maskingKey[4] = { 0x00 };

	if (mask)
	for (byte i = 0; i < 4; i++)
		maskingKey[i] = m_Client.read();

	char *data = new char[length + 1];
	memset(data, '\0', length + 1);

	for (uint16_t i = 0; i < length; i++)
		if (mask)
			data[i] = m_Client.read() ^ maskingKey[i % 4];
		else
			data[i] = m_Client.read();
		
#ifdef _DUMP_FRAME
	Serial.print(F("opcode = 0x")); Serial.print(opcode, HEX);
	Serial.print(F(" masked = ")); Serial.println(mask);
	Serial.print(F("data = ")); Serial.println(data);
#endif

	// ---

	switch (opcode) {
	case 0x00:	// continuation
		// #TODO ...
	break;	
		
	case 0x01:	// text
		if (_onMessage) _onMessage(*this, data, length);
	break;
	
	case 0x02:	// binary
		// #TODO ...
	break;

	case 0x08:	// close
		if (_onClose) _onClose(*this);
	break;

	case 0x09:	// ping
#ifdef _DEBUG
		Serial.println(F("[INFO] Received ping"));
#endif
		_send(0x0A, data, length, maskingKey);
	break;
		
	case 0x0A:	// pong
#ifdef _DEBUG
		Serial.println(F("[INFO] Received pong"));
#endif
	break;

	default:
#ifdef _DEBUG
		Serial.print(F("[ERROR] Unknown opcode (0x")); Serial.print(opcode, HEX); Serial.println(F(")"));
#endif 
		close();
	break;
	}

	delete[] data;
}

void WebSocketClient::setOnOpenCallback(onOpenCallback *callback) {
	_onOpen = callback;
}

void WebSocketClient::setOnCloseCallback(onCloseCallback *callback) {
	_onClose = callback;
}

void WebSocketClient::setOnMessageCallback(onMessageCallback *callback) {
	_onMessage = callback;
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

void WebSocketClient::_send(uint8_t opcode, const char *data, uint16_t length) {
	if (!m_bConnected)
		return;
	
	m_Client.write(opcode | 0x80);
	if (length >= 126) {
		m_Client.write((uint8_t) 126);
		m_Client.write((uint8_t) (length >> 8) & 0xff);
		m_Client.write((uint8_t) (length >> 0) & 0xff);
	} 
	else {
		m_Client.write((uint8_t) length);
	}
	
	m_Client.write(data, length);
}

void WebSocketClient::_send(uint8_t opcode, const char *data, uint16_t length, byte maskingKey[]) {
	if (!m_bConnected)
		return;
	
	m_Client.write(opcode | 0x80);
	if (length >= 126) {
		m_Client.write((uint8_t) 126 |  0x80);
		m_Client.write((uint8_t) (length >> 8) & 0xff);
		m_Client.write((uint8_t) (length >> 0) & 0xff);
	} 
	else {
		m_Client.write((uint8_t) length | 0x80);
	}
	
	for (byte i = 0; i < 4; i++)
		m_Client.write((uint8_t) maskingKey[i]);
		
	for (uint16_t i = 0; i < length; i++)
		m_Client.write(data[i] ^ maskingKey[i % 4]);
}