const WebSocket = require("ws");

const chat = {
  run: function (host, port) {
    wss = new WebSocket.Server({ host, port });

    wss.on("listening", () => {
      const remote = wss.address();
      console.log(`Server running at ${remote.address}:${remote.port}`);
    });

    wss.on("connection", (ws, req) => {
      ws.ip = req.connection.remoteAddress.replace(/^.*:/, "");
      const info = `new client: ${ws.ip}`;
      console.log(info);

      ws.isAlive = true;
      ws.on("pong", () => (ws.isAlive = true));

      wss.clients.forEach((client) => {
        if (client !== ws) client.send(info);
      });

      ws.on("message", (message) => this.broadcast(message, ws));
      ws.on("close", () => {
        const info = `client ${ws.ip} disconnected`;
        console.log(info);

        wss.clients.forEach((ws) => ws.send(info));
      });
    });

    setInterval(() => {
      wss.clients.forEach((ws) => {
        if (ws.isAlive === false) return ws.terminate();

        ws.isAlive = false;
        ws.ping();
      });
    }, 1000);
  },

  broadcast: function (message, sender) {
    const payload = `[${sender.ip}] > ${message}`;
    console.log(payload);

    wss.clients.forEach((ws) => ws.send(payload));
  },
};

chat.run("192.168.46.31", 3000);
