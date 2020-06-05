#pragma once

#include "utility.h"

namespace net {

constexpr uint8_t kValidUpgradeHeader = 0x01;
constexpr uint8_t kValidConnectionHeader = 0x02;
constexpr uint8_t kValidSecKey = 0x04;
constexpr uint8_t kValidVersion = 0x08;

enum class WebSocketError {
  NO_ERROR,

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
  enum class ReadyState : int8_t { CONNECTING = 0, OPEN, CLOSING, CLOSED };
  enum class DataType : int8_t { TEXT, BINARY };
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

  // https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
  enum CloseCode {
    // 0-999 : Reserved and not used.

    NORMAL_CLOSURE =
      1000, /**< Normal closure; the connection successfully completed whatever
               purpose for which it was created. */

    GOING_AWAY,     /**< The endpoint is going away, either because of a server
                       failure or because the browser is navigating away from the
                       page that opened the connection. */
    PROTOCOL_ERROR, /**< The endpoint is terminating the connection due to a
                       protocol error. */
    UNSUPPORTED_DATA, /**< The connection is being terminated because the
                         endpoint received data of a type it cannot accept (for
                         example, a text-only endpoint received binary data). */

    // 1004 : Reserved. A meaning might be defined in the future.

    NO_STATUS_RECVD = 1005, /**< Reserved. Indicates that no status code was
                               provided even though one was expected. */
    ABNORMAL_CLOSURE, /**< Reserved. Used to indicate that a connection was
                         closed abnormally (that is, with no close frame being
                         sent) when a status code is expected. */
    INVALID_FRAME_PAYLOAD_DATA, /**< The endpoint is terminating the connection
                                   because a message was received that contained
                                   inconsistent data (e.g., non-UTF-8 data
                                   within a text message). */
    POLICY_VIOLATION, /**< The endpoint is terminating the connection because it
                         received a message that violates its policy. This is a
                         generic status code, used when codes 1003 and 1009 are
                         not suitable. */
    MESSAGE_TOO_BIG,  /**< The endpoint is terminating the connection because a
                         data frame was received that is too large. */
    MISSING_EXTENSION, /**< The client is terminating the connection because it
                          expected the server to negotiate one or more
                          extension, but the server didn't. */
    INTERNAL_ERROR,    /**< The server is terminating the connection because it
                          encountered an unexpected condition that prevented it
                          from fulfilling the request. */
    SERVICE_RESTART, /**< The server is terminating the connection because it is
                        restarting. */
    TRY_AGAIN_LATER, /**< The server is terminating the connection due to a
                        temporary condition, e.g. it is overloaded and is
                        casting off some of its clients. */
    BAD_GATEWAY,  /**< The server was acting as a gateway or proxy and received
                     an invalid response from the upstream server. This is
                     similar to 502 HTTP Status Code. */
    TLS_HANDSHAKE /**< Reserved. Indicates that the connection was closed due to
                     a failure to perform a TLS handshake (e.g., the server
                     certificate can't be verified). */

    // 1016-1999 : Reserved for future use by the WebSocket standard.
    // 3000-3999 : Available for use by libraries and frameworks. May not be
    //             used by applications. Available for registration at the IANA
    //             via first-come, first-serve.
    // 4000-4999 : Available for use by applications.
  };

  using onCloseCallback = void (*)(WebSocket &ws,
    const WebSocket::CloseCode &code, const char *reason, uint16_t length);
  using onMessageCallback = void (*)(WebSocket &ws,
    const WebSocket::DataType &dataType, const char *message, uint16_t length);

public:
  WebSocket(const WebSocket &) = delete;
  WebSocket &operator=(const WebSocket &) = delete;

  virtual ~WebSocket();

  /**
    * Sends close event.
    * @remarks Max reason length = 123 characters!
    */
  void close(const CloseCode &code, bool instant, const char *reason = nullptr,
    uint16_t length = 0);
  /** Immediately closes the connection. */
  void terminate();

  const ReadyState &getReadyState() const;
  /** Verifies endpoint connection. */
  bool isAlive();

 #if PLATFORM_ARCH != PLATFORM_ARCHITECTURE_ESP8266
  IPAddress getRemoteIP();
 #endif

  void send(
    const WebSocket::DataType dataType, const char *message, uint16_t length);
  void ping(const char *payload = nullptr, uint16_t length = 0);

  void onClose(const onCloseCallback &callback);
  void onMessage(const onMessageCallback &callback);

protected:
  /** Initialization done by client. */
  WebSocket();
  /** Endpoint initialized by server. */
  WebSocket(const NetClient &client);

  int32_t _read();
  bool _read(char *buffer, size_t size);

  void _send(
    uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length);

  void _readFrame();
  bool _readHeader(header_t &header);
  bool _readData(const header_t &header, char *payload);

  void _handleContinuationFrame(const header_t &header, const char *payload);
  void _handleDataFrame(const header_t &header, const char *payload);
  void _handleCloseFrame(const header_t &header, const char *payload);

protected:
  NetClient m_client;
  ReadyState m_readyState;

  bool m_maskEnabled;

  char
    m_dataBuffer[kBufferMaxSize]{}; // #TODO change to dynamic memory allocation
                                    // (would save memory in server).
  uint16_t m_currentOffset{ 0 };
  int8_t m_tbcOpcode{ -1 }; /**< indicates opcode (text/binary) that should be
                               continued by continuation frame */

  onCloseCallback _onClose{ nullptr };
  onMessageCallback _onMessage{ nullptr };
};

} // namespace net