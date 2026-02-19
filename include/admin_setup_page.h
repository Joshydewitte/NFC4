#ifndef ADMIN_SETUP_PAGE_H
#define ADMIN_SETUP_PAGE_H

const char ADMIN_SETUP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Admin Setup</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
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
            color: #f5576c;
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
        .info-box {
            background: #fff5f5;
            border-left: 4px solid #f5576c;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 25px;
            font-size: 14px;
            color: #555;
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
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 12px 15px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 14px;
            transition: border-color 0.3s;
        }
        input:focus {
            outline: none;
            border-color: #f5576c;
        }
        .password-strength {
            height: 4px;
            background: #e0e0e0;
            border-radius: 2px;
            margin-top: 8px;
            overflow: hidden;
        }
        .password-strength-bar {
            height: 100%;
            width: 0%;
            transition: all 0.3s;
        }
        .strength-weak { background: #f5576c; width: 33%; }
        .strength-medium { background: #ffa500; width: 66%; }
        .strength-strong { background: #4caf50; width: 100%; }
        button {
            width: 100%;
            padding: 15px;
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
            margin-top: 10px;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(245, 87, 108, 0.4);
        }
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        .error {
            color: #f5576c;
            font-size: 14px;
            margin-top: 10px;
            display: none;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔐 Admin Setup</h1>
        <p class="subtitle">Welkom! Stel je admin account in</p>
        
        <div class="info-box">
            <strong>⚠️ Belangrijk</strong><br>
            Dit account krijgt toegang tot alle instellingen. Kies een sterk wachtwoord en bewaar deze goed.
        </div>

        <form id="adminForm" onsubmit="submitAdmin(event)">
            <div class="form-group">
                <label for="username">Gebruikersnaam</label>
                <input type="text" id="username" name="username" placeholder="admin" required minlength="4">
            </div>
            
            <div class="form-group">
                <label for="password">Wachtwoord</label>
                <input type="password" id="password" name="password" placeholder="Minimaal 8 tekens" required minlength="8" oninput="checkStrength()">
                <div class="password-strength">
                    <div class="password-strength-bar" id="strengthBar"></div>
                </div>
            </div>
            
            <div class="form-group">
                <label for="confirm">Bevestig Wachtwoord</label>
                <input type="password" id="confirm" name="confirm" placeholder="Herhaal wachtwoord" required minlength="8">
            </div>
            
            <button type="submit" id="submitBtn">✅ Admin Account Aanmaken</button>
            <div class="error" id="error"></div>
        </form>
    </div>

    <script>
        function checkStrength() {
            const password = document.getElementById('password').value;
            const bar = document.getElementById('strengthBar');
            
            bar.className = 'password-strength-bar';
            
            if (password.length < 8) {
                bar.classList.add('strength-weak');
            } else if (password.length < 12) {
                bar.classList.add('strength-medium');
            } else {
                bar.classList.add('strength-strong');
            }
        }
        
        function submitAdmin(event) {
            event.preventDefault();
            
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            const confirm = document.getElementById('confirm').value;
            const errorDiv = document.getElementById('error');
            const submitBtn = document.getElementById('submitBtn');
            
            errorDiv.style.display = 'none';
            
            if (password !== confirm) {
                errorDiv.textContent = '❌ Wachtwoorden komen niet overeen';
                errorDiv.style.display = 'block';
                return;
            }
            
            if (password.length < 8) {
                errorDiv.textContent = '❌ Wachtwoord moet minimaal 8 tekens bevatten';
                errorDiv.style.display = 'block';
                return;
            }
            
            submitBtn.disabled = true;
            submitBtn.textContent = '⏳ Account aanmaken...';
            
            fetch('/setup-admin', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                credentials: 'same-origin',
                body: 'username=' + encodeURIComponent(username) + '&password=' + encodeURIComponent(password)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    window.location.href = '/login';
                } else {
                    errorDiv.textContent = '❌ ' + (data.message || 'Fout bij aanmaken account');
                    errorDiv.style.display = 'block';
                    submitBtn.disabled = false;
                    submitBtn.textContent = '✅ Admin Account Aanmaken';
                }
            })
            .catch(error => {
                errorDiv.textContent = '❌ Verbindingsfout: ' + error;
                errorDiv.style.display = 'block';
                submitBtn.disabled = false;
                submitBtn.textContent = '✅ Admin Account Aanmaken';
            });
        }
    </script>
</body>
</html>
)rawliteral";

#endif // ADMIN_SETUP_PAGE_H
