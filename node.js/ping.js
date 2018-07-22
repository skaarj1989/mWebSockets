const WebSocket = require('ws');
const wss = new WebSocket.Server({ host: '192.168.46.10', port: 3000 });

wss.on('connection', function connection(ws, req) {
	ws.remoteAddress = req.connection.remoteAddress.replace(/^.*:/, '');
	console.log(`new client: ${ws.remoteAddress}`);
	
  ws.isAlive = true;
	ws.on('pong', function() { this.isAlive = true; });
	
	// ---
	
	ws.on('message', function(message) {
		console.log(`[${ws.remoteAddress}] > ${typeof message}:`, message);
	});
	
	ws.on('close', function(code, reason) {
		console.log(`[${ws.remoteAddress}] client disconnected: ${code} "${reason}"`);
	});
	
	ws.on('error', function(err) {
		console.log(`[${ws.remoteAddress}] error:`, err);
	});
	
	ws.on('ping', function() {
		console.log(`[${ws.remoteAddress}] > ping`);
	});
	
	ws.send('Hello from Node.js');
});

const interval = setInterval(function ping() {
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

wss.on('listening', function() {
	let remote = this.address();
	console.log(`Server running at ${remote.address}:${remote.port}`);
});