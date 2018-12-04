#include "WebSocket.h"
#include "utility.h"

void generateMask(byte *target) {
	randomSeed(analogRead(0));
	
	for (uint8_t i = 0; i < 4; i++)
		target[i] = random(0xFF);
}

bool isControlFrame(uint8_t opcode) {
	return (
		(opcode == PING_FRAME) ||
		(opcode == PONG_FRAME) ||
		(opcode == CONNECTION_CLOSE_FRAME)
	);
}

bool isCloseCodeValid(const uint16_t code) {
	return (
    (code >= 1000 &&
      code <= 1013 &&
      code != 1004 &&
      code != 1005 &&
      code != 1006) ||
    (code >= 3000 && code <= 4999)
	);
}

//
//
//

WebSocket::WebSocket() :
	m_eReadyState(WSRS_CLOSED), m_NumPings(0), m_bMaskEnabled(true),
	_onOpen(NULL), _onClose(NULL), _onMessage(NULL), _onError(NULL)
{
	memset(m_DataBuffer, '\0', BUFFER_MAX_SIZE);
}

WebSocket::~WebSocket() {
	terminate();
}

void WebSocket::close(const eWebSocketCloseEvent code, const char *reason, uint16_t length, bool instant) {
	if (m_eReadyState != WSRS_OPEN) return;
	
	m_eReadyState = WSRS_CLOSING;
	
	char buffer[128] = {
		(code >> 8) & 0xFF,
		code & 0xFF,
		'\0',
	};
	
	if (length)
		memcpy(&buffer[2], reason, sizeof(char) * length);
	
	_send(CONNECTION_CLOSE_FRAME, true, m_bMaskEnabled, buffer, 2 + length);
	
	if (instant)
		terminate();
}

void WebSocket::terminate() {
	//m_Client.flush();
	
	m_Client.stop();
	m_eReadyState = WSRS_CLOSED;
}

void WebSocket::send(const eWebSocketDataType dataType, const char *message, uint16_t length) {
	send(dataType, message, length, m_bMaskEnabled);
}

void WebSocket::send(const eWebSocketDataType dataType, const char *message, uint16_t length, bool mask) {
	if (m_eReadyState != WSRS_OPEN) return;
	
	_send(dataType == TEXT ? 0x1 : 0x2,
		true, mask, message, length);
}

