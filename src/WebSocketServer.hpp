#pragma once

/** @file */

#include "WebSocket.hpp"

namespace net {

/** @class IWebSocketServer */
class IWebSocketServer {
public:
  virtual ~IWebSocketServer() = default;

  /**
   * @param header c-string, NULL-terminated.
   * @param value c-string, NULL-terminated.
   */
  using verifyClientCallback =
      bool (*)(const IPAddress &, const char *header, const char *value);
  /** @param ws Accepted client. */
  using protocolHandlerCallback = const char *(*)(const char *);

  /**
   * @brief Startup server.
   * @code{.cpp}
   * WebSocketServer server;
   * server.begin(
   *   [](const IPAddress &ip, const char *header, const char *value) {
   *     // verify client ip address and/or request headers ...
   *   },
   *   [](const char *clientProtocols) {
   *     // return negotiated subprotocol or nullptr to ignore
   *   });
   * @endcode
   * @param callback Function called for every header during hadshake (except
   * for those required by protocol, like **Connection**, **Upgrade** etc.)
   */
  virtual void begin(
      const verifyClientCallback &verifyClient = nullptr,
      const protocolHandlerCallback &protocolHandler = nullptr) = 0;

  /** @brief Disconnects all clients. */
  virtual void shutdown() = 0;

  /** @brief Sends message to all connected clients. */
  virtual void
  broadcast(WebSocketDataType, const char *message, uint16_t length) = 0;

  /** @note Call this in main loop. */
  virtual void listen() = 0;

  /** @return Amount of connected clients. */
  virtual uint8_t countClients() const = 0;

  using onConnectionCallback = void (*)(IWebSocket &ws);

  /**
   * @brief
   * @code{.cpp}
   * WebSocketServer server;
   * server.onConnection([](WebSocket &ws) {
   *   // New client, setup it's callbacks:
   *   ws.onClose([](WebSocket &ws, WebSocketCloseCode,
   *                 const char *reason, uint16_t length) {
   *     // handle close event ...
   *   });
   *   ws.onMessage([](WebSocket &ws, WebSocketDataType,
   *               const char *message, uint16_t length) {
   *     // handle data frame ...
   *   });
   * });
   * @endcode
   * @param callback Function that will be called for every successfully
   * connected client.
   */
  virtual void onConnection(const onConnectionCallback &callback) = 0;
};

/** @class WebSocketServer */
template <class NetServer, class NetClient, uint8_t _MaxConnections = 4>
class WebSocketServer : public IWebSocketServer {
  static_assert(_MaxConnections <= 8, "I don't think so ...");

public:
  /**
   * @brief Initializes server on given port.
   * @note Don't forget to call begin()
   */
  explicit WebSocketServer(uint16_t port = 3000);
  WebSocketServer(const WebSocketServer &) = delete;
  ~WebSocketServer() override;

  WebSocketServer &operator=(const WebSocketServer &) = delete;

  void begin(
      const verifyClientCallback &verifyClient = nullptr,
      const protocolHandlerCallback &protocolHandler = nullptr) override;
  void shutdown() override;

  void
  broadcast(WebSocketDataType, const char *message, uint16_t length) override;

  void listen() override;

  uint8_t countClients() const override;

  void onConnection(const onConnectionCallback &callback) override;

private:
  /** @cond */
  WebSocket<NetClient> *_getWebSocket(NetClient &) const;

  /// @param[out] protocol
  bool _handleRequest(NetClient &, char selectedProtocol[]);
  void _rejectRequest(NetClient &, WebSocketError);
  void _acceptRequest(NetClient &, const char *secKey, const char *protocol);

  void _cleanDeadConnections();
  /** @endcond */
private:
  // Issue with EthernetServer on ESP32:
  // https://github.com/arduino-libraries/Ethernet/issues/88#issuecomment-455498941
  NetServer m_server;
  WebSocket<NetClient> *m_sockets[_MaxConnections]{};

  verifyClientCallback _verifyClient{nullptr};
  protocolHandlerCallback _protocolHandler{nullptr};
  onConnectionCallback _onConnection{nullptr};
};

bool isValidGET(char *line);
bool isValidUpgrade(const char *line);
bool isValidConnection(char *value);
WebSocketError validateHandshake(uint8_t flags, const char *secKey);

/**
 * @example ./simple-server/simple-server.ino
 * Example usage of WebSocketServer class
 */

} // namespace net

#include "WebSocketServer.inl"
