#pragma once

#include "utility.h"

namespace net {

constexpr auto kValidUpgradeHeader = 0x01;
constexpr auto kValidConnectionHeader = 0x02;
constexpr auto kValidSecKey = 0x04;
constexpr auto kValidVersion = 0x08;

enum class WebSocketError {
  CONNECTION_ERROR,
  CONNECTION_REFUSED,

  //
  // Client errors:
  //

  BAD_REQUEST = 400,
  REQUEST_TIMEOUT = 408,
  UPGRADE_REQUIRED = 426,

  //
  // Server errors:
  //

  INTERNAL_SERVER_ERROR = 500,
  NOT_IMPLEMENTED = 501,
  SERVICE_UNAVAILABLE = 503
};

class WebSocket {
  friend class WebSocketServer;
  struct header_t;

public:
  enum class ReadyState { CONNECTING = 0, OPEN, CLOSING, CLOSED };
  enum class DataType { TEXT, BINARY };
  enum Opcode {
    CONTINUATION_FRAME = 0x00,

    //
    // Data Frames (non-control):
    //

    TEXT_FRAME = 0x01,
    BINARY_FRAME = 0x02,

    // Reserved non-control frames:
    // 0x03
    // 0x04
    // 0x05
    // 0x06
    // 0x07

    //
    // Control frames:
    //

    CONNECTION_CLOSE_FRAME = 0x08,
    PING_FRAME = 0x09,
    PONG_FRAME = 0x0A

    // Reserved control opcodes:
    // 0x0B
    // 0x0C
    // 0x0D
    // 0x0E
    // 0x0F
  };

  enum CloseCode {
    // 0-999 : Reserved and not used.

    NORMAL_CLOSURE = 1000,
    GOING_AWAY,
    PROTOCOL_ERROR,
    UNSUPPORTED_DATA,

    // 1004 : Reserved.

    NO_STATUS_RECVD = 1005,
    ABNORMAL_CLOSURE,
    INVALID_FRAME_PAYLOAD_DATA,
    POLICY_VIOLATION,
    MESSAGE_TOO_BIG,
    MISSING_EXTENSION,
    INTERNAL_ERROR,
    SERVICE_RESTART,
    TRY_AGAIN_LATER,
    BAD_GATEWAY,
    TLS_HANDSHAKE

    // 1016-1999 : Reserved for future use by the WebSocket standard.
    // 3000-3999 : Available for use by libraries and frameworks.
    // 4000-4999 : Available for use by applications.
  };

  using onCloseCallback = void (*)(WebSocket &ws,
    const WebSocket::CloseCode &code, const char *reason, uint16_t length);
  using onMessageCallback = void (*)(WebSocket &ws,
    const WebSocket::DataType &dataType, const char *message, uint16_t length);

public:
  virtual ~WebSocket();

  // max reason length = 123 characters!
  void close(const CloseCode &code, bool instant, const char *reason = nullptr,
    uint16_t length = 0);
  void terminate();

  const ReadyState &getReadyState() const;
  bool isAlive();

  IPAddress getRemoteIP();

  void send(
    const WebSocket::DataType dataType, const char *message, uint16_t length);
  void ping(const char *payload = nullptr, uint16_t length = 0);

  void onClose(const onCloseCallback &callback);
  void onMessage(const onMessageCallback &callback);

protected:
  WebSocket();                        // Initialization done by client
  WebSocket(const NetClient &client); // Called by server

  bool _waitForResponse(uint16_t maxAttempts, uint8_t time = 1);

  int _read();
  bool _read(uint8_t *buffer, size_t size);

  void _handleFrame();
  bool _readHeader(header_t &header);
  bool _readData(const header_t &header, char *payload);

  void _send(
    uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length);

protected:
  NetClient m_client;
  ReadyState m_readyState{ ReadyState::CLOSED };

  char *m_secKey{ nullptr };
  bool m_maskEnabled;

  char m_dataBuffer[kBufferMaxSize]{};
  uint16_t m_currentOffset{ 0 };
  int8_t m_tbcOpcode{ -1 }; // indicates opcode (text/binary) that should be
                            // continued by continuation frame

  onCloseCallback _onClose{ nullptr };
  onMessageCallback _onMessage{ nullptr };
};

} // namespace net