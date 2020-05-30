#include "WebSocket.h"

namespace net {

//
// Helper functions:
//

constexpr bool isControlFrame(const uint8_t opcode) {
  return ((opcode == WebSocket::Opcode::PING_FRAME) ||
          (opcode == WebSocket::Opcode::PONG_FRAME) ||
          (opcode == WebSocket::Opcode::CONNECTION_CLOSE_FRAME));
}

// Inspired on "ws", Node.js WebSocket library
// https://github.com/websockets/ws/blob/master/lib/validation.js
constexpr bool isCloseCodeValid(const uint16_t code) {
  return ((code >= WebSocket::CloseCode::NORMAL_CLOSURE &&
            code <= WebSocket::CloseCode::TRY_AGAIN_LATER && code != 1004 &&
            code != WebSocket::CloseCode::NO_STATUS_RECVD &&
            code != WebSocket::CloseCode::ABNORMAL_CLOSURE) ||
          (code >= 3000 && code <= 4999));
}

struct WebSocket::header_t {
  bool fin;
  bool rsv1, rsv2, rsv3;
  uint8_t opcode;

  bool mask;
  byte maskingKey[4]{};
  uint32_t length;
};

//
// Public:
//

WebSocket::~WebSocket() { terminate(); }

void WebSocket::close(
  const CloseCode &code, bool instant, const char *reason, uint16_t length) {
  if (m_readyState != ReadyState::OPEN) return;

  // #TODO handle too big length (above 123 characters)

  m_readyState = ReadyState::CLOSING;
  char buffer[128]{ static_cast<char>((code >> 8) & 0xFF),
    static_cast<char>(code & 0xFF) };

  if (length) memcpy(&buffer[2], reason, sizeof(char) * length);
  _send(CONNECTION_CLOSE_FRAME, true, m_maskEnabled, buffer, 2 + length);

  if (instant) {
    terminate();
    if (_onClose) _onClose(*this, code, reason, length);
  }
}

void WebSocket::terminate() {
  m_client.stop();
  m_readyState = ReadyState::CLOSED;
  m_currentOffset = 0;
  m_tbcOpcode = 1;
}

const WebSocket::ReadyState &WebSocket::getReadyState() const {
  return m_readyState;
}

bool WebSocket::isAlive() {
  return m_client.connected() && m_readyState != ReadyState::CLOSED;
}

#if PLATFORM_ARCH != PLATFORM_ARCHITECTURE_ESP8266
IPAddress WebSocket::getRemoteIP() { return _REMOTE_IP(m_client); }
#endif

void WebSocket::send(
  const WebSocket::DataType dataType, const char *message, uint16_t length) {
  if (m_readyState != WebSocket::ReadyState::OPEN) return;
  _send(dataType == WebSocket::DataType::TEXT ? TEXT_FRAME : BINARY_FRAME, true,
    m_maskEnabled, message, length);
}

void WebSocket::ping(const char *payload, uint16_t length) {
  if (m_readyState != WebSocket::ReadyState::OPEN) return;
  _send(PING_FRAME, true, m_maskEnabled, payload, length);
}

void WebSocket::onClose(const onCloseCallback &callback) {
  _onClose = callback;
}

void WebSocket::onMessage(const onMessageCallback &callback) {
  _onMessage = callback;
}

//
//
//

WebSocket::WebSocket() : m_maskEnabled(true) {}
WebSocket::WebSocket(const NetClient &client)
  : m_client(client), m_maskEnabled(false), m_readyState(ReadyState::OPEN) {}

int WebSocket::_read() {
  const uint32_t timeout = millis() + kTimeoutInterval;
  while (!m_client.available() && millis() < timeout) {
    delay(1);
  }

  if (millis() > timeout) {
    close(PROTOCOL_ERROR, true);
    return -1;
  }

  return m_client.read();
}

bool WebSocket::_read(uint8_t *buffer, size_t size) {
  int16_t bite = -1;
  size_t counter = 0;

  do {
    if ((bite = _read()) == -1) return false;

    buffer[counter++] = bite;
  } while (counter != size);

  return true;
}

void WebSocket::_handleFrame() {
  if (m_readyState == ReadyState::CLOSED) return;

  header_t header;
  if (!_readHeader(header)) return;

  char *payload = nullptr;
  if (header.length) {
    payload = new char[header.length + 1]{};
    if (!_readData(header, payload)) {
      SAFE_DELETE_ARRAY(payload);
      return;
    }
  }

  // ---

  if (m_readyState == ReadyState::CLOSING &&
      header.opcode != CONNECTION_CLOSE_FRAME) {
    __debugOutput(F("waiting for close frame!\n"));
    SAFE_DELETE_ARRAY(payload);
    return;
  }

  switch (header.opcode) {
  case Opcode::CONTINUATION_FRAME: {
    if (m_tbcOpcode == -1) {
      close(PROTOCOL_ERROR, true);
      break;
    }

    memcpy(&m_dataBuffer[m_currentOffset], payload, header.length);

    if (header.fin) {
      if (_onMessage) {
        DataType dataType = m_tbcOpcode == WebSocket::Opcode::TEXT_FRAME
                              ? DataType::TEXT
                              : DataType::BINARY;
        _onMessage(
          *this, dataType, m_dataBuffer, m_currentOffset + header.length);
      }

      memset(m_dataBuffer, '\0', sizeof(char) * kBufferMaxSize);
      m_currentOffset = 0;
      m_tbcOpcode = -1;
    } else {
      m_currentOffset += header.length;
    }

    break;
  }
  case Opcode::TEXT_FRAME:
  case Opcode::BINARY_FRAME: {
    if (m_currentOffset > 0) {
      close(PROTOCOL_ERROR, true);
      break;
    }

    if (header.fin) {
      if (_onMessage) {
        const auto dataType = header.opcode == Opcode::TEXT_FRAME
                                ? DataType::TEXT
                                : DataType::BINARY;

        _onMessage(*this, dataType, payload, header.length);
      }

    } else {
      memcpy(&m_dataBuffer[m_currentOffset], payload, header.length);
      m_currentOffset += header.length;
      m_tbcOpcode = header.opcode;
    }

    break;
  }
  case Opcode::CONNECTION_CLOSE_FRAME: {
    uint16_t code = 0;
    const char *reason = nullptr;

    if (header.length) {
      for (byte i = 0; i < 2; i++)
        code = (code << 8) + (payload[i] & 0xFF);

      if (!isCloseCodeValid(code)) {
        close(PROTOCOL_ERROR, true);
        break;
      }

      reason = (header.length) ? &(payload[2]) : nullptr;
    }

    __debugOutput(F("Received close frame: code = %u, reason = %s\n"), code,
      header.length ? reason : " ");

    if (m_readyState == ReadyState::OPEN) {
      if (header.length)
        close(static_cast<CloseCode>(code), true, reason, header.length - 2);
      else
        close(NORMAL_CLOSURE, true);
    } else {
      __debugOutput(F("this should never happen! wtf?\n"));
    }

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
    __debugOutput(F("Unrecognized frame opcode: %d\n"), header.opcode);
    close(PROTOCOL_ERROR, true);
    break;
  }
  }

  SAFE_DELETE_ARRAY(payload);
}

bool WebSocket::_readHeader(header_t &header) {
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

  int16_t bite = -1;

  byte temp[2]{};
  if (!_read(temp, 2)) return false;
  //__debugOutput(F("B[0] = %x, B[1] = %x\n"), temp[0], temp[1]);

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
        F("Control frames max length = 125, here = %d\n"), header.length);

