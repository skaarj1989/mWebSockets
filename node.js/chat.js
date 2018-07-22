const WebSocket = require('ws');
const wss = new WebSocket.Server({ host: '192.168.46.10', port: 3000 });

var chat = {	
	run: function() {
		wss.on('listening', function() {
			let remote = this.address();
			console.log(`Server running at ${remote.address}:${remote.port}`);
		});
		
		wss.on('connection', function(ws, req) {
			ws.ip = req.connection.remoteAddress.replace(/^.*:/, '');
			let info = `new client: ${ws.ip}`;
			console.log(info);
			
			ws.isAlive = true;
			ws.on('pong', function() { this.isAlive = true; });
			
			wss.clients.forEach((client) => {
				if (client !== ws)
					client.send(info);
			});
			
			ws.on('message', (message) => chat.broadcast(message, ws) );
			ws.on('close', () => {
				let info = `client ${ws.ip} disconnected`;
				console.log(info);
				
				wss.clients.forEach((ws) => {
					ws.send(info);
				});
			});
		});
		
		const interval = setInterval(function ping() {
			wss.clients.forEach((ws) => {
				if (ws.isAlive === false)
					return ws.terminate();
								
				ws.isAlive = false;
				ws.ping();
			});
		}, 1000);
	},
	
	broadcast: function(message, sender) {
		let payload = `[${sender.ip}] > ${message}`;
		console.log(payload);
		
		wss.clients.forEach(function each(ws) {
			ws.send(payload);
		});
	}
};

chat.run();