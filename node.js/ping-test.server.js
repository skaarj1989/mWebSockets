const WebSocket = require("ws");
const wss = new WebSocket.Server({ host: "192.168.46.31", port: 3000 });

wss.on("connection", (ws, req) => {
  ws.remoteAddress = req.socket.remoteAddress.replace(/^.*:/, "");
  console.log(`new client: ${ws.remoteAddress}`);

  ws.isAlive = true;
  ws.on("pong", () => (ws.isAlive = true));

  // ---

  ws.on("message", (message) => {
    console.log(`[${ws.remoteAddress}] > ${typeof message}:`, message);
  });

  ws.on("close", (code, reason) => {
    console.log(
      `[${ws.remoteAddress}] client disconnected: ${code} "${reason}"`
    );
  });

  ws.on("error", (err) => console.log(`[${ws.remoteAddress}] error:`, err));
  ws.on("ping", () => console.log(`[${ws.remoteAddress}] > ping`));

  ws.send("Hello from Node.js");
});

setInterval(() => {
  wss.clients.forEach((ws) => {
    if (ws.isAlive === false) {
      console.log(`[${ws.remoteAddress}] dead`);
      return ws.terminate();
    }

    console.log(`[${ws.remoteAddress}] alive`);

    ws.isAlive = false;
    ws.ping();
  });
}, 10000);

wss.on("listening", () => {
  let remote = wss.address();
  console.log(`Server running at ${remote.address}:${remote.port}`);
});