      close(PROTOCOL_ERROR, true);
      return false;
    }
  }

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
    __debugOutput(F("Unsupported frame size = %d\n"), header.length);

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

bool WebSocket::_readData(const header_t &header, char *payload) {
  int16_t bite = -1;

  if (header.mask) {
    for (uint16_t i = 0; i < header.length; i++) {
      if ((bite = _read()) == -1) return false;

      payload[i] = bite ^ header.maskingKey[i % 4];
    }
  } else {
    if (!_read(reinterpret_cast<byte *>(payload), header.length)) {
      return false;
    }
  }

#ifdef _DUMP_FRAME_DATA
  if (header.length) printf(F("%s\n"), payload);
#endif

  return true;
}

void WebSocket::_send(
  uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length) {
  uint16_t bytesWritten = 0;
  bytesWritten += m_client.write(opcode | (fin ? 0x80 : 0x00));

  if (length <= 125) {
    bytesWritten +=
      m_client.write((mask ? 0x80 : 0x00) | static_cast<byte>(length));
  } else if (length <= 0xFFFF) {
    bytesWritten += m_client.write((mask ? 0x80 : 0x00) | 126);
    bytesWritten += m_client.write(static_cast<uint8_t>(length >> 8) & 0xFF);
    bytesWritten += m_client.write(static_cast<uint8_t>(length & 0xFF));
  } else
    return; // too big ...

#ifdef _DUMP_HEADER
  printf(F("TX FRAME : OPCODE=%u, FIN=%s, RSV=0, PAYLOAD-LEN=%u, MASK="),
    opcode, fin ? "True" : "False", length);
#endif

  if (mask) {
    byte maskingKey[4]{};
    generateMask(maskingKey);

#ifdef _DUMP_HEADER
    printf(F("%x%x%x%x\n"), maskingKey[0], maskingKey[1], maskingKey[2],
      maskingKey[3]);
#endif

    bytesWritten += m_client.write(maskingKey, 4);

    for (uint16_t i = 0; i < length; i++)
      bytesWritten +=
        m_client.write(static_cast<byte>(data[i] ^ maskingKey[i % 4]));
  } else {
#ifdef _DUMP_HEADER
    printf(F("None\n"));
#endif

    if (length) {
      bytesWritten +=
        m_client.write(reinterpret_cast<const byte *>(data), length);
    }
  }

#ifdef _DUMP_FRAME_DATA
  if (length) printf(F("%s\n"), data);
#endif

#ifdef _DUMP_HEADER
  printf(F("TX BYTES = %u\n"), bytesWritten);
#endif
}

} // namespace net