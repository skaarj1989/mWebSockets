const WebSocket = require("ws");

const wss = new WebSocket.Server({
  host: "192.168.46.31",
  port: 3000,
});

wss.on("connection", (ws, req) => {
  ws.remoteAddress = req.socket.remoteAddress.replace(/^.*:/, "");
  console.log(`new client: ${ws.remoteAddress}`);

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

  setTimeout(() => {
    console.log(`disconnecting ${ws.remoteAddress} ...`);
    ws.close();
  }, 60000);
});

wss.on("listening", () => {
  let remote = wss.address();
  console.log(`Server running at ${remote.address}:${remote.port}`);
});
