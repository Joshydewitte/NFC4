#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

const char SETTINGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Instellingen</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #f5f5f5;
            padding: 20px;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .header h1 { font-size: 24px; }
        .logout-btn {
            background: rgba(255,255,255,0.2);
            color: white;
            border: 2px solid white;
            padding: 8px 20px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            transition: all 0.3s;
        }
        .logout-btn:hover {
            background: white;
            color: #667eea;
        }
        .card {
            background: white;
            border-radius: 10px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .card h2 {
            color: #333;
            margin-bottom: 20px;
            font-size: 20px;
            display: flex;
            align-items: center;
        }
        .card h2::before {
            content: '';
            width: 4px;
            height: 24px;
            background: #667eea;
            margin-right: 10px;
            border-radius: 2px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #555;
            font-weight: 500;
            font-size: 14px;
        }
        input[type="text"], input[type="password"], select {
            width: 100%;
            padding: 12px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 14px;
            transition: border-color 0.3s;
        }
        input:focus, select:focus {
            outline: none;
            border-color: #667eea;
        }
        .radio-group {
            display: flex;
            gap: 20px;
            margin-top: 10px;
        }
        .radio-group label {
            display: flex;
            align-items: center;
            gap: 8px;
            cursor: pointer;
        }
        button {
            padding: 12px 30px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
        }
        .danger-btn {
            background: linear-gradient(135deg, #ff6b6b 0%, #ee5a6f 100%);
        }
        .success-btn {
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
        }
        .button-group {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
        }
        .info-box {
            background: #f0f4ff;
            border-left: 4px solid #667eea;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            font-size: 14px;
            color: #555;
        }
        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-online { background: #4caf50; }
        .status-offline { background: #f44336; }
        #message {
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            display: none;
        }
        .message-success {
            background: #d4edda;
            border: 1px solid #c3e6cb;
            color: #155724;
        }
        .message-error {
            background: #f8d7da;
            border: 1px solid #f5c6cb;
            color: #721c24;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>⚙️ NFC Reader Instellingen</h1>
        <button class="logout-btn" onclick="logout()">Uitloggen</button>
    </div>

    <div id="message"></div>

    <div class="card">
        <h2>Server Instellingen</h2>
        <div class="form-group">
            <label for="serverUrl">Server URL (inclusief http:// en poort)</label>
            <input type="text" id="serverUrl" placeholder="http://192.168.10.7:3000" value="%SERVER_URL%">
            <small style="color: #666; display: block; margin-top: 5px;">
                ℹ️ Voorbeeld: http://192.168.10.7:3000 (zonder trailing slash)
            </small>
        </div>
        <div class="form-group">
            <label>Server Status</label>
            <div>
                <span class="status-indicator %SERVER_STATUS_CLASS%" id="serverStatusIndicator"></span>
                <span id="serverStatus">%SERVER_STATUS%</span>
            </div>
        </div>
        <button onclick="saveServerSettings()">💾 Server Opslaan</button>
        <button onclick="testServer()" style="margin-left: 10px;">🔄 Test Verbinding</button>
    </div>

    <div class="card">
        <h2>Reader Modus</h2>
        <div class="info-box">
            <strong>Machine Reader:</strong> Leest kaarten en communiceert met server<br>
            <strong>Config Reader:</strong> Schrijft nieuwe kaarten met masterkey
        </div>
        <div class="radio-group">
            <label>
                <input type="radio" name="readerMode" value="machine" %MACHINE_CHECKED%>
                🏭 Machine Reader
            </label>
            <label>
                <input type="radio" name="readerMode" value="config" %CONFIG_CHECKED%>
                🔧 Config Reader
            </label>
        </div>
        <button onclick="saveReaderMode()" style="margin-top: 20px;">💾 Modus Opslaan</button>
    </div>

    <div class="card" id="configModeCard" style="display: %CONFIG_DISPLAY%;">
        <h2>Config Modus Opties</h2>
        <div class="info-box">
            <strong>🔑 Twee methoden voor kaart personalisatie:</strong><br>
            <strong>1. Server Keys (Aanbevolen)</strong> - Haal keys automatisch op van server<br>
            <strong>2. Handmatige Masterkey</strong> - Voor offline/test personalisatie
        </div>
        
        <div class="section-title" style="margin-top: 20px;">Handmatige Masterkey (Optioneel)</div>
        <div class="info-box" style="background: #fff8e1; border-left-color: #ffa500;">
            ⚠️ Alleen gebruiken voor offline personalisatie of testen.<br>
            Server keys worden automatisch gebruikt als geen masterkey is ingesteld.
        </div>
        <div class="form-group">
            <label for="masterkey">Masterkey (32 hex characters)</label>
            <input type="text" id="masterkey" placeholder="00000000000000000000000000000000" maxlength="32" pattern="[0-9A-Fa-f]{32}">
        </div>
        <button onclick="setMasterkey()">🔑 Masterkey Activeren (Sessie)</button>
        <button onclick="clearMasterkey()" style="margin-left: 10px; background: #999;">🗑 Masterkey Wissen</button>
    </div>

    <div class="card">
        <h2>Netwerk Instellingen</h2>
        <div class="radio-group">
            <label>
                <input type="radio" name="ipMode" value="dhcp" %DHCP_CHECKED% onchange="toggleStaticIP()">
                📡 DHCP (Automatisch)
            </label>
            <label>
                <input type="radio" name="ipMode" value="static" %STATIC_CHECKED% onchange="toggleStaticIP()">
                📌 Statisch IP
            </label>
        </div>
        <div id="staticFields" style="margin-top: 20px; display: %STATIC_DISPLAY%;">
            <div class="form-group">
                <label for="staticIP">IP Adres</label>
                <input type="text" id="staticIP" placeholder="192.168.1.100" value="%STATIC_IP%">
            </div>
            <div class="form-group">
                <label for="gateway">Gateway</label>
                <input type="text" id="gateway" placeholder="192.168.1.1" value="%GATEWAY%">
            </div>
            <div class="form-group">
                <label for="subnet">Subnet Mask</label>
                <input type="text" id="subnet" placeholder="255.255.255.0" value="%SUBNET%">
            </div>
        </div>
        <button onclick="saveNetworkSettings()" style="margin-top: 10px;">💾 Netwerk Opslaan</button>
    </div>

    <div class="card">
        <h2>Systeem Beheer</h2>
        <div class="button-group">
            <button onclick="rebootDevice()">🔄 Herstarten</button>
            <button class="danger-btn" onclick="resetNetwork()">🔌 Reset Netwerk</button>
            <button class="danger-btn" onclick="factoryReset()">⚠️ Factory Reset</button>
        </div>
    </div>

    <script>
        function toggleStaticIP() {
            const mode = document.querySelector('input[name="ipMode"]:checked').value;
            document.getElementById('staticFields').style.display = mode === 'static' ? 'block' : 'none';
        }
        
        function toggleConfigMode() {
            const mode = document.querySelector('input[name="readerMode"]:checked').value;
            document.getElementById('configModeCard').style.display = mode === 'config' ? 'block' : 'none';
        }
        
        document.querySelectorAll('input[name="readerMode"]').forEach(radio => {
            radio.addEventListener('change', toggleConfigMode);
        });
        
        function showMessage(text, isError = false) {
            const msg = document.getElementById('message');
            msg.textContent = text;
            msg.className = isError ? 'message-error' : 'message-success';
            msg.style.display = 'block';
            setTimeout(() => msg.style.display = 'none', 5000);
        }
        
        function saveServerSettings() {
            const url = document.getElementById('serverUrl').value.trim();
            
            if (!url || url === '') {
                showMessage('❌ Vul een server URL in', true);
                return;
            }
            
            // Basic URL validation
            if (!url.startsWith('http://') && !url.startsWith('https://')) {
                showMessage('❌ URL moet beginnen met http:// of https://', true);
                return;
            }
            
            // Remove trailing slash
            const cleanUrl = url.endsWith('/') ? url.slice(0, -1) : url;
            
            showMessage('💾 Opslaan en testen...');
            
            fetch('/api/settings/server', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'same-origin',
                body: JSON.stringify({ url: cleanUrl })
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    // Update the input field with cleaned URL
                    document.getElementById('serverUrl').value = cleanUrl;
                    
                    const statusIndicator = document.getElementById('serverStatusIndicator');
                    const statusText = document.getElementById('serverStatus');
                    
                    if (data.connected) {
                        showMessage('✅ Server opgeslagen en online!');
                        statusText.textContent = 'Online';
                        statusIndicator.className = 'status-indicator status-online';
                    } else {
                        showMessage('⚠️ Server opgeslagen maar offline - controleer of de server draait', true);
                        statusText.textContent = 'Offline';
                        statusIndicator.className = 'status-indicator status-offline';
                    }
                }
            })
            .catch(e => showMessage('❌ Fout: ' + e, true));
        }
        
        function testServer() {
            showMessage('🔄 Test verbinding...');
            
            const statusIndicator = document.getElementById('serverStatusIndicator');
            const statusText = document.getElementById('serverStatus');
            
            fetch('/api/test-server', {
                credentials: 'same-origin'
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    showMessage('✅ Server bereikbaar! Latency: ' + data.latency + 'ms');
                    statusText.textContent = 'Online';
                    statusIndicator.className = 'status-indicator status-online';
                } else {
                    showMessage('❌ Server niet bereikbaar - controleer URL en of server draait', true);
                    statusText.textContent = 'Offline';
                    statusIndicator.className = 'status-indicator status-offline';
                }
            })
            .catch(e => {
                showMessage('❌ Fout bij testen: ' + e, true);
                statusText.textContent = 'Offline';
                statusIndicator.className = 'status-indicator status-offline';
            });
        }
        
        function saveReaderMode() {
            const mode = document.querySelector('input[name="readerMode"]:checked').value;
            fetch('/api/settings/mode', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'same-origin',
                body: JSON.stringify({ mode: mode })
            })
            .then(r => r.json())
            .then(data => {
                showMessage('✅ Reader modus opgeslagen');
                setTimeout(() => location.reload(), 1500);
            })
            .catch(e => showMessage('❌ Fout: ' + e, true));
        }
        
        function setMasterkey() {
            const key = document.getElementById('masterkey').value;
            if (!/^[0-9A-Fa-f]{32}$/.test(key)) {
                showMessage('❌ Ongeldige masterkey (32 hex characters vereist)', true);
                return;
            }
            fetch('/api/settings/masterkey', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'same-origin',
                body: JSON.stringify({ key: key })
            })
            .then(r => r.json())
            .then(data => showMessage('✅ Masterkey actief voor deze sessie'))
            .catch(e => showMessage('❌ Fout: ' + e, true));
        }
        
        function clearMasterkey() {
            fetch('/api/settings/masterkey/clear', {
                method: 'POST',
                credentials: 'same-origin'
            })
            .then(r => r.json())
            .then(data => {
                showMessage('🗑 Masterkey gewist');
                document.getElementById('masterkey').value = '';
            })
            .catch(e => showMessage('❌ Fout: ' + e, true));
        }
        
        function saveNetworkSettings() {
            const mode = document.querySelector('input[name="ipMode"]:checked').value;
            const data = { mode: mode };
            if (mode === 'static') {
                data.ip = document.getElementById('staticIP').value;
                data.gateway = document.getElementById('gateway').value;
                data.subnet = document.getElementById('subnet').value;
            }
            fetch('/api/settings/network', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'same-origin',
                body: JSON.stringify(data)
            })
            .then(r => r.json())
            .then(data => showMessage('✅ Netwerk instellingen opgeslagen - Herstart vereist'))
            .catch(e => showMessage('❌ Fout: ' + e, true));
        }
        
        function rebootDevice() {
            if (confirm('ESP32 herstarten?')) {
                fetch('/api/reboot', { 
                    method: 'POST',
                    credentials: 'same-origin'
                });
                showMessage('🔄 Herstarten...');
            }
        }
        
        function resetNetwork() {
            if (confirm('Alle netwerk instellingen resetten?')) {
                fetch('/api/reset-network', { 
                    method: 'POST',
                    credentials: 'same-origin'
                });
                showMessage('🔌 Netwerk gereset - Herstart vereist');
            }
        }
        
        function factoryReset() {
            if (confirm('WAARSCHUWING: Alle instellingen worden gewist! Doorgaan?')) {
                fetch('/api/factory-reset', { 
                    method: 'POST',
                    credentials: 'same-origin'
                });
                showMessage('⚠️ Factory reset uitgevoerd...');
                setTimeout(() => location.href = '/', 3000);
            }
        }
        
        function logout() {
            fetch('/api/logout', { 
                method: 'POST',
                credentials: 'same-origin'
            })
            .then(() => location.href = '/login');
        }
    </script>
</body>
</html>
)rawliteral";

#endif // SETTINGS_PAGE_H
