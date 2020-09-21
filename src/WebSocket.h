#pragma once

/** @file */

#include "utility.h"

namespace net {

/**
 * @brief Generates Sec-WebSocket-Key value.
 * @param[out] output Array of 25 elements (with one for NULL).
 */
void generateSecKey(char output[]);
/**
 * @brief Generates Sec-WebSocket-Accept value.
 * @param[out] output Array of 29 elements (including NULL).
 * @param[in] key Client 'Sec-Websocket-Key' to encode
 */
bool encodeSecKey(char output[], const char *key);

/**
 * Error codes.
 */
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

/**
 * @class WebSocket
 * @see https://tools.ietf.org/html/rfc6455
 */
class WebSocket {
  friend class WebSocketServer;
  struct header_t;

public:
  /**
   * Connection states.
   * @see https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/readyState
   */
  enum class ReadyState : int8_t { CONNECTING = 0, OPEN, CLOSING, CLOSED };
  /** Frame data types. */
  enum class DataType : int8_t { TEXT, BINARY };
  
  /** Frame opcodes. */
  enum Opcode {
    CONTINUATION_FRAME = 0x00,

    //
    // Data Frames (non-control):
    //

    TEXT_FRAME = 0x01,
    BINARY_FRAME = 0x02,

    //
    // Reserved non-control frames:
    //

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

    //
    // Reserved control opcodes:
    //

    // 0x0B
    // 0x0C
    // 0x0D
    // 0x0E
    // 0x0F
  };

  /**
   * Close event code.
   * @see https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
   */
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

  /**
   * @param ws Closing endpoint.
   * @param code Close event code.
   * @param reason Contains message for close event, c-string, non NULL-terminated. May be empty.
   * @param length Number of characters in reason c-string.
   */
  using onCloseCallback = void (*)(WebSocket &ws,
    const WebSocket::CloseCode &code, const char *reason, uint16_t length);

  /**
   * @param ws Source of a message.
   * @param dataType Type of message.
   * @param message Contains data, non NULL-terminated.
   * @param length Number of data bytes.
   */
  using onMessageCallback = void (*)(WebSocket &ws,
    const WebSocket::DataType &dataType, const char *message, uint16_t length);

public:
  WebSocket(const WebSocket &) = delete;
  WebSocket &operator=(const WebSocket &) = delete;

  virtual ~WebSocket();

  /**
   * @brief Sends close event.
   * @param code
   * @param instant Determines if close should be done immediately.
   * @param reason Additional message (not required), doesn't have to be NULL-terminated. Max length = 123 characters.
   * @param length Number of characters in reason.
   */
  void close(const CloseCode &code, bool instant, const char *reason = nullptr,
    uint16_t length = 0);
  /** @brief Instantly closes connection. */
  void terminate();

  /** @return Endpoint connection status. */
  const ReadyState &getReadyState() const;
  /** @brief Verifies endpoint connection. */
  bool isAlive() const;

  /**
   * @return Endpoint IP address.
   * @remark For some microcontrollers it may be empty.
   */
  IPAddress getRemoteIP() const;

  /**
   * @brief Sends message frame.
   * @param message Doesn't have to be NULL-terminated.
   */
  void send(
    const WebSocket::DataType dataType, const char *message, uint16_t length);
  /**
   * @brief Sends ping message.
   * @param payload Additional message, doesn't have to be NULL-terminated. Max length = 125.
   * @param length Number of characters in payload.
   */
  void ping(const char *payload = nullptr, uint16_t length = 0);

  /**
   * @brief Sets close event handler.
   * @code{.cpp}
   * ws.onClose([](WebSocket &ws, const WebSocket::CloseCode &code,
   *             const char *reason, uint16_t length) {
   *   // handle close event ...
   * });
   * @endcode
   */
  void onClose(const onCloseCallback &callback);
  /**
   * @brief Sets message handler function.
   * @code{.cpp}
   * ws.onMessage([](WebSocket &ws, const WebSocket::DataType &dataType,
   *               const char *message, uint16_t length) {
   *   // handle data frame ...
   * });
   * @endcode
   */
  void onMessage(const onMessageCallback &callback);

protected:
  /** @remark Reserved for WebSocketClient. */
  WebSocket();
  /** @remark Reserved for WebSocketServer. */
  WebSocket(const NetClient &client);

  /** @cond */
  int32_t _read();
  bool _read(char *buffer, size_t size, size_t offset = 0);

  void _send(
    uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length);

  void _readFrame();
  bool _readHeader(header_t &header);
  bool _readData(const header_t &header, char *payload, size_t offset = 0);

  void _clearDataBuffer();

  void _handleContinuationFrame(const header_t &header);
  void _handleDataFrame(const header_t &header);
  void _handleCloseFrame(const header_t &header, const char *payload);
  /** @endcond */
protected:
  mutable NetClient m_client;
  ReadyState m_readyState;

  /** @note Client endpoint must always mask frames. */
  bool m_maskEnabled;

  char
    m_dataBuffer[kBufferMaxSize]{}; /* #TODO Change to dynamic memory allocation
                                     (would save memory in server). */
  uint16_t m_currentOffset{ 0 };
  int8_t m_tbcOpcode{ -1 }; /**< Indicates opcode (text/binary) that should be
                               continued by continuation frame. */

  onCloseCallback _onClose{ nullptr };
  onMessageCallback _onMessage{ nullptr };
};

/** @cond */
constexpr uint8_t kValidUpgradeHeader = 0x01;
constexpr uint8_t kValidConnectionHeader = 0x02;
constexpr uint8_t kValidSecKey = 0x04;
constexpr uint8_t kValidVersion = 0x08;
/** @endcond */

} // namespace net