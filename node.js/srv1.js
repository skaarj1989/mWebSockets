const WebSocket = require('ws');

const wss = new WebSocket.Server({ host: '192.168.46.10', port: 3000 });

function noop() {}

function heartbeat() {
  this.isAlive = true;
}

wss.on('connection', function connection(ws, req) {
	ws.remoteAddress = req.connection.remoteAddress.replace(/^.*:/, '');
	console.log(`new client: ${ws.remoteAddress}`);
	
  ws.isAlive = true;
  ws.on('pong', heartbeat);
	
	// ---
	
	ws.on('message', function(message) {
		console.log(`[${ws.remoteAddress}] ${message}`);
	});
	
	ws.on('close', function() {
		console.log(`[${ws.remoteAddress}] client disconnected`);
	});
	
	ws.on('error', function(err) {
		console.log(`[${ws.remoteAddress}] error`, err);
	});
	
	ws.send('hello from node.js');
});

const interval = setInterval(function ping() {
  wss.clients.forEach(function each(ws) {
    if (ws.isAlive === false) {
			console.log(`[${ws.remoteAddress}] dead`);
			return ws.terminate();
		}
		
		console.log(`[${ws.remoteAddress}] alive`);
		
    ws.isAlive = false;
    ws.ping(noop);
  });
}, 10000);

wss.on('listening', function() {
	let remote = this.address();
	console.log(`Server running at ${remote.address}:${remote.port}`);
});