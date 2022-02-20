#pragma once

/** @file */

#include "Config.hpp"
#include "Utility.hpp"

namespace net {

/**
 * @brief Generates Sec-WebSocket-Accept value.
 * @param[in] key Client 'Sec-Websocket-Key' to encode
 * @param[out] output Array of 29 elements (including NULL).
 */
void encodeSecKey(const char *key, char output[]);

/** Frame opcodes. */
enum WebSocketOpcode {
  CONTINUATION_FRAME = 0x00,

  //
  // Data frames (non-control):
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

/** Frame data types. */
enum class WebSocketDataType : uint8_t { TEXT, BINARY };

/**
 * Close event codes.
 * @see https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
 */
enum WebSocketCloseCode {
  // 0-999 : Reserved and not used.

  /// Normal closure; the connection successfully completed whatever purpose
  /// for which it was created.
  NORMAL_CLOSURE = 1000,
  /// The endpoint is going away, either because of a server  failure or
  /// because the browser is navigating away from the page that opened the
  /// connection.
  GOING_AWAY,
  /// The endpoint is terminating the connection due to aprotocol error.
  PROTOCOL_ERROR,
  /// The connection is being terminated because the endpoint received data of
  /// a type it cannot accept (for example, a text-only endpoint received
  /// binary data).
  UNSUPPORTED_DATA,

  // 1004 : Reserved. A meaning might be defined in the future.

  /// Reserved. Indicates that no status code was provided even though one was
  /// expected.
  NO_STATUS_RECVD = 1005,
  /// Reserved. Used to indicate that a connection was closed abnormally (that
  /// is, with no close frame being sent) when a status code is expected.
  ABNORMAL_CLOSURE,
  /// The endpoint is terminating the connection because a message was
  /// received that contained inconsistent data (e.g., non-UTF-8 data within a
  /// text message).
  INVALID_FRAME_PAYLOAD_DATA,
  /// The endpoint is terminating the connection because it received a message
  /// that violates its policy. This is a generic status code, used when codes
  /// 1003 and 1009 are not suitable.
  POLICY_VIOLATION,
  /// The endpoint is terminating the connection because a data frame was
  /// received that is too large.
  MESSAGE_TOO_BIG,
  /// The client is terminating the connection because it expected the server
  /// to negotiate one or more extension, but the server didn't.
  MISSING_EXTENSION,
  /// The server is terminating the connection because it encountered an
  /// unexpected condition that prevented it from fulfilling the request.
  INTERNAL_ERROR,
  /// The server is terminating the connection because it is restarting.
  SERVICE_RESTART,
  /// The server is terminating the connection due to a temporary condition,
  /// e.g. it is overloaded and is casting off some of its clients.
  TRY_AGAIN_LATER,
  /// The server was acting as a gateway or proxy and received an invalid
  /// response from the upstream server. This is similar to 502 HTTP Status
  /// Code.
  BAD_GATEWAY,
  /// Reserved. Indicates that the connection was closed due to a failure to
  /// perform a TLS handshake (e.g., the server certificate can't be
  /// verified).
  TLS_HANDSHAKE

