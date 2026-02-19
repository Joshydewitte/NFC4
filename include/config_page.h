#ifndef CONFIG_PAGE_H
#define CONFIG_PAGE_H

const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Configuratie</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            max-width: 500px;
            width: 100%;
            padding: 40px 30px;
        }
        h1 {
            color: #333;
            margin-bottom: 10px;
            font-size: 28px;
            text-align: center;
        }
        .subtitle {
            color: #666;
            text-align: center;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .section {
            margin-bottom: 25px;
        }
        .section-title {
            font-weight: 600;
            color: #667eea;
            margin-bottom: 15px;
            font-size: 16px;
            display: flex;
            align-items: center;
        }
        .section-title::before {
            content: '';
            width: 4px;
            height: 20px;
            background: #667eea;
            margin-right: 10px;
            border-radius: 2px;
        }
        button {
            width: 100%;
            padding: 15px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
            margin-bottom: 10px;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4);
        }
        button:active {
            transform: translateY(0);
        }
        .scan-btn {
            background: linear-gradient(135deg, #48c6ef 0%, #6f86d6 100%);
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 12px 15px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 14px;
            transition: border-color 0.3s;
            margin-bottom: 15px;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #555;
            font-weight: 500;
            font-size: 14px;
        }
        .form-group {
            margin-bottom: 20px;
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
        .spinner {
            display: none;
            width: 40px;
            height: 40px;
            margin: 20px auto;
            border: 4px solid #f3f3f3;
            border-top: 4px solid #667eea;
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .network-list {
            max-height: 400px;
            overflow-y: auto;
        }
        .network-item {
            padding: 15px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            margin-bottom: 10px;
            cursor: pointer;
            transition: all 0.3s;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .network-item:hover {
            border-color: #667eea;
            background: #f8f9ff;
        }
        .network-name {
            font-weight: 600;
            color: #333;
        }
        .network-signal {
            color: #666;
            font-size: 12px;
        }
        .network-secure {
            color: #764ba2;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔧 WiFi Configuratie</h1>
        <p class="subtitle">Configureer je ESP32 NFC Reader</p>
        
        <div class="info-box">
            <strong>📡 Setup Modus</strong><br>
            Scan voor beschikbare netwerken of voer handmatig je WiFi gegevens in.
        </div>

        <div class="section">
            <div class="section-title">Netwerk Scannen</div>
            <button class="scan-btn" onclick="scanNetworks()">🔍 Scan WiFi Netwerken</button>
            <div class="spinner" id="spinner"></div>
            <div id="networks"></div>
        </div>

        <div class="section">
            <div class="section-title">Handmatige Configuratie</div>
            <form action="/connect" method="POST">
                <div class="form-group">
                    <label for="ssid">WiFi Netwerk (SSID)</label>
                    <input type="text" id="ssid" name="ssid" placeholder="Voer SSID in" required>
                </div>
                <div class="form-group">
                    <label for="password">Wachtwoord</label>
                    <input type="password" id="password" name="password" placeholder="Voer wachtwoord in">
                </div>
                <button type="submit">✅ Verbinden</button>
            </form>
        </div>
    </div>

    <script>
        function scanNetworks() {
            document.getElementById('spinner').style.display = 'block';
            document.getElementById('networks').innerHTML = '';
            
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('spinner').style.display = 'none';
                    const networksDiv = document.getElementById('networks');
                    networksDiv.innerHTML = '<div class="network-list">';
                    
                    if (data.networks && data.networks.length > 0) {
                        data.networks.forEach(network => {
                            const signalStrength = network.rssi > -50 ? 'Excellent' : 
                                                 network.rssi > -60 ? 'Goed' : 
                                                 network.rssi > -70 ? 'Redelijk' : 'Zwak';
                            const secure = network.encryption !== 'Open' ? '🔒 Beveiligd' : '🔓 Open';
                            
                            networksDiv.innerHTML += `
                                <div class="network-item" onclick="selectNetwork('${network.ssid}', ${network.encryption !== 'Open'})">
                                    <div>
                                        <div class="network-name">${network.ssid}</div>
                                        <div class="network-signal">Signaal: ${signalStrength} (${network.rssi} dBm)</div>
                                    </div>
                                    <div class="network-secure">${secure}</div>
                                </div>
                            `;
                        });
                    } else {
                        networksDiv.innerHTML += '<p style="text-align:center;color:#999;">Geen netwerken gevonden</p>';
                    }
                    
                    networksDiv.innerHTML += '</div>';
                })
                .catch(error => {
                    document.getElementById('spinner').style.display = 'none';
                    alert('Fout bij scannen: ' + error);
                });
        }
        
        function selectNetwork(ssid, isSecure) {
            document.getElementById('ssid').value = ssid;
            if (isSecure) {
                document.getElementById('password').focus();
            }
        }
    </script>
</body>
</html>
)rawliteral";

#endif // CONFIG_PAGE_H
