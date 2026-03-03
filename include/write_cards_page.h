#ifndef WRITE_CARDS_PAGE_H
#define WRITE_CARDS_PAGE_H

const char WRITE_CARDS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Schrijf Kaarten - NFC Configuratie</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }

        .container {
            background: white;
            border-radius: 20px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            padding: 40px;
            max-width: 600px;
            width: 100%;
        }

        h1 {
            color: #2d3748;
            margin-bottom: 10px;
            font-size: 28px;
        }

        .subtitle {
            color: #718096;
            margin-bottom: 30px;
            font-size: 14px;
        }

        .secret-section {
            background: #f7fafc;
            border: 2px dashed #cbd5e0;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 25px;
        }

        .secret-label {
            font-weight: 600;
            color: #2d3748;
            margin-bottom: 8px;
            display: block;
        }

        .secret-input-wrapper {
            position: relative;
            margin-bottom: 10px;
        }

        #masterSecret {
            width: 100%;
            padding: 12px  42px 12px 12px;
            border: 2px solid #e2e8f0;
            border-radius: 8px;
            font-size: 16px;
            font-family: 'Courier New', monospace;
            transition: all 0.3s;
        }

        #masterSecret:focus {
            outline: none;
            border-color: #667eea;
            box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
        }

        .toggle-visibility {
            position: absolute;
            right: 12px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            cursor: pointer;
            font-size: 20px;
            color: #718096;
            padding: 0;
            width: 30px;
            height: 30px;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .toggle-visibility:hover {
            color: #667eea;
        }

        .secret-help {
            font-size: 12px;
            color: #718096;
            margin-top: 8px;
        }

        .mode-selector {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 25px;
        }

        .mode-option {
            background: #f7fafc;
            border: 2px solid #e2e8f0;
            border-radius: 12px;
            padding: 20px;
            cursor: pointer;
            transition: all 0.3s;
            text-align: center;
        }

        .mode-option:hover {
            border-color: #cbd5e0;
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(0,0,0,0.1);
        }

        .mode-option.active {
            border-color: #667eea;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            box-shadow: 0 8px 20px rgba(102, 126, 234, 0.3);
        }

        .mode-icon {
            font-size: 32px;
            margin-bottom: 10px;
        }

        .mode-title {
            font-weight: 600;
            margin-bottom: 5px;
        }

        .mode-desc {
            font-size: 12px;
            opacity: 0.8;
        }

        .mode-option.active .mode-desc {
            opacity: 1;
        }

        .status-section {
            background: #edf2f7;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 25px;
            display: none;
        }

        .status-section.visible {
            display: block;
        }

        .status-card {
            background: white;
            border-radius: 8px;
            padding: 15px;
            margin-bottom: 12px;
        }

        .status-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
        }

        .card-uid {
            font-family: 'Courier New', monospace;
            font-weight: 600;
            color: #2d3748;
        }

        .status-badge {
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 600;
        }

        .status-badge.success {
            background: #c6f6d5;
            color: #22543d;
        }

        .status-badge.error {
            background: #fed7d7;
            color: #742a2a;
        }

        .status-badge.processing {
            background: #bee3f8;
            color: #2c5282;
        }

        .status-message {
            font-size: 14px;
            color: #4a5568;
        }

        .button-group {
            display: flex;
            gap: 12px;
        }

        .btn {
            flex: 1;
            padding: 14px 24px;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
        }

        .btn-primary {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.3);
        }

        .btn-primary:hover {
            box-shadow: 0 6px 20px rgba(102, 126, 234, 0.4);
            transform: translateY(-2px);
        }

        .btn-secondary {
            background: white;
            color: #667eea;
            border: 2px solid #667eea;
        }

        .btn-secondary:hover {
            background: #f7fafc;
        }

        .btn-danger {
            background: #f56565;
            color: white;
        }

        .btn-danger:hover {
            background: #e53e3e;
        }

        .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
            transform: none !important;
        }

        .stats-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 12px;
            margin-top: 20px;
        }

        .stat-box {
            background: white;
            border-radius: 8px;
            padding: 15px;
            text-align: center;
        }

        .stat-value {
            font-size: 24px;
            font-weight: 700;
            color: #667eea;
        }

        .stat-label {
            font-size: 12px;
            color: #718096;
            margin-top: 4px;
        }

        .log-section {
            margin-top: 20px;
            background: #1a202c;
            border-radius: 8px;
            padding: 15px;
            max-height: 300px;
            overflow-y: auto;
            font-family: 'Courier New', monospace;
            font-size: 12px;
        }

        .log-entry {
            padding: 4px 0;
            color: #cbd5e0;
        }

        .log-entry.info { color: #63b3ed; }
        .log-entry.success { color: #68d391; }
        .log-entry.error { color: #fc8181; }
        .log-entry.warning { color: #f6ad55; }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        .processing {
            animation: pulse 1.5s ease-in-out infinite;
        }

        .back-link {
            display: inline-block;
            color: #667eea;
            text-decoration: none;
            margin-bottom: 20px;
            font-weight: 500;
        }

        .back-link:hover {
            text-decoration: underline;
        }
    </style>
</head>
<body>
    <div class="container">
        <a href="/settings" class="back-link">← Terug naar instellingen</a>
        
        <h1>📝 Kaarten Schrijven</h1>
        <p class="subtitle">⚠️ LET OP: Schrijven kan NIET ongedaan worden gemaakt!</p>

        <!-- Key Source Selection -->
        <div class="secret-section">
            <label class="secret-label">🔑 Key Bron</label>
            <div class="mode-selector">
                <div class="mode-option active" data-source="esp32">
                    <div class="mode-icon">📱</div>
                    <div class="mode-title">ESP32 Mode</div>
                    <div class="mode-desc">Gebruik lokaal ingevuld secret</div>
                </div>
                <div class="mode-option" data-source="server">
                    <div class="mode-icon">🌐</div>
                    <div class="mode-title">Server Mode</div>
                    <div class="mode-desc">Haal keys van server per UID</div>
                </div>
            </div>
        </div>

        <!-- Master Secret Input (Only for ESP32 Mode) -->
        <div class="secret-section" id="masterSecretSection">
            <label class="secret-label">🔐 Master Secret (32 hex karakters = 16 bytes)</label>
            <div class="secret-input-wrapper">
                <input type="password" id="masterSecret" placeholder="00000000000000000000000000000000" maxlength="32" style="text-transform: uppercase;" />
                <button class="toggle-visibility" id="toggleSecret" type="button">👁️</button>
            </div>
            <div class="secret-help">
                ℹ️ Exact 32 hex karakters vereist (0-9, A-F). Dit geheim wordt ALLEEN in geheugen gebruikt.
            </div>
        </div>

        <!-- Card Type Selection -->
        <div class="secret-section">
            <label class="secret-label">🏭 Kaart Type</label>
            <div style="margin-bottom: 10px;">
                <label style="display: flex; align-items: center; gap: 8px; cursor: pointer;">
                    <input type="checkbox" id="isFactoryCard" checked style="width: 18px; height: 18px;" />
                    <span>Factory kaart (standaard 0x00...00 key)</span>
                </label>
            </div>
            <div id="previousKeySection" style="display: none;">
                <label class="secret-label">🔑 Vorige Key (32 hex karakters = 16 bytes)</label>
                <div class="secret-input-wrapper">
                    <input type="password" id="previousKey" placeholder="00000000000000000000000000000000" maxlength="32" style="text-transform: uppercase;" />
                    <button class="toggle-visibility" id="togglePrevKey" type="button">👁️</button>
                </div>
                <div class="secret-help">
                    ℹ️ Exact 32 hex karakters vereist (0-9, A-F). De key die momenteel op de kaart staat.
                </div>
            </div>
        </div>

        <!-- Write Mode Selection -->
        <div class="secret-section">
            <label class="secret-label">📝 Schrijf Modus</label>
            <div class="mode-selector">
                <div class="mode-option active" data-mode="single">
                    <div class="mode-icon">🎯</div>
                    <div class="mode-title">Enkele Kaart</div>
                    <div class="mode-desc">Schrijf één kaart en stop</div>
                </div>
                <div class="mode-option" data-mode="continuous">
                    <div class="mode-icon">♾️</div>
                    <div class="mode-title">Continue Modus</div>
                    <div class="mode-desc">Blijf schrijven voor elke kaart</div>
                </div>
            </div>
        </div>

        <!-- Status Section -->
        <div class="status-section" id="statusSection">
            <div id="statusCards"></div>
            
            <div class="stats-grid">
                <div class="stat-box">
                    <div class="stat-value" id="statSuccess">0</div>
                    <div class="stat-label">Succesvol</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value" id="statFailed">0</div>
                    <div class="stat-label">Mislukt</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value" id="statTotal">0</div>
                    <div class="stat-label">Totaal</div>
                </div>
            </div>
        </div>

        <!-- Action Buttons -->
        <div class="button-group">
            <button class="btn btn-primary" id="startBtn">
                ▶️ Start Schrijven
            </button>
            <button class="btn btn-danger" id="stopBtn" style="display: none;">
                ⏹️ Stop
            </button>
        </div>

        <!-- Log Section -->
        <div class="log-section" id="logSection"></div>
    </div>

    <script>
        let ws = null;
        let isRunning = false;
        let currentMode = 'single';
        let keySource = 'esp32';  // NEW: esp32 or server
        let stats = { success: 0, failed: 0, total: 0 };

        // Initialize WebSocket connection
        function initWebSocket() {
            ws = new WebSocket('ws://' + window.location.hostname + '/ws');
            
            ws.onopen = function() {
                addLog('WebSocket verbonden', 'success');
            };
            
            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    handleWebSocketMessage(data);
                } catch(e) {
                    addLog(event.data, 'info');
                }
            };
            
            ws.onclose = function() {
                addLog('WebSocket verbinding verloren, herverbinden...', 'warning');
                setTimeout(initWebSocket, 3000);
            };
            
            ws.onerror = function(error) {
                addLog('WebSocket error: ' + error, 'error');
            };
        }

        // Handle WebSocket messages
        function handleWebSocketMessage(data) {
            if (data.type === 'write_card_status') {
                updateCardStatus(data);
            } else if (data.type === 'log') {
                addLog(data.message, data.level || 'info');
            }
        }

        // Update card status
        function updateCardStatus(data) {
            const statusCards = document.getElementById('statusCards');
            
            let cardElement = document.getElementById('card-' + data.uid);
            if (!cardElement) {
                cardElement = document.createElement('div');
                cardElement.className = 'status-card';
                cardElement.id = 'card-' + data.uid;
                statusCards.appendChild(cardElement);
            }
            
            let badgeClass = 'processing';
            let badgeText = 'Bezig...';
            
            if (data.status === 'success') {
                badgeClass = 'success';
                badgeText = '✅ Succesvol';
                stats.success++;
            } else if (data.status === 'error') {
                badgeClass = 'error';
                badgeText = '❌ Mislukt';
                stats.failed++;
            }
            
            stats.total++;
            updateStats();
            
            cardElement.innerHTML = `
                <div class="status-header">
                    <span class="card-uid">${data.uid}</span>
                    <span class="status-badge ${badgeClass}">${badgeText}</span>
                </div>
                <div class="status-message">${data.message}</div>
            `;
            
            // Show status section
            document.getElementById('statusSection').classList.add('visible');
        }

        // Update statistics
        function updateStats() {
            document.getElementById('statSuccess').textContent = stats.success;
            document.getElementById('statFailed').textContent = stats.failed;
            document.getElementById('statTotal').textContent = stats.total;
        }

        // Add log entry
        function addLog(message, level = 'info') {
            const logSection = document.getElementById('logSection');
            const entry = document.createElement('div');
            entry.className = 'log-entry ' + level;
            const timestamp = new Date().toLocaleTimeString('nl-NL');
            entry.textContent = `[${timestamp}] ${message}`;
            logSection.appendChild(entry);
            logSection.scrollTop = logSection.scrollHeight;
        }

        // Key Source selection (ESP32 vs Server)
        document.querySelectorAll('[data-source]').forEach(option => {
            option.addEventListener('click', function() {
                if (isRunning) return;
                
                document.querySelectorAll('[data-source]').forEach(opt => {
                    opt.classList.remove('active');
                });
                this.classList.add('active');
                keySource = this.dataset.source;
                
                // Show/hide master secret section based on source
                const masterSecretSection = document.getElementById('masterSecretSection');
                if (keySource === 'esp32') {
                    masterSecretSection.style.display = 'block';
                    addLog('Key bron: ESP32 (lokaal secret)', 'info');
                } else {
                    masterSecretSection.style.display = 'none';
                    addLog('Key bron: Server (haal keys per UID)', 'info');
                }
            });
        });

        // Write Mode selection (Single vs Continuous)
        document.querySelectorAll('[data-mode]').forEach(option => {
            option.addEventListener('click', function() {
                if (isRunning) return;
                
                document.querySelectorAll('[data-mode]').forEach(opt => {
                    opt.classList.remove('active');
                });
                this.classList.add('active');
                currentMode = this.dataset.mode;
                addLog(`Schrijf modus: ${currentMode === 'single' ? 'Enkele Kaart' : 'Continue'}`, 'info');
            });
        });

        // Toggle secret visibility
        document.getElementById('toggleSecret').addEventListener('click', function() {
            const input = document.getElementById('masterSecret');
            if (input.type === 'password') {
                input.type = 'text';
                this.textContent = '🙈';
            } else {
                input.type = 'password';
                this.textContent = '👁️';
            }
        });

        // Toggle previous key visibility
        document.getElementById('togglePrevKey').addEventListener('click', function() {
            const input = document.getElementById('previousKey');
            if (input.type === 'password') {
                input.type = 'text';
                this.textContent = '🙈';
            } else {
                input.type = 'password';
                this.textContent = '👁️';
            }
        });

        // Hex input filtering for masterSecret (alleen 0-9, A-F, max 32 chars)
        document.getElementById('masterSecret').addEventListener('input', function(e) {
            let value = e.target.value.toUpperCase();
            // Verwijder alles behalve 0-9 en A-F
            value = value.replace(/[^0-9A-F]/g, '');
            // Limiteer tot 32 karakters
            if (value.length > 32) {
                value = value.substring(0, 32);
            }
            e.target.value = value;
        });

        // Hex input filtering for previousKey (alleen 0-9, A-F, max 32 chars)
        document.getElementById('previousKey').addEventListener('input', function(e) {
            let value = e.target.value.toUpperCase();
            // Verwijder alles behalve 0-9 en A-F
            value = value.replace(/[^0-9A-F]/g, '');
            // Limiteer tot 32 karakters
            if (value.length > 32) {
                value = value.substring(0, 32);
            }
            e.target.value = value;
        });

        // Show/hide previous key section based on factory card checkbox
        document.getElementById('isFactoryCard').addEventListener('change', function() {
            const prevKeySection = document.getElementById('previousKeySection');
            prevKeySection.style.display = this.checked ? 'none' : 'block';
        });

        // Start writing with CONFIRMATION
        document.getElementById('startBtn').addEventListener('click', async function() {
            const masterSecret = document.getElementById('masterSecret').value.trim();
            const isFactory = document.getElementById('isFactoryCard').checked;
            const previousKey = document.getElementById('previousKey').value.trim();
            
            // Validation for ESP32 mode
            if (keySource === 'esp32') {
                if (!masterSecret) {
                    alert('⚠️ Voer eerst een master secret in!');
                    return;
                }
                
                if (masterSecret.length !== 32) {
                    alert('⚠️ Master secret moet exact 32 hex karakters zijn (16 bytes)!');
                    return;
                }
                
                if (!/^[0-9A-Fa-f]{32}$/.test(masterSecret)) {
                    alert('⚠️ Master secret moet een geldige hex string zijn (0-9, A-F)');
                    return;
                }
            }

            // Validation for personalized cards (both modes)
            if (!isFactory) {
                if (!previousKey) {
                    alert('⚠️ Voer de vorige key in!');
                    return;
                }
                
                if (previousKey.length !== 32) {
                    alert(`⚠️ Vorige key moet exact 32 hex karakters zijn (16 bytes)\\nJe hebt ${previousKey.length} karakters ingevoerd.`);
                    return;
                }
                
                if (!/^[0-9A-Fa-f]{32}$/.test(previousKey)) {
                    alert('⚠️ Vorige key moet een geldige hex string zijn (0-9, A-F)');
                    return;
                }
            }
            
            // CRITICAL: CONFIRMATION DIALOG
            const sourceName = keySource === 'esp32' ? 'ESP32 lokaal secret' : 'Server keys (per UID)';
            const modeName = currentMode === 'single' ? 'enkele kaart' : 'continue mode';
            const cardType = isFactory ? 'Factory kaarten (0x00...00)' : 'Gepersonaliseerde kaarten';
            
            let confirmMessage = `🚨 BEVESTIG SCHRIJVEN 🚨\\n\\n`;
            confirmMessage += `Key bron: ${sourceName}\\n`;
            confirmMessage += `Schrijf modus: ${modeName}\\n`;
            confirmMessage += `Kaart type: ${cardType}\\n\\n`;
            
            if (keySource === 'esp32') {
                confirmMessage += `Secret (eerste 8): ${masterSecret.substring(0, 8)}...\\n\\n`;
            }
            
            confirmMessage += `⚠️ DIT KAN NIET ONGEDAAN WORDEN!\\n\\n`;
            confirmMessage += `Weet je ZEKER dat je wilt starten?`;
            
            if (!confirm(confirmMessage)) {
                addLog('❌ Schrijven geannuleerd door gebruiker', 'warning');
                return;
            }
            
            // SECOND CONFIRMATION FOR ESP32 MODE WITH MASTER SECRET
            if (keySource === 'esp32') {
                let secondConfirm = `🔐 VERIFIEER MASTER SECRET 🔐\\n\\n`;
                secondConfirm += `Je gaat het volgende master secret gebruiken:\\n`;
                secondConfirm += `\\n"${masterSecret}"\\n\\n`;
                secondConfirm += `⚠️ CONTROLEER DIT ZORGVULDIG!\\n`;
                secondConfirm += `Dit secret wordt gebruikt om keys af te leiden.\\n\\n`;
                secondConfirm += `Klopt dit master secret?`;
                
                if (!confirm(secondConfirm)) {
                    addLog('❌ Master secret verificatie geannuleerd', 'warning');
                    return;
                }
                
                addLog('✅ Master secret geverifieerd door gebruiker', 'success');
            }
            
            // User confirmed, proceed
            isRunning = true;
            this.style.display = 'none';
            document.getElementById('stopBtn').style.display = 'flex';
            
            addLog('✅ Gebruiker heeft schrijven bevestigd', 'success');
            addLog(`Key bron: ${sourceName}`, 'info');
            addLog(`Modus: ${modeName}`, 'info');
            
            // Send start command to ESP32
            try {
                const requestBody = {
                    keySource: keySource,
                    masterSecret: keySource === 'esp32' ? masterSecret : '',
                    mode: currentMode,
                    isFactory: isFactory,
                    previousKey: isFactory ? '' : previousKey  // NEVER send null, always string
                };
                
                console.log('Request body:', requestBody);
                
                const response = await fetch('/api/write/start', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(requestBody)
                });
                
                const result = await response.json();
                console.log('Server response:', result);
                
                if (result.success) {
                    addLog(result.message, 'success');
                } else {
                    addLog('❌ Server fout: ' + result.message, 'error');
                    console.error('Server error details:', result);
                    stopWriting();
                }
            } catch (error) {
                addLog('Fout bij starten: ' + error, 'error');
                console.error('Fetch error:', error);
                stopWriting();
            }
        });

        // Stop writing
        document.getElementById('stopBtn').addEventListener('click', async function() {
            try {
                const response = await fetch('/api/write/stop', {
                    method: 'POST'
                });
                
                const result = await response.json();
                addLog(result.message, 'info');
            } catch (error) {
                addLog('Fout bij stoppen: ' + error, 'error');
            }
            
            stopWriting();
        });

        function stopWriting() {
            isRunning = false;
            document.getElementById('startBtn').style.display = 'flex';
            document.getElementById('stopBtn').style.display = 'none';
        }

        // Stop writing when leaving page
        window.addEventListener('beforeunload', async function(e) {
            if (isRunning) {
                // Send stop request (using sendBeacon for reliability during unload)
                const data = new Blob([JSON.stringify({})], { type: 'application/json' });
                navigator.sendBeacon('/api/write/stop', data);
            }
        });

        // Initialize on page load
        window.addEventListener('load', async function() {
            // IMPORTANT: Stop any active write mode from previous session
            try {
                await fetch('/api/write/stop', { method: 'POST' });
                console.log('Cleaned up any previous write mode');
            } catch (error) {
                console.log('Cleanup request failed (may not be active):', error);
            }
            
            initWebSocket();
            addLog('📝 Schrijf-modus geïnitialiseerd', 'success');
            addLog('⚠️ CONFIG MODE SCHRIJFT NIET - gebruik deze pagina!', 'warning');
        });
    </script>
</body>
</html>
)rawliteral";

#endif // WRITE_CARDS_PAGE_H
