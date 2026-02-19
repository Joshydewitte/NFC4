#ifndef STATUS_PAGE_H
#define STATUS_PAGE_H

const char STATUS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NFC Reader Status</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #f5f5f5;
            padding: 20px;
        }
        .header {
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
            color: white;
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .header h1 { font-size: 24px; }
        .nav-buttons {
            display: flex;
            gap: 10px;
        }
        .nav-btn {
            background: rgba(255,255,255,0.2);
            color: white;
            border: 2px solid white;
            padding: 8px 20px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            transition: all 0.3s;
            text-decoration: none;
        }
        .nav-btn:hover {
            background: white;
            color: #11998e;
        }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }
        .stat-card {
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .stat-label {
            color: #666;
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 8px;
        }
        .stat-value {
            color: #333;
            font-size: 24px;
            font-weight: 600;
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-online { background: #4caf50; }
        .status-offline { background: #f44336; }
        .status-warning { background: #ffa500; }
        .log-container {
            background: white;
            border-radius: 10px;
            padding: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            max-height: 600px;
            display: flex;
            flex-direction: column;
        }
        .log-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
            padding-bottom: 15px;
            border-bottom: 2px solid #e0e0e0;
        }
        .log-header h2 {
            color: #333;
            font-size: 20px;
        }
        .log-controls {
            display: flex;
            gap: 10px;
        }
        .log-controls button {
            padding: 8px 15px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 12px;
            transition: all 0.3s;
        }
        .clear-btn {
            background: #ff6b6b;
            color: white;
        }
        .pause-btn {
            background: #667eea;
            color: white;
        }
        .log-output {
            font-family: 'Courier New', monospace;
            font-size: 13px;
            line-height: 1.6;
            overflow-y: auto;
            flex-grow: 1;
            background: #1e1e1e;
            color: #d4d4d4;
            padding: 15px;
            border-radius: 5px;
        }
        .log-entry {
            padding: 4px 0;
            border-bottom: 1px solid #333;
        }
        .log-entry:last-child {
            border-bottom: none;
        }
        .log-timestamp {
            color: #858585;
            margin-right: 10px;
        }
        .log-level-info { color: #4fc3f7; }
        .log-level-success { color: #4caf50; }
        .log-level-warning { color: #ffa500; }
        .log-level-error { color: #f44336; }
        .ws-status {
            font-size: 12px;
            padding: 5px 10px;
            border-radius: 5px;
            display: inline-block;
        }
        .ws-connected {
            background: #d4edda;
            color: #155724;
        }
        .ws-disconnected {
            background: #f8d7da;
            color: #721c24;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>📊 NFC Reader Status Dashboard</h1>
        <div class="nav-buttons">
            <a href="/settings" class="nav-btn">⚙️ Instellingen</a>
            <button class="nav-btn" onclick="logout()">Uitloggen</button>
        </div>
    </div>

    <div class="stats-grid">
        <div class="stat-card">
            <div class="stat-label">Reader Status</div>
            <div class="stat-value">
                <span class="status-indicator" id="readerStatus"></span>
                <span id="readerStatusText">Loading...</span>
            </div>
        </div>
        <div class="stat-card">
            <div class="stat-label">Reader Modus</div>
            <div class="stat-value" id="readerMode">-</div>
        </div>
        <div class="stat-card">
            <div class="stat-label">Server Verbinding</div>
            <div class="stat-value">
                <span class="status-indicator" id="serverStatus"></span>
                <span id="serverStatusText">-</span>
            </div>
        </div>
        <div class="stat-card">
            <div class="stat-label">Kaarten Gelezen</div>
            <div class="stat-value" id="cardsRead">0</div>
        </div>
        <div class="stat-card">
            <div class="stat-label">Webocket Status</div>
            <div class="stat-value">
                <span class="ws-status" id="wsStatus">Connecting...</span>
            </div>
        </div>
        <div class="stat-card">
            <div class="stat-label">Uptime</div>
            <div class="stat-value" id="uptime">-</div>
        </div>
    </div>

    <div class="log-container">
        <div class="log-header">
            <h2>📝 Real-time Log</h2>
            <div class="log-controls">
                <button class="pause-btn" id="pauseBtn" onclick="togglePause()">⏸ Pauzeren</button>
                <button class="clear-btn" onclick="clearLog()">🗑 Wissen</button>
            </div>
        </div>
        <div class="log-output" id="logOutput">
            <div class="log-entry">
                <span class="log-timestamp">[--:--:--]</span>
                <span class="log-level-info">Verbinding maken met reader...</span>
            </div>
        </div>
    </div>

    <script>
        let ws;
        let isPaused = false;
        let reconnectAttempts = 0;
        const maxReconnectAttempts = 10;
        
        function connectWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const hostname = window.location.hostname;
            const wsUrl = protocol + '//' + hostname + ':81/';
            
            console.log('Connecting to WebSocket:', wsUrl);
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                console.log('WebSocket connected successfully');
                reconnectAttempts = 0;
                updateWSStatus(true);
                addLogEntry({ message: '✅ WebSocket verbonden', level: 'success' });
            };
            
            ws.onmessage = function(event) {
                if (!isPaused) {
                    handleMessage(event.data);
                }
            };
            
            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
                updateWSStatus(false);
                addLogEntry({ message: '❌ WebSocket error', level: 'error' });
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected');
                updateWSStatus(false);
                addLogEntry({ message: '⚠️ WebSocket disconnected', level: 'warning' });
                
                // Auto-reconnect
                if (reconnectAttempts < maxReconnectAttempts) {
                    reconnectAttempts++;
                    console.log('Reconnect attempt', reconnectAttempts, 'in 3 seconds...');
                    setTimeout(connectWebSocket, 3000);
                } else {
                    addLogEntry({ message: '❌ WebSocket reconnect failed - reload page', level: 'error' });
                }
            };
        }
        
        function handleMessage(data) {
            try {
                const msg = JSON.parse(data);
                
                // Update stats
                if (msg.type === 'stats') {
                    updateStats(msg.data);
                }
                
                // Add log entry
                if (msg.type === 'log') {
                    addLogEntry(msg.data);
                }
                
                // Status update
                if (msg.type === 'status') {
                    updateStatus(msg.data);
                }
            } catch (e) {
                // Plain text log
                addLogEntry({ message: data, level: 'info' });
            }
        }
        
        function updateStats(data) {
            if (data.cardsRead !== undefined) {
                document.getElementById('cardsRead').textContent = data.cardsRead;
            }
            if (data.uptime !== undefined) {
                document.getElementById('uptime').textContent = formatUptime(data.uptime);
            }
        }
        
        function updateStatus(data) {
            if (data.readerStatus) {
                const indicator = document.getElementById('readerStatus');
                const text = document.getElementById('readerStatusText');
                indicator.className = 'status-indicator ' + (data.readerStatus === 'online' ? 'status-online' : 'status-offline');
                text.textContent = data.readerStatus === 'online' ? 'Online' : 'Offline';
            }
            
            if (data.serverStatus) {
                const indicator = document.getElementById('serverStatus');
                const text = document.getElementById('serverStatusText');
                indicator.className = 'status-indicator ' + (data.serverStatus === 'online' ? 'status-online' : 'status-offline');
                text.textContent = data.serverStatus === 'online' ? 'Connected' : 'Disconnected';
            }
            
            if (data.readerMode) {
                document.getElementById('readerMode').textContent = data.readerMode === 'machine' ? '🏭 Machine' : '🔧 Config';
            }
        }
        
        function addLogEntry(data) {
            const logOutput = document.getElementById('logOutput');
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            
            const timestamp = new Date().toLocaleTimeString('nl-NL');
            const level = data.level || 'info';
            const message = data.message || data;
            
            entry.innerHTML = `
                <span class="log-timestamp">[${timestamp}]</span>
                <span class="log-level-${level}">${message}</span>
            `;
            
            logOutput.appendChild(entry);
            
            // Auto-scroll
            if (!isPaused) {
                logOutput.scrollTop = logOutput.scrollHeight;
            }
            
            // Keep only last 200 entries
            while (logOutput.children.length > 200) {
                logOutput.removeChild(logOutput.firstChild);
            }
        }
        
        function updateWSStatus(connected) {
            const status = document.getElementById('wsStatus');
            if (connected) {
                status.textContent = '🟢 Connected';
                status.className = 'ws-status ws-connected';
            } else {
                status.textContent = '🔴 Disconnected';
                status.className = 'ws-status ws-disconnected';
            }
        }
        
        function togglePause() {
            isPaused = !isPaused;
            const btn = document.getElementById('pauseBtn');
            btn.textContent = isPaused ? '▶ Hervatten' : '⏸ Pauzeren';
        }
        
        function clearLog() {
            document.getElementById('logOutput').innerHTML = '';
        }
        
        function formatUptime(seconds) {
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            const secs = seconds % 60;
            return `${hours}h ${minutes}m ${secs}s`;
        }
        
        function logout() {
            fetch('/api/logout', { 
                method: 'POST',
                credentials: 'same-origin'
            })
            .then(() => location.href = '/login');
        }
        
        // Initialize
        console.log('Initializing status page...');
        connectWebSocket();
        
        // Fetch initial stats after a short delay (wait for WebSocket to connect)
        setTimeout(function() {
            console.log('Fetching initial stats...');
            fetch('/api/stats', {
                credentials: 'same-origin'
            })
                .then(r => r.json())
                .then(data => {
                    console.log('Stats received:', data);
                    updateStatus(data);
                    addLogEntry({ message: '📊 Status data geladen', level: 'info' });
                })
                .catch(e => {
                    console.error('Error fetching stats:', e);
                    addLogEntry({ message: '❌ Fout bij laden stats: ' + e, level: 'error' });
                });
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

#endif // STATUS_PAGE_H
