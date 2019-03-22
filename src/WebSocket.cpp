#include "WebSocket.h"
#include "utility.h"

namespace net {

void generateMask(byte *output) {
	randomSeed(analogRead(0));
	
	for (uint8_t i = 0; i < 4; i++)
		output[i] = random(0xFF);
}

bool isControlFrame(uint8_t opcode) {
	return (
		(opcode == PING_FRAME) ||
		(opcode == PONG_FRAME) ||
		(opcode == CONNECTION_CLOSE_FRAME)
	);
}

// Ripped from "ws", Node.js WebSocket library
// https://github.com/websockets/ws/blob/master/lib/validation.js
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

WebSocket::WebSocket()
	: readyState_(WebSocketReadyState::CLOSED), maskEnabled_(true),
		onClose_(nullptr), onMessage_(nullptr), onError_(nullptr)
{
	memset(dataBuffer_, '\0', BUFFER_MAX_SIZE);
}

WebSocket::~WebSocket() {
	terminate();
}

void WebSocket::close(const WebSocketCloseCode code, bool instant, const char *reason, uint16_t length) {
	if (readyState_ != WebSocketReadyState::OPEN) return;
	
	readyState_ = WebSocketReadyState::CLOSING;
	
	char buffer[128] = {
		static_cast<char>((code >> 8) & 0xFF),
		static_cast<char>(code & 0xFF),
		'\0',
	};
	
	if (length)
		memcpy(&buffer[2], reason, sizeof(char) * length);
	
	_send(CONNECTION_CLOSE_FRAME, true, maskEnabled_, buffer, 2 + length);
	
	if (instant) {
		terminate();
		
		if (onClose_)
			onClose_(*this, code, reason, length);
	}
}

bool WebSocket::terminate() {
	//client_.flush();
	
	readyState_ = WebSocketReadyState::CLOSED;
	memset(dataBuffer_, '\0', BUFFER_MAX_SIZE);

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266
	delay(100);
#endif
	
	client_.stop();	
	return true;
}

void WebSocket::send(const WebSocketDataType dataType, const char *message, uint16_t length) {
	if (readyState_ != WebSocketReadyState::OPEN) return;
	
	_send(dataType == TEXT ? TEXT_FRAME : BINARY_FRAME,
		true, maskEnabled_, message, length);
}

void WebSocket::ping(const char *payload, uint16_t length) {
	if (readyState_ != WebSocketReadyState::OPEN) return; 
	
	_send(PING_FRAME, true, maskEnabled_, payload, length);
}

const WebSocketReadyState &WebSocket::getReadyState() const {
	return readyState_;
}

bool WebSocket::isAlive() {
	bool alive = client_.connected();
	alive &= readyState_ != WebSocketReadyState::CLOSED;

	return alive;
}

IPAddress WebSocket::getRemoteIP() {
	return _REMOTE_IP(client_);
}

//
// Private functions:
//

WebSocket::WebSocket(NetClient client)
	: client_(client), readyState_(WebSocketReadyState::OPEN), maskEnabled_(false),
		onClose_(nullptr), onMessage_(nullptr), onError_(nullptr)
{
	memset(dataBuffer_, '\0', BUFFER_MAX_SIZE);
}

bool WebSocket::_waitForResponse(uint16_t maxAttempts, uint8_t time) {
	uint16_t attempts = 0;
	
	while (!client_.available() && attempts < maxAttempts) {
		attempts++;
		delay(time);
	}
	
	return attempts < maxAttempts;
}

int WebSocket::_read() {
	unsigned long timeout = millis() + TIMEOUT_INTERVAL;
	while (!client_.available() && millis() < timeout) {
		delay(1); //__debugOutput(F("."));
	}
	
	if (millis() > timeout) {
		close(PROTOCOL_ERROR, true);
		return -1;
	}
	
	return client_.read();
}

bool WebSocket::_read(uint8_t *buffer, size_t size) {
	//__debugOutput(F("attempt to read %d bytes, for now got: %d\n"), size, client_.available());
	
	int16_t bite = -1;
	size_t counter = 0;	
	
	do {
		if ((bite = _read()) == -1)
			return false;
		
		buffer[counter++] = bite;
	} while (counter != size);
	
	//__debugOutput(F("done reading\n"));
	return true;
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
	int16_t bite = -1;
	
	byte temp[2] = { 0 };
	if (!_read(temp, 2)) return false;
	
	//__debugOutput(F("B[0] = %x, B[1] = %x\n"), temp[0], temp[1]);
	
	header.fin = temp[0] & 0x80;
	header.rsv1 = temp[0] & 0x40;
	header.rsv2 = temp[0] & 0x20;
	header.rsv3 = temp[0] & 0x10;
	header.opcode = temp[0] & 0x0F;
	
	if (header.rsv1 || header.rsv2 || header.rsv3) {
		__debugOutput(F("Reserved bits should be empty!\n"));
		__debugOutput(F("RSV1 = %d, RSV2 = %d, RSV3 = %d\n"), header.rsv1, header.rsv2, header.rsv3);
		
		close(PROTOCOL_ERROR, true);
		return false;
	}
	
	header.mask = temp[1] & 0x80;
	header.length = temp[1] & 0x7F;
	
	if (isControlFrame(header.opcode)) {
		if (!header.fin) {
			__debugOutput(F("Control frames must not be fragmented!\n"));
			
			close(PROTOCOL_ERROR, true);
			return false;
		}
		
		if (header.length > 125) {
			__debugOutput(F("Control frames max length = 125, here = %d\n"), header.length);
			
			close(PROTOCOL_ERROR, true);
			return false;
		}
	}
	
	if (header.length == 126) {
		if ((bite = _read()) == -1)
			return false;
		
		header.length = bite << 8;
		
		if ((bite = _read()) == -1)
			return false;
		
		header.length |= bite;
	}
	else if (header.length == 127) {
		__debugOutput(F("Unsupported frame size!\n"));
		
		close(MESSAGE_TOO_BIG, true);
		return false;
	}
	
	if (header.length > BUFFER_MAX_SIZE) {
		__debugOutput(F("Unsupported frame size = %d\n"), header.length);
		
		close(MESSAGE_TOO_BIG, true);
		return false;
	}

	if (header.mask)
		if (!_read(header.maskingKey, 4))
			return false;
	
#ifdef _DUMP_HEADER
	printf(F("RX FRAME : OPCODE=%u, FIN=%s, RSV=%d, PAYLOAD-LEN=%u, MASK="),
		header.opcode, header.fin ? "True" : "False", header.rsv1, header.length);
		
	!header.mask ? printf(F("None\n")) : printf(F("%x%x%x%x\n"),
		header.maskingKey[0], header.maskingKey[1], header.maskingKey[2], header.maskingKey[3]);
#endif
	
	return true;
}

bool WebSocket::_readData(const webSocketHeader_t &header, char *payload) {
	memset(payload, '\0', header.length + 1);
	
	int16_t bite = -1;
	
	if (header.mask) {
		for (uint16_t i = 0; i < header.length; i++) {
			if ((bite = _read()) == -1)
				return false;
			
			payload[i] = bite ^ header.maskingKey[i % 4];
		}
	}
	else {
		if (!_read(reinterpret_cast<byte *>(payload), header.length)) {
			return false;
		}
	}
	
#ifdef _DUMP_FRAME_DATA
	if (header.length) printf(F("%s\n"), payload);
#endif

	return true;
}

void WebSocket::_send(uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length) {
	uint16_t bytesWritten = 0;
	bytesWritten += client_.write(opcode | (fin ? 0x80 : 0x00));
	
	if (length <= 125) {
		bytesWritten += client_.write((mask ? 0x80 : 0x00) | static_cast<byte>(length));
	}
	else if (length <= 0xFFFF) {
		bytesWritten += client_.write((mask ? 0x80 : 0x00) | 126);
    bytesWritten += client_.write(static_cast<uint8_t>(length >> 8) & 0xFF);
    bytesWritten += client_.write(static_cast<uint8_t>(length & 0xFF));
	}
	else
		return; // too big ...
	
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

		bytesWritten += client_.write(maskingKey, 4);

		for (uint16_t i = 0; i < length; i++)
			bytesWritten += client_.write(static_cast<byte>(data[i] ^ maskingKey[i % 4]));
	}
	else {
#ifdef _DUMP_HEADER
		printf(F("None\n"));
#endif
		
		if (length)
			bytesWritten += client_.write(reinterpret_cast<const byte *>(data), length);
	}
	
#ifdef _DUMP_FRAME_DATA
	if (length) printf(F("%s\n"), data);	
#endif

#ifdef _DUMP_HEADER
	printf(F("TX BYTES = %u\n"), bytesWritten);
#endif
	
