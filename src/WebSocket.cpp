#include "WebSocket.h"

bool isControlFrame(const eWebSocketOpcode opcode) {
	switch (opcode) {
	case CONNECTION_CLOSE_FRAME:
	case PING_FRAME:
	case PONG_FRAME:
		return true;
	}
	
	return false;
}

WebSocket::WebSocket() :
	m_eReadyState(CLOSED), m_NumPings(0),
	_onOpen(NULL), _onClose(NULL), _onMessage(NULL), _onError(NULL)
{
	memset(m_DataBuffer, '\0', _MAX_FRAME_LENGTH);
}

WebSocket::~WebSocket() {
	terminate();
}

void WebSocket::close(const eWebSocketCloseEvent code, const char *reason, uint16_t length, bool instant) {
	if (m_eReadyState != OPEN) return;
	
	char buffer[32] = { (code >> 8) & 0xFF, code & 0xFF, '\0' };
	memcpy(&buffer[2], reason, sizeof(char) * length);
		
	_send(CONNECTION_CLOSE_FRAME, buffer, 2 + length);
	
	if (instant)
		terminate();
	else
		m_eReadyState = CLOSING;	
}

void WebSocket::terminate() {
	m_Client.stop();
	m_eReadyState = CLOSED;
}

void WebSocket::send(const eWebSocketDataType dataType, const char *message, uint16_t length, bool mask) {
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

void WebSocket::ping() {
	if (m_eReadyState != OPEN) return; 
		
	_send(PING_FRAME, NULL, 0);
	m_NumPings++;
	
	if (m_NumPings > 5) {
		__debugOutput(F("Dead connection, terminating\n"));

		if (_onClose)
			_onClose(*this, ABNORMAL_CLOSURE, NULL, 0);

		terminate();
	}
}

const eWebSocketReadyState &WebSocket::getReadyState() const {
	return m_eReadyState;
}

//
// Private functions:
//

WebSocket::WebSocket(EthernetClient client) :
	m_Client(client), m_eReadyState(OPEN), m_NumPings(0),
	_onOpen(NULL), _onClose(NULL), _onMessage(NULL), _onError(NULL)
{
	memset(m_DataBuffer, '\0', _MAX_FRAME_LENGTH);
}

void WebSocket::_send(const eWebSocketOpcode opcode, const char *data, uint16_t length) {
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

void WebSocket::_send(const eWebSocketOpcode opcode, const char *data, uint16_t length, byte maskingKey[]) {	
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

frame_t *WebSocket::_getFrame(bool maskingAllowed) {
	if (m_eReadyState == CLOSED) return;
	
	if (!m_Client.connected() || !m_Client.available())
		return NULL;
	
	frame_t *pFrame = new frame_t;
	
	byte bite = m_Client.read();
	pFrame->opcode = bite & 0x0F;
	pFrame->isFinal = bite & 0x80;
	
	pFrame->rsv1 = bite & 0x40;
	pFrame->rsv2 = bite & 0x20;
	pFrame->rsv3 = bite & 0x10;
	
	if (pFrame->rsv1 || pFrame->rsv2 || pFrame->rsv3) {
		close(PROTOCOL_ERROR, NULL, 0, true);
		return NULL;
	}
	
#ifdef _DUMP_FRAME		
	printf(F("------------------------------\n"));
	printf(F("bite = 0x%02x\n"), bite);
	printf(F("frame.isFinal = %s\n"), pFrame->isFinal ? "true" : "false");
	printf(F("frame.opcode = %d\n"), pFrame->opcode);
	printf(F("frame.rsv1 = %d\n"), pFrame->rsv1);
	printf(F("frame.rsv2 = %d\n"), pFrame->rsv2);
	printf(F("frame.rsv3 = %d\n"), pFrame->rsv3);	
#endif 
	
	bite = m_Client.read();
	pFrame->length = bite & 0x7F;
	pFrame->isMasked = bite & 0x80;
	
	if (!maskingAllowed && pFrame->isMasked) {
		__debugOutput(F("A server must not mask any frames that it sends to the client."));
		//_triggerError();
		close(PROTOCOL_ERROR, (PGM_P)F("Masked frame from server"), 24, true);
		return NULL;
	}
	
#ifdef _DUMP_FRAME	
	printf(F("bite = 0x%02x\n"), bite);
	printf(F("frame.length = %d (code)\n"), pFrame->length);
	printf(F("frame.isMasked = %s\n"), pFrame->isMasked ? "true" : "false");
#endif 	
	
	// ---

	if (pFrame->length == 126) {
		pFrame->length = m_Client.read() << 8;
		pFrame->length |= m_Client.read() << 0;
	}
	else if (pFrame->length == 127) {
		close(MESSAGE_TOO_BIG, NULL, 0, true);
		return NULL;
	}
	
#ifdef _DUMP_FRAME
		printf(F("frame.length = %u\n"), pFrame->length);
#endif

	if (pFrame->length > _MAX_FRAME_LENGTH) {
		_triggerError(FRAME_LENGTH_EXCEEDED);
		close(MESSAGE_TOO_BIG, NULL, 0, true);
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
			Serial.print(F("0x"));
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

void WebSocket::_handleFrame(bool maskingAllowed) {	
	frame_t *pFrame = _getFrame(maskingAllowed);
	if (!pFrame) return;
	
	if (m_eReadyState == CLOSING)
		if (pFrame->opcode != CONNECTION_CLOSE_FRAME)
			return;
		
	if (isControlFrame(pFrame->opcode)) {
		// control messages must not be fragmented
		if (!pFrame->isFinal || pFrame->length > 125)
			close(PROTOCOL_ERROR, NULL, 0, true);
	}
	
	switch (pFrame->opcode) {
	case CONTINUATION_FRAME:
		if (!strlen(m_DataBuffer)) {
			close(PROTOCOL_ERROR, NULL, 0, true);
			break;
		}
		
		strncat(m_DataBuffer, pFrame->data, pFrame->length);
		if (pFrame->isFinal) {
			if (_onMessage)
				_onMessage(*this, TEXT, m_DataBuffer, strlen(m_DataBuffer));
			
			memset(m_DataBuffer, '\0', _MAX_FRAME_LENGTH);
		}
	break;
	
	case TEXT_FRAME:
		if (strlen(m_DataBuffer)) {
			close(PROTOCOL_ERROR, NULL, 0, true);
			break;
		}
	
		if (pFrame->isFinal) {			
			if (_onMessage)
				_onMessage(*this, TEXT, pFrame->data, pFrame->length);
		}
		else
			strncat(m_DataBuffer, pFrame->data, pFrame->length);
		
	break;
	
	case BINARY_FRAME:
		strncat(m_DataBuffer, pFrame->data, pFrame->length);
		
		if (pFrame->isFinal) {			
			if (_onMessage)
				_onMessage(*this, BINARY, m_DataBuffer, strlen(m_DataBuffer));
			
			memset(m_DataBuffer, '\0', _MAX_FRAME_LENGTH);
		}
	break;
	
	case CONNECTION_CLOSE_FRAME: {
		uint16_t code = 0;
		const char *reason = NULL;

		if (pFrame->length) {
			for (int i = 0; i < 2; i++)
				code = (code << 8) + (pFrame->data[i] & 0xFF);
			
			if (!isCloseCodeValid(code)) {
				close(PROTOCOL_ERROR, NULL, 0, true);
				break;
			}
			
			reason = (pFrame->length) ?
			&(pFrame->data[2]) : NULL;
		}
		
		if (m_eReadyState == OPEN) {
			if (pFrame->length)
				_send(pFrame->opcode, pFrame->data, pFrame->length);
			else
				close(NORMAL_CLOSURE, NULL, 0, true);
			
			m_eReadyState = CLOSING;
		}
		
		if (_onClose)
			_onClose(*this, code, reason, pFrame->length);
	
		terminate();
	}
	break;
	case PING_FRAME:
		_send(PONG_FRAME, pFrame->data, pFrame->length);
	break;
	
	case PONG_FRAME:
		if (m_NumPings > 0) m_NumPings--;
	break;
	
	default:
		__debugOutput(F("Unrecognized frame opcode: %d\n"), pFrame->opcode);
		_triggerError(UNKNOWN_OPCODE);
		close(PROTOCOL_ERROR);
	break;
	}
	
	delete[] pFrame->data;
	delete pFrame;
}

void WebSocket::_triggerError(const eWebSocketError code) {
	if (_onError) _onError(code);
}