  // 1016-1999 : Reserved for future use by the WebSocket standard.
  // 3000-3999 : Available for use by libraries and frameworks. May not be
  //             used by applications. Available for registration at the IANA
  //             via first-come, first-serve.
  // 4000-4999 : Available for use by applications.
};

/** Error codes. */
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
 * Connection states.
 * @see https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/readyState
 */
enum class WebSocketReadyState : uint8_t {
  CONNECTING = 0,
  OPEN,
  CLOSING,
  CLOSED
};

/** @cond */
struct WebSocketHeader_t;
/** @endcond */

/**
 * @class WebSocket
 */
template <class NetClient> class WebSocket {
  template <class, class, uint8_t> friend class WebSocketServer;

public:
  /**
   * @param ws Closing endpoint.
   * @param code Close event code.
   * @param reason Contains a message for a close event, c-string, non
   * NULL-terminated. Might be empty.
   * @param length The number of characters in the reason c-string.
   */
  using onCloseCallback = void (*)(
      WebSocket &ws, const WebSocketCloseCode code, const char *reason,
      uint16_t length);

  /**
   * @param ws Source of a message.
   * @param dataType Type of a message.
   * @param message Non NULL-terminated.
   * @param length Number of data bytes.
   */
  using onMessageCallback = void (*)(
      WebSocket &ws, const WebSocketDataType dataType, const char *message,
      uint16_t length);

public:
  WebSocket(const WebSocket &) = delete;
  virtual ~WebSocket();

  WebSocket &operator=(const WebSocket &) = delete;

  /**
   * @brief Sends a close event.
   * @param instant Determines if it should be closed immediately.
   * @param reason An additional message (not required), doesn't have to be
   * NULL-terminated. Max length = 123 characters.
   * @param length The number of characters in the reason c-string.
   */
  void close(
      const WebSocketCloseCode, bool instant, const char *reason = nullptr,
      uint16_t length = 0);
  /** @brief Immediately closes the connection. */
  void terminate();

  /** @return Endpoint connection status. */
  WebSocketReadyState getReadyState() const;
  /** @brief Verifies endpoint connection. */
  bool isAlive() const;

  /**
   * @return Endpoint IP address.
   * @remark For some microcontrollers it might be empty.
   */
  IPAddress getRemoteIP() const;
  const char *getProtocol() const;

  /**
   * @brief Sends a message frame.
   * @param message Doesn't have to be NULL-terminated.
   */
  void send(const WebSocketDataType, const char *message, uint16_t length);
  /**
   * @brief Sends a ping message.
   * @param payload An additional message, doesn't have to be NULL-terminated.
   * Max length = 125.
   * @param length The number of characters in payload.
   */
  void ping(const char *payload = nullptr, uint16_t length = 0);

  /**
   * @brief Sets the close event handler.
   * @code{.cpp}
   * ws.onClose([](WebSocket &ws, const WebSocketCloseCode code,
   *               const char *reason, uint16_t length) {
   *   // handle close event ...
   * });
   * @endcode
   */
  void onClose(const onCloseCallback &);
  /**
   * @brief Sets the message handler function.
   * @code{.cpp}
   * ws.onMessage([](WebSocket &ws, const WebSocketDataType dataType,
   *                 const char *message, uint16_t length) {
   *   // handle data frame ...
   * });
   * @endcode
   */
  void onMessage(const onMessageCallback &);

protected:
  /** @remark Reserved for WebSocketClient. */
  WebSocket() = default;
  /** @remark Reserved for WebSocketServer. */
  WebSocket(const NetClient &, const char *protocol);

  /** @cond */
  int32_t _read();
  bool _read(char *buffer, size_t size, size_t offset = 0);

  void
  _send(uint8_t opcode, bool fin, bool mask, const char *data, uint16_t length);

  void _readFrame();
  bool _readHeader(WebSocketHeader_t &);
  bool _readData(const WebSocketHeader_t &, char *payload, size_t offset = 0);

  void _clearDataBuffer();

  void _handleContinuationFrame(const WebSocketHeader_t &);
  void _handleDataFrame(const WebSocketHeader_t &);
  void _handleCloseFrame(const WebSocketHeader_t &, const char *payload);
  /** @endcond */
protected:
  mutable NetClient m_client;
  WebSocketReadyState m_readyState{WebSocketReadyState::CLOSED};
  char *m_protocol{nullptr};

  /** @note A client endpoint must always mask frames. */
  bool m_maskEnabled{true};

  char m_dataBuffer[kBufferMaxSize]{};
  uint16_t m_currentOffset{0};
  /// Indicates an opcode (text/binary) that should be continued by continuation
  /// frame.
  int8_t m_tbcOpcode{-1};

  onCloseCallback _onClose{nullptr};
  onMessageCallback _onMessage{nullptr};
};

} // namespace net

#include "WebSocket.inl"
