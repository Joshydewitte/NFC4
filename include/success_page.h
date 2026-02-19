#ifndef SUCCESS_PAGE_H
#define SUCCESS_PAGE_H

const char SUCCESS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Verbinding Succesvol</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
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
            text-align: center;
        }
        .success-icon {
            font-size: 80px;
            margin-bottom: 20px;
            animation: scaleIn 0.5s ease-out;
        }
        @keyframes scaleIn {
            from { transform: scale(0); }
            to { transform: scale(1); }
        }
        h1 {
            color: #11998e;
            margin-bottom: 15px;
            font-size: 28px;
        }
        .message {
            color: #666;
            margin-bottom: 30px;
            font-size: 16px;
            line-height: 1.6;
        }
        .info-box {
            background: #f0fff4;
            border: 2px solid #11998e;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
        }
        .info-label {
            color: #666;
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 5px;
        }
        .info-value {
            color: #11998e;
            font-size: 24px;
            font-weight: 600;
            font-family: 'Courier New', monospace;
            word-break: break-all;
        }
        .note {
            color: #999;
            font-size: 14px;
            margin-top: 20px;
            padding-top: 20px;
            border-top: 1px solid #e0e0e0;
        }
        .countdown {
            color: #11998e;
            font-weight: 600;
            font-size: 18px;
            margin-top: 15px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="success-icon">✅</div>
        <h1>Verbinding Succesvol!</h1>
        <p class="message">
            Je ESP32 is succesvol verbonden met het WiFi netwerk.<br>
            De configuratie portal wordt nu afgesloten.
        </p>
        
        <div class="info-box">
            <div class="info-label">Nieuw IP-Adres</div>
            <div class="info-value">%IP_ADDRESS%</div>
        </div>
        
        <div class="info-box">
            <div class="info-label">Verbonden met</div>
            <div class="info-value" style="font-size:20px;">%SSID%</div>
        </div>
        
        <div class="note">
            ℹ️ Dit venster sluit over <span class="countdown" id="countdown">10</span> seconden.<br>
            Je ESP32 is nu klaar voor gebruik op het nieuwe netwerk.
        </div>
    </div>

    <script>
        let seconds = 10;
        const countdownElement = document.getElementById('countdown');
        
        const interval = setInterval(() => {
            seconds--;
            countdownElement.textContent = seconds;
            
            if (seconds <= 0) {
                clearInterval(interval);
                window.close();
            }
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

#endif // SUCCESS_PAGE_H
