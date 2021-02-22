#include "WebSocket.h"
#include "base64/Base64.h"
#include "CryptoLegacy/SHA1.h"

namespace net {

//
// Helper functions:
//

constexpr bool isControlFrame(uint8_t opcode) {
  return ((opcode == WebSocket::Opcode::PING_FRAME) ||
          (opcode == WebSocket::Opcode::PONG_FRAME) ||
          (opcode == WebSocket::Opcode::CONNECTION_CLOSE_FRAME));
}

// Inspired on "ws", Node.js WebSocket library
// https://github.com/websockets/ws/blob/master/lib/validation.js
constexpr bool isCloseCodeValid(uint16_t code) {
  return ((code >= WebSocket::CloseCode::NORMAL_CLOSURE &&
            code <= WebSocket::CloseCode::TRY_AGAIN_LATER && code != 1004 &&
            code != WebSocket::CloseCode::NO_STATUS_RECVD &&
            code != WebSocket::CloseCode::ABNORMAL_CLOSURE) ||
          (code >= 3000 && code <= 4999));
}

void generateSecKey(char output[]) {
  constexpr auto kLength{ 16 };
  char temp[kLength + 1]{};

  randomSeed(analogRead(0));
  for (byte i = 0; i < kLength; ++i)
    temp[i] = static_cast<char>(random(0xFF));

  base64_encode(output, temp, kLength);
}

bool encodeSecKey(char output[], const char *key) {
  constexpr auto kSecKeyLength{ 24 };
  constexpr char kMagicString[]{ "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" };
  constexpr auto kMagicStringLenght{ sizeof(kMagicString) - 1 };
  
  constexpr auto kBufferLength{ kSecKeyLength + kMagicStringLenght };
  char buffer[kBufferLength + 1]{};
  memcpy(&buffer[0], key, kSecKeyLength);
  memcpy(&buffer[kSecKeyLength], kMagicString, kMagicStringLenght);

  SHA1 sha1;
  sha1.update(buffer, kBufferLength);
  sha1.finalize(buffer, 20);
  base64_encode(output, buffer, 20);
  return true;
}

/** @param[out] output Array of 4 elements (without NULL). */
void generateMask(char output[]) {
  randomSeed(analogRead(0));
  for (byte i = 0; i < 4; ++i)
    output[i] = static_cast<char>(random(0xFF));
}

/**
 * @see https://github.com/websockets/utf-8-validate/blob/master/src/validation.c
 */
bool isValidUTF8(const byte *s, size_t length) {
  const uint8_t *end{ s + length };

  //
  // This code has been taken from utf8_check.c which was developed by
  // Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>.
  //
  // For original code / licensing please refer to
  // https://www.cl.cam.ac.uk/%7Emgk25/ucs/utf8_check.c
  //
  while (s < end) {
    if (*s < 0x80) { // 0xxxxxxx
      s++;
    } else if ((s[0] & 0xe0) == 0xc0) { // 110xxxxx 10xxxxxx
      if (s + 1 == end || (s[1] & 0xc0) != 0x80 ||
          (s[0] & 0xfe) == 0xc0 // overlong
      ) {
        break;
      } else {
        s += 2;
      }
    } else if ((s[0] & 0xf0) == 0xe0) { // 1110xxxx 10xxxxxx 10xxxxxx
      if (s + 2 >= end || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
          (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||
          (s[0] == 0xed && (s[1] & 0xe0) == 0xa0)) {
        break;
      } else {
        s += 3;
      }
    } else if ((s[0] & 0xf8) == 0xf0) { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      if (s + 3 >= end || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
          (s[3] & 0xc0) != 0x80 ||
          (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||   // overlong
          (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4 // > U+10FFFF
      ) {
        break;
      } else {
        s += 4;
      }
    } else {
      break;
    }
  }

  return s == end;
}

//
// Helper structs:
//

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

/** @cond */
struct WebSocket::header_t {
  bool fin;
  bool rsv1, rsv2, rsv3;
  uint8_t opcode;
  bool mask;
  char maskingKey[4]{};
  uint32_t length;
};
/** @endcond */

//
// WebSocket class implementation (public):
//

WebSocket::~WebSocket() { terminate(); }

void WebSocket::close(
  const CloseCode &code, bool instant, const char *reason, uint16_t length) {
  if (m_readyState != ReadyState::OPEN) return;
  if (length > 123) {
    // #TODO Trigger error ...
    return;
  }

  m_readyState = ReadyState::CLOSING;
  char buffer[128]{
    static_cast<char>((code >> 8) & 0xFF),
    static_cast<char>(code & 0xFF)
  };

  if (length) memcpy(&buffer[2], reason, length);
  _send(CONNECTION_CLOSE_FRAME, true, m_maskEnabled, buffer, 2 + length);

  if (instant) {
    terminate();
    if (_onClose) _onClose(*this, code, reason, length);
  }
}

void WebSocket::terminate() {
  m_client.stop();
  m_readyState = ReadyState::CLOSED;
  _clearDataBuffer();
}

const WebSocket::ReadyState &WebSocket::getReadyState() const {
  return m_readyState;
}

bool WebSocket::isAlive() const {
  return m_client.connected() && m_readyState != ReadyState::CLOSED;
}

IPAddress WebSocket::getRemoteIP() const { return fetchRemoteIp(m_client); }

void WebSocket::send(
  const WebSocket::DataType dataType, const char *message, uint16_t length) {
  if (m_readyState != ReadyState::OPEN) {
    // #TODO Trigger error ...
    return;
  }

  _send(dataType == DataType::TEXT ? TEXT_FRAME : BINARY_FRAME, true,
    m_maskEnabled, message, length);
}

void WebSocket::ping(const char *payload, uint16_t length) {
  if (m_readyState != ReadyState::OPEN) {
    // #TODO Trigger error ...
    return;
  }

  _send(PING_FRAME, true, m_maskEnabled, payload, length);
}

void WebSocket::onClose(const onCloseCallback &callback) {
  _onClose = callback;
}

void WebSocket::onMessage(const onMessageCallback &callback) {
  _onMessage = callback;
}

//
// Protected:
//

WebSocket::WebSocket()
  : m_readyState(ReadyState::CLOSED), m_maskEnabled(true) {
  // All frames sent from client to server are masked.
}
WebSocket::WebSocket(const NetClient &client)
  : m_client(client), m_readyState(ReadyState::OPEN), m_maskEnabled(false) {}

int32_t WebSocket::_read() {
  return m_client.read();
}

bool WebSocket::_read(char *buffer, size_t size, size_t offset) {
  size_t counter{ 0 };
  int32_t bite{ -1 };
  do {
    if ((bite = _read()) == -1) return false;
    buffer[offset + (counter++)] = bite;
  } while (counter != size);

  return true;
}

void WebSocket::_send(
  uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length) {
  uint16_t bytesWritten{ 0 };
  bytesWritten += m_client.write(opcode | (fin ? 0x80 : 0x00));

  if (length <= 125) {
    bytesWritten +=
      m_client.write((mask ? 0x80 : 0x00) | static_cast<char>(length));
  } else if (length <= 0xFFFF) {
    bytesWritten += m_client.write((mask ? 0x80 : 0x00) | 126);
    bytesWritten += m_client.write(static_cast<char>(length >> 8) & 0xFF);
    bytesWritten += m_client.write(static_cast<char>(length & 0xFF));
  } else
    return; // too big ...

#ifdef _DUMP_HEADER
  printf(F("TX FRAME : OPCODE=%u, FIN=%s, RSV=0, PAYLOAD-LEN=%u, MASK="),
    opcode, fin ? "True" : "False", length);
#endif

  if (mask) {
    char maskingKey[4]{};
    generateMask(maskingKey);

#ifdef _DUMP_HEADER
    printf(F("%x%x%x%x\n"), maskingKey[0], maskingKey[1], maskingKey[2],
      maskingKey[3]);
#endif

    bytesWritten += m_client.write(maskingKey, 4);
    for (uint16_t i = 0; i < length; ++i)
      bytesWritten +=
        m_client.write(static_cast<char>(data[i] ^ maskingKey[i % 4]));
  } else {
#ifdef _DUMP_HEADER
    printf(F("None\n"));
#endif

    if (length) {
      bytesWritten +=
        m_client.write(reinterpret_cast<const char *>(data), length);
    }
  }

#ifdef _DUMP_FRAME_DATA
  if (length) printf(F("%s\n"), data);
#endif

#ifdef _DUMP_HEADER
  printf(F("TX BYTES = %u\n"), bytesWritten);
#endif
}

void WebSocket::_readFrame() {
  if (m_readyState == ReadyState::CLOSED) return;

  header_t header;
  if (!_readHeader(header)) return;
  
  bool usingTempBuffer = isControlFrame(header.opcode);
  char *payload{ nullptr };
  size_t offset{ 0 };

  if (usingTempBuffer) {
    payload = new char[header.length + 1]{};
  } else {
    payload = m_dataBuffer;
    offset = m_currentOffset;

    if (header.length + offset >= kBufferMaxSize)
      return close(CloseCode::MESSAGE_TOO_BIG, true);
  }

  if (header.length > 0) {
    if (!_readData(header, payload, offset)) {
      if (usingTempBuffer) SAFE_DELETE_ARRAY(payload);
      return;
    }
  }

  switch (header.opcode) {
  case Opcode::CONTINUATION_FRAME: {
    _handleContinuationFrame(header);
    break;
  }
  case Opcode::TEXT_FRAME:
  case Opcode::BINARY_FRAME: {
    _handleDataFrame(header);
    break;
  }
  case Opcode::CONNECTION_CLOSE_FRAME: {
    _handleCloseFrame(header, payload);
    break;
  }
  case Opcode::PING_FRAME: {
    _send(PONG_FRAME, true, m_maskEnabled, payload, header.length);
    break;
  }
  case Opcode::PONG_FRAME: {
    break;
  }
  default: {
    __debugOutput(F("Unrecognized frame opcode: %u\n"), header.opcode);
    close(PROTOCOL_ERROR, true);
    break;
  }
  }

  if (usingTempBuffer) SAFE_DELETE_ARRAY(payload);
}

bool WebSocket::_readHeader(header_t &header) {
  char temp[2]{};
  if (!_read(temp, 2)) return false;

  header.fin = temp[0] & 0x80;
  header.rsv1 = temp[0] & 0x40;
  header.rsv2 = temp[0] & 0x20;
  header.rsv3 = temp[0] & 0x10;
  header.opcode = temp[0] & 0x0F;

  if (header.rsv1 || header.rsv2 || header.rsv3) {
    __debugOutput(F("Reserved bits should be empty!\n"));
    __debugOutput(F("RSV1 = %d, RSV2 = %d, RSV3 = %d\n"), header.rsv1,
      header.rsv2, header.rsv3);

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
      __debugOutput(
        F("Control frames max length = 125, here = %u\n"), header.length);

      close(PROTOCOL_ERROR, true);
      return false;
    }
  }

  int32_t bite{ -1 };
  if (header.length == 126) {
    if ((bite = _read()) == -1) return false;
    header.length = bite << 8;
    if ((bite = _read()) == -1) return false;

    header.length |= bite;
  } else if (header.length == 127) {
    __debugOutput(F("Unsupported frame size!\n"));

    close(MESSAGE_TOO_BIG, true);
    return false;
  }

  if (header.length > kBufferMaxSize) {
    __debugOutput(F("Unsupported frame size = %u\n"), header.length);

    close(MESSAGE_TOO_BIG, true);
    return false;
  }

  if (header.mask)
    if (!_read(header.maskingKey, 4)) return false;

#ifdef _DUMP_HEADER
  printf(F("RX FRAME : OPCODE=%u, FIN=%s, RSV=%d, PAYLOAD-LEN=%u, MASK="),
    header.opcode, header.fin ? "True" : "False", header.rsv1, header.length);

  !header.mask
    ? printf(F("None\n"))
    : printf(F("%x%x%x%x\n"), header.maskingKey[0], header.maskingKey[1],
        header.maskingKey[2], header.maskingKey[3]);
#endif

  return true;
}

bool WebSocket::_readData(const header_t &header, char *payload, size_t offset) {
  int32_t bite{ -1 };

  if (header.mask) {
    for (uint32_t i = 0; i < header.length; ++i) {
      if ((bite = _read()) == -1) return false;
      payload[offset + i] = bite ^ header.maskingKey[i % 4];
    }
  } else {
    if (!_read(payload, header.length, offset)) {
      return false;
    }
  }

#ifdef _DUMP_FRAME_DATA
  if (header.length) printf(F("%s\n"), payload);
#endif

  return true;
}

void WebSocket::_clearDataBuffer() {
  memset(m_dataBuffer, '\0', kBufferMaxSize);
  m_currentOffset = 0;
  m_tbcOpcode = -1;
}

void WebSocket::_handleContinuationFrame(const header_t &header) {
  if (m_tbcOpcode == -1) return close(PROTOCOL_ERROR, true);

  if (header.fin) {
    auto totalLength{ m_currentOffset + header.length };
    const auto dataType{ m_tbcOpcode == Opcode::TEXT_FRAME ? DataType::TEXT
                                                           : DataType::BINARY };
    if (dataType == DataType::TEXT) {
      if (!isValidUTF8(
            reinterpret_cast<const byte *>(m_dataBuffer), totalLength))
        return close(INVALID_FRAME_PAYLOAD_DATA, true);
    }

    if (_onMessage) {
      _onMessage(*this, dataType, m_dataBuffer, totalLength);
    }
    _clearDataBuffer();
  } else {
    m_currentOffset += header.length;
  }
}

void WebSocket::_handleDataFrame(const header_t &header) {
  if (m_currentOffset > 0) return close(PROTOCOL_ERROR, true);

  if (header.fin) {
    const auto dataType{
      header.opcode == Opcode::TEXT_FRAME ? DataType::TEXT : DataType::BINARY
    };

    if (dataType == DataType::TEXT) {
      if (!isValidUTF8(
            reinterpret_cast<const byte *>(m_dataBuffer), header.length))
        return close(INVALID_FRAME_PAYLOAD_DATA, true);
    }

    if (_onMessage) {
      _onMessage(*this, dataType, m_dataBuffer, header.length);
    }
    _clearDataBuffer();
  } else {
    m_currentOffset += header.length;
    m_tbcOpcode = header.opcode;
  }
}

void WebSocket::_handleCloseFrame(const header_t &header, const char *payload) {
  uint16_t code{ NORMAL_CLOSURE };
  const char *reason{ nullptr };
  uint16_t reasonLength{ 0 };

  if (header.length > 0) {    
    for (byte i = 0; i < 2; ++i)
      code = (code << 8) + (payload[i] & 0xFF);

    if (!isCloseCodeValid(code)) return close(PROTOCOL_ERROR, true);

    reasonLength = header.length - 2;
    reason = &payload[2];
    if (!isValidUTF8(reinterpret_cast<const byte *>(reason), reasonLength))
      return close(PROTOCOL_ERROR, true);
  }

  __debugOutput(F("Received close frame: code = %u, reason = %s\n"), code,
    header.length ? reason : " ");

  if (m_readyState == ReadyState::OPEN)
    close(static_cast<CloseCode>(code), true, reason, reasonLength);
}

} // namespace net