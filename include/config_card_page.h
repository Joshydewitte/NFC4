#ifndef CONFIG_CARD_PAGE_H
#define CONFIG_CARD_PAGE_H

const char CONFIG_CARD_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Kaart Personaliseren</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #f5f5f5;
            padding: 20px;
        }
        .header {
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
            color: white;
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .header h1 { font-size: 24px; }
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
            color: #f5576c;
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
            background: #f5576c;
            margin-right: 10px;
            border-radius: 2px;
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
        .warning-box {
            background: #fff8e1;
            border-left: 4px solid #ffa500;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            font-size: 14px;
            color: #555;
        }
        .success-box {
            background: #d4edda;
            border-left: 4px solid #4caf50;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            font-size: 14px;
            color: #155724;
        }
        .error-box {
            background: #f8d7da;
            border-left: 4px solid #f44336;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            font-size: 14px;
            color: #721c24;
        }
        .step-indicator {
            display: flex;
            justify-content: space-between;
            margin-bottom: 30px;
            padding: 0 20px;
        }
        .step {
            flex: 1;
            text-align: center;
            position: relative;
        }
        .step::before {
            content: '';
            position: absolute;
            top: 20px;
            left: 50%;
            width: 100%;
            height: 2px;
            background: #e0e0e0;
            z-index: 0;
        }
        .step:first-child::before {
            display: none;
        }
        .step-number {
            width: 40px;
            height: 40px;
            border-radius: 50%;
            background: #e0e0e0;
            color: #999;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            font-weight: 600;
            margin-bottom: 10px;
            position: relative;
            z-index: 1;
        }
        .step.active .step-number {
            background: #667eea;
            color: white;
        }
        .step.completed .step-number {
            background: #4caf50;
            color: white;
        }
        .step-number::after {
            content: '✓';
            display: none;
        }
        .step.completed .step-number::after {
            display: block;
        }
        .step.completed .step-number span {
            display: none;
        }
        .step-label {
            font-size: 12px;
            color: #666;
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
            margin-right: 10px;
        }
        button:hover:not(:disabled) {
            transform: translateY(-2px);
        }
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        .button-group {
            display: flex;
            gap: 10px;
            margin-top: 20px;
        }
        .cancel-btn {
            background: #999;
        }
        .card-display {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
            text-align: center;
        }
        .card-uid {
            font-size: 24px;
            font-weight: 600;
            margin: 10px 0;
            font-family: 'Courier New', monospace;
        }
        .card-type {
            font-size: 14px;
            opacity: 0.9;
        }
        .progress-spinner {
            border: 4px solid #f3f3f3;
            border-top: 4px solid #667eea;
            border-radius: 50%;
            width: 40px;
            height: 40px;
            animation: spin 1s linear infinite;
            display: inline-block;
            margin: 20px auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .hidden {
            display: none;
        }
        #logOutput {
            background: #1e1e1e;
            color: #d4d4d4;
            padding: 15px;
            border-radius: 5px;
            font-family: 'Courier New', monospace;
            font-size: 12px;
            max-height: 300px;
            overflow-y: auto;
            margin-top: 20px;
        }
        .log-entry {
            padding: 2px 0;
            line-height: 1.6;
        }
        .log-level-info { color: #4fc3f7; }
        .log-level-success { color: #4caf50; }
        .log-level-warning { color: #ffa500; }
        .log-level-error { color: #f44336; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🔧 Kaart Personaliseren (Config Mode)</h1>
        <a href="/status" class="nav-btn">← Terug</a>
    </div>

    <div class="card">
        <div class="step-indicator">
            <div class="step active" id="step1">
                <div class="step-number"><span>1</span></div>
                <div class="step-label">Kaart detecteren</div>
            </div>
            <div class="step" id="step2">
                <div class="step-number"><span>2</span></div>
                <div class="step-label">Key ophalen</div>
            </div>
            <div class="step" id="step3">
                <div class="step-number"><span>3</span></div>
                <div class="step-label">Authenticeren</div>
            </div>
            <div class="step" id="step4">
                <div class="step-number"><span>4</span></div>
                <div class="step-label">Key schrijven</div>
            </div>
            <div class="step" id="step5">
                <div class="step-number"><span>5</span></div>
                <div class="step-label">Verificatie</div>
            </div>
        </div>
    </div>

    <div class="card">
        <h2>Workflow Instructies</h2>
        
        <div class="info-box" id="infoBox">
            <strong>ℹ️ Stap 1: Kaart detecteren</strong><br>
            Houd een NTAG424 DNA kaart tegen de reader om te beginnen.
        </div>

        <div id="cardDisplay" class="hidden">
            <div class="card-display">
                <div>🔖 Kaart Gedetecteerd</div>
                <div class="card-uid" id="cardUid">-</div>
                <div class="card-type" id="cardType">-</div>
            </div>
        </div>

        <div id="progressSection" class="hidden">
            <div style="text-align: center;">
                <div class="progress-spinner"></div>
                <p id="progressText">Bezig...</p>
            </div>
        </div>

        <div id="resultBox" class="hidden"></div>

        <div class="button-group">
            <button id="startBtn" onclick="startPersonalization()">🚀 Start Personalisatie</button>
            <button id="retryBtn" class="hidden" onclick="retryPersonalization()">🔄 Opnieuw Proberen</button>
            <button id="nextBtn" class="hidden" onclick="nextCard()">➡️ Volgende Kaart</button>
            <button class="cancel-btn" onclick="location.href='/status'">❌ Annuleren</button>
        </div>

        <div id="logOutput"></div>
    </div>

    <script>
        let ws;
        let currentStep = 1;
        let detectedCard = null;

        function connectWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const hostname = window.location.hostname;
            const wsUrl = protocol + '//' + hostname + ':81/';
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                addLog('✅ WebSocket verbonden', 'success');
            };
            
            ws.onmessage = function(event) {
                handleMessage(event.data);
            };
            
            ws.onerror = function(error) {
                addLog('❌ WebSocket error', 'error');
            };
            
            ws.onclose = function() {
                addLog('⚠️ WebSocket disconnected - attempting reconnect...', 'warning');
                setTimeout(connectWebSocket, 3000);
            };
        }

        function handleMessage(data) {
            try {
                const msg = JSON.parse(data);
                
                if (msg.type === 'log') {
                    addLog(msg.data.message, msg.data.level);
                }
                
                if (msg.type === 'card_detected') {
                    onCardDetected(msg.data);
                }
                
                if (msg.type === 'personalization_progress') {
                    onProgress(msg.data);
                }
                
                if (msg.type === 'personalization_complete') {
                    onComplete(msg.data);
                }
                
                if (msg.type === 'personalization_error') {
                    onError(msg.data);
                }
            } catch (e) {
                // Ignore non-JSON messages
            }
        }

        function onCardDetected(data) {
            detectedCard = data;
            document.getElementById('cardUid').textContent = data.uid;
            document.getElementById('cardType').textContent = data.type;
            document.getElementById('cardDisplay').classList.remove('hidden');
            document.getElementById('startBtn').disabled = false;
            setStep(1, 'completed');
            addLog('🔖 Kaart gedetecteerd: ' + data.uid, 'success');
        }

        function startPersonalization() {
            if (!detectedCard) {
                showError('Geen kaart gedetecteerd!');
                return;
            }

            document.getElementById('startBtn').classList.add('hidden');
            document.getElementById('progressSection').classList.remove('hidden');
            document.getElementById('resultBox').classList.add('hidden');

            setStep(2, 'active');
            updateInfo('🔑 Keys ophalen van server...');

            // Request personalization via API
            fetch('/api/personalize/start', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'same-origin',
                body: JSON.stringify({ uid: detectedCard.uid })
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    addLog('✅ Personalisatie gestart', 'success');
                } else {
                    showError(data.error || 'Fout bij starten personalisatie');
                }
            })
            .catch(e => {
                showError('Server fout: ' + e.message);
            });
        }

        function onProgress(data) {
            setStep(data.step, 'active');
            updateInfo(data.message);
            addLog(data.message, 'info');
        }

        function onComplete(data) {
            setStep(5, 'completed');
            document.getElementById('progressSection').classList.add('hidden');
            
            const resultBox = document.getElementById('resultBox');
            resultBox.className = 'success-box';
            resultBox.innerHTML = `
                <strong>✅ Kaart succesvol gepersonaliseerd!</strong><br>
                UID: ${detectedCard.uid}<br>
                Master key is geschreven naar de kaart.
            `;
            resultBox.classList.remove('hidden');

            document.getElementById('nextBtn').classList.remove('hidden');
            addLog('✅ Personalisatie voltooid!', 'success');
        }

        function onError(data) {
            document.getElementById('progressSection').classList.add('hidden');
            showError(data.error || 'Onbekende fout');
            document.getElementById('retryBtn').classList.remove('hidden');
        }

        function showError(message) {
            const resultBox = document.getElementById('resultBox');
            resultBox.className = 'error-box';
            resultBox.innerHTML = `<strong>❌ Fout:</strong><br>${message}`;
            resultBox.classList.remove('hidden');
            addLog('❌ ' + message, 'error');
        }

        function retryPersonalization() {
            location.reload();
        }

        function nextCard() {
            location.reload();
        }

        function setStep(stepNum, status) {
            const step = document.getElementById('step' + stepNum);
            if (step) {
                step.className = 'step ' + status;
            }
        }

        function updateInfo(message) {
            const infoBox = document.getElementById('infoBox');
            infoBox.innerHTML = '<strong>ℹ️</strong> ' + message;
        }

        function addLog(message, level = 'info') {
            const logOutput = document.getElementById('logOutput');
            const entry = document.createElement('div');
            entry.className = 'log-entry log-level-' + level;
            
            const timestamp = new Date().toLocaleTimeString('nl-NL');
            entry.textContent = `[${timestamp}] ${message}`;
            
            logOutput.appendChild(entry);
            logOutput.scrollTop = logOutput.scrollHeight;
        }

        // Initialize
        connectWebSocket();
        addLog('Wachten op kaart...', 'info');

        // Poll for card detection
        setInterval(function() {
            if (!detectedCard) {
                fetch('/api/card/current', {
                    credentials: 'same-origin'
                })
                .then(r => r.json())
                .then(data => {
                    if (data.present && !detectedCard) {
                        onCardDetected(data);
                    }
                })
                .catch(e => {
                    // Silently fail - card not present
                });
            }
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

#endif // CONFIG_CARD_PAGE_H
