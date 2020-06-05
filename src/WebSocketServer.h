#pragma once

#include "WebSocket.h"
#include "utility.h"

namespace net {

class WebSocketServer {
public:
  using verifyClientCallback = bool (*)(
    const IPAddress &ip, const char *header, const char *value);
  using onConnectionCallback = void (*)(WebSocket &ws);

public:
  WebSocketServer(const WebSocketServer &) = delete;
  WebSocketServer &operator=(const WebSocketServer &) = delete;

  /** Initializes server on given port. */
  WebSocketServer(uint16_t port = 3000);
  ~WebSocketServer();

  /**
   * Startup server.
   * @param callback Function called for every header during hadshake (except
   * for those required by protocol, like "Connection", "Upgrade" etc.)
   */
  void begin(const verifyClientCallback &callback = nullptr);
  /** Disconnects all clients. */
  void shutdown();

  /** Sends message to all connected clients. */
  void broadcast(
    const WebSocket::DataType &dataType, const char *message, uint16_t length);

  void listen();

  /** @return Amount of connected clients. */
  uint8_t countClients();

  /**
   * @param callback Function that will be called for every successfully
   * connected client.
   */
  void onConnection(const onConnectionCallback &callback);

private:
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

private:
  NetServer m_server;
  WebSocket *m_sockets[MAX_CONNECTIONS]{};

  verifyClientCallback _verifyClient{ nullptr };
  onConnectionCallback _onConnection{ nullptr };
};

} // namespace net