#pragma once

/** @file */

#include "WebSocket.h"
#include "utility.h"

namespace net {

/**
 * @class WebSocketServer
 * @see https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
 */
class WebSocketServer {
public:
  /**
   * @param header c-string, NULL-terminated.
   * @param value c-string, NULL-terminated.
   */
  using verifyClientCallback = bool (*)(
    const IPAddress &ip, const char *header, const char *value);
  /** @param ws Accepted client. */
  using onConnectionCallback = void (*)(WebSocket &ws);

public:
  WebSocketServer(const WebSocketServer &) = delete;
  WebSocketServer &operator=(const WebSocketServer &) = delete;

  /**
   * @brief Initializes server on given port.
   * @note Don't forget to call begin()
   */
  WebSocketServer(uint16_t port = 3000);
  ~WebSocketServer();

  /**
   * @brief Startup server.
   * @code{.cpp}
   * WebSocketServer server;
   * server.begin([](const IPAddress &ip, const char *header, const char *value) {
   *   // verify client ip address and/or request headers ...
   * });
   * @endcode
   * @param callback Function called for every header during hadshake (except
   * for those required by protocol, like **Connection**, **Upgrade** etc.)
   */
  void begin(const verifyClientCallback &callback = nullptr);
  /** @brief Disconnects all clients. */
  void shutdown();

  /** @brief Sends message to all connected clients. */
  void broadcast(
    const WebSocket::DataType &dataType, const char *message, uint16_t length);

  /** @note Call this in main loop. */
  void listen();

  /** @return Amount of connected clients. */
  uint8_t countClients() const;

  /**
   * @code{.cpp}
   * WebSocketServer server;
   * server.onConnection([](WebSocket &ws) {
   *   // New client, setup it's callbacks:
   *   ws.onClose([](WebSocket &ws, const WebSocket::CloseCode &code,
   *             const char *reason, uint16_t length) {
   *     // handle close event ...
   *   });
   *   ws.onMessage([](WebSocket &ws, const WebSocket::DataType &dataType,
   *               const char *message, uint16_t length) {
   *     // handle data frame ...
   *   });
   * });
   * @endcode
   * @param callback Function that will be called for every successfully
   * connected client.
   */
  void onConnection(const onConnectionCallback &callback);

private:
  /** @cond */
  WebSocket *_getWebSocket(NetClient &client) const;

  bool _handleRequest(NetClient &client);
  bool _isValidGET(char *line);
  bool _isValidUpgrade(const char *line);
  bool _isValidConnection(char *value);
  bool _isValidVersion(uint8_t version);
  WebSocketError _validateHandshake(uint8_t flags, const char *secKey);
  void _rejectRequest(NetClient &client, const WebSocketError &code);
  void _acceptRequest(NetClient &client, const char *secKey);
  
  void _cleanDeadConnections();
  /** @endcond */
private:
  NetServer m_server;
  WebSocket *m_sockets[kMaxConnections]{};

  verifyClientCallback _verifyClient{ nullptr };
  onConnectionCallback _onConnection{ nullptr };
};

/** @example ./simple-server/simple-server.ino
 * Example usage of WebSocketServer class
 */

} // namespace net