#ifndef CONNECTING_PAGE_H
#define CONNECTING_PAGE_H

const char CONNECTING_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Verbinden...</title>
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
            text-align: center;
        }
        .spinner-container {
            margin: 30px 0;
        }
        .spinner {
            width: 60px;
            height: 60px;
            margin: 0 auto;
            border: 6px solid #f3f3f3;
            border-top: 6px solid #667eea;
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        h1 {
            color: #667eea;
            margin-bottom: 15px;
            font-size: 28px;
        }
        .message {
            color: #666;
            margin-bottom: 20px;
            font-size: 16px;
            line-height: 1.6;
        }
        .ssid {
            color: #764ba2;
            font-weight: 600;
            font-size: 18px;
            margin: 20px 0;
            padding: 15px;
            background: #f8f9ff;
            border-radius: 8px;
        }
        .status {
            color: #999;
            font-size: 14px;
            margin-top: 20px;
        }
        .dots::after {
            content: '...';
            animation: dots 1.5s steps(4, end) infinite;
        }
        @keyframes dots {
            0%, 20% { content: '.'; }
            40% { content: '..'; }
            60%, 100% { content: '...'; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Verbinden met WiFi</h1>
        <div class="spinner-container">
            <div class="spinner"></div>
        </div>
        <p class="message">
            Je ESP32 maakt nu verbinding met het geselecteerde netwerk.
        </p>
        <div class="ssid">%SSID%</div>
        <p class="status">
            Even geduld<span class="dots"></span>
        </p>
    </div>
    
    <script>
        // Automatisch status checken na 8 seconden
        setTimeout(() => {
            window.location.href = '/status';
        }, 8000);
    </script>
</body>
</html>
)rawliteral";

#endif // CONNECTING_PAGE_H