void WebSocket::ping() {
	if (m_eReadyState != WSRS_OPEN) return; 
	
	_send(PING_FRAME, true, m_bMaskEnabled, NULL, 0);
	m_NumPings++;
	
	if (m_NumPings > 5) {
		__debugOutput(F("Dead connection, terminating\n"));

		m_eReadyState = WSRS_CLOSING;
		
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


#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
WebSocket::WebSocket(WiFiClient client) :
#else
WebSocket::WebSocket(EthernetClient client) :
#endif
	m_Client(client), m_eReadyState(WSRS_OPEN), m_NumPings(0), m_bMaskEnabled(false),
	_onOpen(NULL), _onClose(NULL), _onMessage(NULL), _onError(NULL)
{
	memset(m_DataBuffer, '\0', BUFFER_MAX_SIZE);
}

bool WebSocket::_poll(uint16_t maxAttempts, uint8_t time) {
	uint16_t attempts = 0;
	
	while (!m_Client.available() && attempts < maxAttempts) {
		attempts++;
		delay(time);
	}
	
	return attempts < maxAttempts;
}

int WebSocket::_read() {
	while (!m_Client.available())
		delay(1);

	return m_Client.read();
}

void WebSocket::_read(uint8_t *buf, size_t size) {
	while (m_Client.available() < size)
		delay(1);
	
	m_Client.read(buf, size);
}

// 	0                   1                   2                   3
// 	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// 	+-+-+-+-+-------+-+-------------+-------------------------------+
// 	|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
// 	|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
// 	|N|V|V|V|       |S|             |   (if payload len==126/127)   |
// 	| |1|2|3|       |K|             |                               |
// 	+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
// 	|     Extended payload length continued, if payload len == 127  |
// 	+ - - - - - - - - - - - - - - - +-------------------------------+
// 	|                               |Masking-key, if MASK set to 1  |
// 	+-------------------------------+-------------------------------+
// 	| Masking-key (continued)       |          Payload Data         |
// 	+-------------------------------- - - - - - - - - - - - - - - - +
// 	:                     Payload Data continued ...                :
// 	+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// 	|                     Payload Data continued ...                |
// 	+---------------------------------------------------------------+

bool WebSocket::_readHeader(webSocketHeader_t &header) {	
	byte temp[2] = { 0 };
	_read(temp, 2);
	
	//__debugOutput(F("B[0] = %x, B[1] = %x\n"), temp[0], temp[1]);
	
	header.fin = temp[0] & 0x80;
	header.rsv1 = temp[0] & 0x40;
	header.rsv2 = temp[0] & 0x20;
	header.rsv3 = temp[0] & 0x10;
	header.opcode = temp[0] & 0x0F;
	
	if (header.rsv1 || header.rsv2 || header.rsv3) {
		__debugOutput(F("Reserved bits should be empty!\n"));
		__debugOutput(F("RSV1 = %d, RSV2 = %d, RSV3 = %d\n"),
			header.rsv1, header.rsv2, header.rsv3);
		
		close(PROTOCOL_ERROR, NULL, 0, true);
		return false;
	}
	
	header.mask = temp[1] & 0x80;
	header.length = temp[1] & 0x7F;
	
	if (isControlFrame(header.opcode)) {
		if (!header.fin) {
			__debugOutput(F("Control frames must not be fragmented!\n"));
			close(PROTOCOL_ERROR, NULL, 0, true);
			return false;
		}
		
		if (header.length > 125) {
			__debugOutput(F("Control frames max length = 125, here = %d\n"), header.length);
			close(PROTOCOL_ERROR, NULL, 0, true);
			return false;
		}
	}
	
	if (header.length == 126) {
		header.length = _read() << 8;
		header.length |= _read();
	}
	else if (header.length == 127) {
		__debugOutput(F("Unsupported frame size!\n"));
		close(PROTOCOL_ERROR, NULL, 0, true);
		return false;
	}
	
	if (header.length > BUFFER_MAX_SIZE) {
		__debugOutput(F("Unsupported frame size = %d\n"), header.length);
		close(PROTOCOL_ERROR, NULL, 0, true);
		return false;
	}

	if (header.mask)
		_read(header.maskingKey, 4);
	
#ifdef _DUMP_HEADER
	printf(F("RX FRAME : OPCODE=%u, FIN=%s, RSV=%d, PAYLOAD-LEN=%u, MASK="),
		header.opcode, header.fin ? "True" : "False", header.rsv1, header.length);
		
	!header.mask ? printf(F("None\n")) : printf(F("%x%x%x%x\n"),
		header.maskingKey[0], header.maskingKey[1], header.maskingKey[2], header.maskingKey[3]);
#endif
	
	return true;
}

void WebSocket::_readData(const webSocketHeader_t &header, char *payload) {
	memset(payload, 0, header.length + 1);
	
	if (header.mask) {
		for (uint16_t i = 0; i < header.length; i++)
			payload[i] = _read() ^ header.maskingKey[i % 4];
	}
	else
		_read((byte*)payload, header.length);
	
#ifdef _DUMP_FRAME_DATA
	if (header.length) printf(F("%s\n"), payload);
#endif
}

void WebSocket::_handleFrame() {
	if (m_eReadyState == WSRS_CLOSED) return;
	
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
	while (!m_Client.available())
    delay(1);
#else
	if (!m_Client.connected() || !m_Client.available())
		return;
	
	/*if (!m_Client.available())
		return;*/
#endif
	
	// ---
	
	webSocketHeader_t header;
	if (!_readHeader(header)) {
		__debugOutput(F("Failed to read header!\n"));
		return;
	}
	
	char *payload = new char[header.length + 1];
	memset(payload, '\0', sizeof(char) * header.length + 1);
	_readData(header, payload);
	
	// ---
	
	if (m_eReadyState == WSRS_CLOSING && header.opcode != CONNECTION_CLOSE_FRAME) {
		__debugOutput(F("waiting for close frame!\n"));
		delete[] payload;
		return;
	}
	
	switch (header.opcode) {
	case CONTINUATION_FRAME:
		if (!strlen(m_DataBuffer)) {
			close(PROTOCOL_ERROR, NULL, 0, true);
			break;
		}
		
		strncat(m_DataBuffer, payload, header.length);
		if (header.fin) {
			if (_onMessage)
				_onMessage(*this, TEXT, m_DataBuffer, strlen(m_DataBuffer));
			
			memset(m_DataBuffer, '\0', BUFFER_MAX_SIZE);
		}
	break;
	
	case TEXT_FRAME:
		if (strlen(m_DataBuffer)) {
			close(PROTOCOL_ERROR, NULL, 0, true);
			break;
		}
	
		if (header.fin) {			
			if (_onMessage)
				_onMessage(*this, TEXT, payload, header.length);
		}
		else
			strncat(m_DataBuffer, payload, header.length);
		
	break;
	
	case BINARY_FRAME:
		strncat(m_DataBuffer, payload, header.length);
		
		if (header.fin) {			
			if (_onMessage)
				_onMessage(*this, BINARY, m_DataBuffer, strlen(m_DataBuffer));
			
			memset(m_DataBuffer, '\0', BUFFER_MAX_SIZE);
		}
	break;
	
	case CONNECTION_CLOSE_FRAME: {
		uint16_t code = 0;
		const char *reason = NULL;

		if (header.length) {
			for (byte i = 0; i < 2; i++)
				code = (code << 8) + (payload[i] & 0xFF);
			
			if (!isCloseCodeValid(code)) {
				__debugOutput(F("Invalid close code: %d\n"), code);
				close(PROTOCOL_ERROR, NULL, 0, true);
				break;
			}
			
			reason = (header.length) ?
				&(payload[2]) : NULL;
		}
		
		__debugOutput(F("Received close frame: code = %u, reason = %s\n"), code, reason);
		
		if (m_eReadyState == WSRS_OPEN) {
			if (header.length) 
				//_send(header.opcode, true, m_bMaskEnabled, payload, header.length);
				close((eWebSocketCloseEvent)code, reason, header.length - 2, true);
			else
				close(NORMAL_CLOSURE, NULL, 0, true);
		}
		
		if (_onClose)
			_onClose(*this, (eWebSocketCloseEvent)code, reason, header.length - 2);
		
		//terminate();
	}
	break;
	case PING_FRAME:
		_send(PONG_FRAME, true, m_bMaskEnabled, payload, header.length);
	break;
	
	case PONG_FRAME:
		if (m_NumPings > 0) m_NumPings--;
	break;
	
	default:
		__debugOutput(F("Unrecognized frame opcode: %d\n"), header.opcode);
		_triggerError(UNKNOWN_OPCODE);
		close(PROTOCOL_ERROR, NULL, 0, true);
	break;
	}
	
	delete[] payload;
}

void WebSocket::_send(uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length) {
	uint16_t bytesWritten = 0;
	bytesWritten += m_Client.write(opcode | (fin ? 0x80 : 0x0));
	
	if (length <= 125) {
		bytesWritten += m_Client.write((mask ? 0x80 : 0x00) | static_cast<byte>(length));
	}
	else if (length <= 0xFFFF) {
		bytesWritten += m_Client.write((mask ? 0x80 : 0x00) | 126);
    bytesWritten += m_Client.write(static_cast<uint8_t>(length >> 8) & 0xFF);
    bytesWritten += m_Client.write(static_cast<uint8_t>(length & 0xFF));
	}
	else {
		// Too big ...
		return;
	}	
	
#ifdef _DUMP_HEADER
	printf(F("TX FRAME : OPCODE=%u, FIN=%s, RSV=0, PAYLOAD-LEN=%u, MASK="),
		opcode, fin ? "True" : "False", length);
#endif
	
	if (mask) {
		byte maskingKey[4] = { 0 };
		generateMask(maskingKey);
		
#ifdef _DUMP_HEADER
		printf(F("%x%x%x%x\n"),
		maskingKey[0], maskingKey[1], maskingKey[2], maskingKey[3]);
#endif

		bytesWritten += m_Client.write(maskingKey, 4);

		for (uint16_t i = 0; i < length; i++)
			bytesWritten += m_Client.write(static_cast<byte>(data[i] ^ maskingKey[i % 4]));
	}
	else {
#ifdef _DUMP_HEADER
		printf(F("None\n"));
#endif
		
		if (length)
			bytesWritten += m_Client.write((byte*)data, length);
	}
	
#ifdef _DUMP_FRAME_DATA
	if (length) printf(F("%s\n"), data);	
#endif

#ifdef _DEBUG
	printf(F("TX BYTES = %u\n"), bytesWritten);
#endif
	
	m_Client.flush();
	//delay(100);
}

void WebSocket::_triggerError(const eWebSocketError code) {
	if (_onError) _onError(code);
}