	//client_.flush();
}

void WebSocket::_handleFrame() {
	if (readyState_ == WebSocketReadyState::CLOSED) return;
	
	webSocketHeader_t header;
	if (!_readHeader(header)) return;
	
	char *payload = NULL;

	if (header.length) {
		payload = new char[header.length + 1];
		memset(payload, '\0', sizeof(char) * header.length + 1);
		
		if (!_readData(header, payload)) {
			delete[] payload;
			return;
		}
	}
	
	// ---
	
	if (readyState_ == WebSocketReadyState::CLOSING && header.opcode != CONNECTION_CLOSE_FRAME) {
		__debugOutput(F("waiting for close frame!\n"));
		SAFE_DELETE_ARR(payload);		
		return;
	}
	
	switch (header.opcode) {
		case CONTINUATION_FRAME: {
			if (!strlen(dataBuffer_)) {	
				close(PROTOCOL_ERROR, true);
				break;
			}
			
			strncat(dataBuffer_, payload, header.length);
			if (header.fin) {
				if (onMessage_)
					onMessage_(*this, TEXT, dataBuffer_, strlen(dataBuffer_));
				
				memset(dataBuffer_, '\0', BUFFER_MAX_SIZE);
			}
			
			break;
		}
		case TEXT_FRAME: {
			if (strlen(dataBuffer_)) {
				close(PROTOCOL_ERROR, true);
				break;
			}
		
			if (header.fin) {			
				if (onMessage_)
					onMessage_(*this, TEXT, payload, header.length);
			}
			else
				strncat(dataBuffer_, payload, header.length);
			
			break;
		}
		case BINARY_FRAME: {
			strncat(dataBuffer_, payload, header.length);
			
			if (header.fin) {			
				if (onMessage_)
					onMessage_(*this, BINARY, dataBuffer_, strlen(dataBuffer_));
				
				memset(dataBuffer_, '\0', BUFFER_MAX_SIZE);
			}
			
			break;
		}
		case CONNECTION_CLOSE_FRAME: {
			uint16_t code = 0;
			const char *reason = NULL;

			if (header.length) {
				for (byte i = 0; i < 2; i++)
					code = (code << 8) + (payload[i] & 0xFF);
				
				if (!isCloseCodeValid(code)) {
					__debugOutput(F("Invalid close code: %d\n"), code);
					close(PROTOCOL_ERROR, true);
					break;
				}
				
				reason = (header.length) ? &(payload[2]) : NULL;
			}
			
			__debugOutput(F("Received close frame: code = %u, reason = %s\n"), code, header.length ? reason : " ");
			
			if (readyState_ == WebSocketReadyState::OPEN) {
				if (header.length) 
					close(static_cast<WebSocketCloseCode>(code), true, reason, header.length - 2);
				else 
					close(NORMAL_CLOSURE, true);
			}
			else {
				// should never happen
			}
			
			break;
		}
		case PING_FRAME: {
			_send(PONG_FRAME, true, maskEnabled_, payload, header.length);
			break;
		}		
		case PONG_FRAME: {
			break;
		}
		default: {
			__debugOutput(F("Unrecognized frame opcode: %d\n"), header.opcode);
			close(PROTOCOL_ERROR, true);
			break;
		}
	}
	
	SAFE_DELETE_ARR(payload);
}

void WebSocket::_triggerError(const WebSocketError code) {
	if (onError_) onError_(code);
}

};