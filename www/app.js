const ws = new WebSocket(`ws://${location.host}/ws`);
ws.onopen = () => console.log('WS connected');
ws.onmessage = (e) => console.log('Message:', e.data);
