const WebSocket = require('ws');

const wss = new WebSocket.Server({ host: '192.168.46.10', port: 3000 });

wss.on('connection', function connection(ws, req) {
	ws.remoteAddress = req.connection.remoteAddress.replace(/^.*:/, '');
	console.log(`new client: ${ws.remoteAddress}`);
	
	ws.on('message', function(message) {
		console.log(`[${ws.remoteAddress}] ${message}`);
	});
	
	ws.on('close', function() {
		console.log(`[${ws.remoteAddress}] client disconnected`);
	});
	
	ws.on('error', function(err) {
		console.log(`[${ws.remoteAddress}] error`, err);
	});
	
	ws.on('ping', function() {
		console.log(`[${ws.remoteAddress}] ping`);
	});

	ws.send('hello from node.js');
	//ws.close();
});

wss.on('listening', function() {
	let remote = this.address();
	console.log(`Server running at ${remote.address}:${remote.port}`);
});