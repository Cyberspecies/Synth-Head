/*****************************************************************
 * File:      WebPages_Setup.hpp
 * Category:  Manager/WiFi
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    WiFi setup page HTML generation for captive portal.
 *    Allows users to configure custom SSID/password or keep random ones.
 *****************************************************************/

#ifndef WEB_PAGES_SETUP_HPP
#define WEB_PAGES_SETUP_HPP

namespace arcos::manager {

inline String CaptivePortalManager::generateSetupPage() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SynthHead - WiFi Setup</title>
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
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 500px;
      width: 100%;
      padding: 40px;
    }
    h1 {
      color: #667eea;
      font-size: 28px;
      margin-bottom: 10px;
      text-align: center;
    }
    .subtitle {
      color: #666;
      text-align: center;
      margin-bottom: 30px;
      font-size: 14px;
    }
    .current-creds {
      background: #f0f0f0;
      padding: 15px;
      border-radius: 10px;
      margin-bottom: 30px;
    }
    .current-creds h3 {
      color: #333;
      font-size: 16px;
      margin-bottom: 10px;
    }
    .cred-item {
      margin: 8px 0;
      font-family: monospace;
      font-size: 14px;
    }
    .cred-label {
      color: #666;
      display: inline-block;
      width: 90px;
    }
    .cred-value {
      color: #000;
      font-weight: bold;
    }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      color: #333;
      font-weight: 600;
      margin-bottom: 8px;
      font-size: 14px;
    }
    input[type="text"], input[type="password"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      font-size: 16px;
      transition: border 0.3s;
    }
    input:focus {
      outline: none;
      border-color: #667eea;
    }
    .hint {
      color: #999;
      font-size: 12px;
      margin-top: 5px;
    }
    .button-group {
      display: flex;
      gap: 10px;
      margin-top: 30px;
    }
    button {
      flex: 1;
      padding: 14px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102,126,234,0.4);
    }
    .btn-secondary {
      background: #f0f0f0;
      color: #333;
    }
    .btn-secondary:hover {
      background: #e0e0e0;
    }
    .warning {
      background: #fff3cd;
      border-left: 4px solid #ffc107;
      padding: 12px;
      border-radius: 5px;
      margin-top: 20px;
      font-size: 13px;
      color: #856404;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎭 SynthHead WiFi Setup</h1>
    <p class="subtitle">Configure your device's WiFi credentials</p>
    
    <div class="current-creds">
      <h3>Current Credentials (Generated)</h3>
      <div class="cred-item">
        <span class="cred-label">SSID:</span>
        <span class="cred-value">)";
  
  html += current_ssid_;
  html += R"(</span>
      </div>
      <div class="cred-item">
        <span class="cred-label">Password:</span>
        <span class="cred-value">)";
  html += current_password_;
  html += R"(</span>
      </div>
    </div>
    
    <form method="POST" action="/setup">
      <div class="form-group">
        <label>Custom SSID (optional)</label>
        <input type="text" name="ssid" placeholder="Leave empty to keep current">
        <div class="hint">1-32 characters</div>
      </div>
      
      <div class="form-group">
        <label>Custom Password (optional)</label>
        <input type="password" name="password" placeholder="Leave empty to keep current">
        <div class="hint">Minimum 8 characters</div>
      </div>
      
      <div class="button-group">
        <button type="submit" name="action" value="custom" class="btn-primary">
          Set Custom
        </button>
        <button type="submit" name="action" value="keep" class="btn-secondary">
          Keep Current
        </button>
      </div>
    </form>
    
    <div class="warning">
      ⚠️ After changing credentials, you will need to reconnect to the new network.
    </div>
  </div>
</body>
</html>
)";
  
  return html;
}

inline void CaptivePortalManager::handleSetupSubmit(AsyncWebServerRequest* request) {
  String action = "";
  String new_ssid = "";
  String new_password = "";
  
  // Get form parameters
  if (request->hasParam("action", true)) {
    action = request->getParam("action", true)->value();
  }
  if (request->hasParam("ssid", true)) {
    new_ssid = request->getParam("ssid", true)->value();
  }
  if (request->hasParam("password", true)) {
    new_password = request->getParam("password", true)->value();
  }
  
  if (action == "custom" && new_ssid.length() > 0 && new_password.length() >= 8) {
    // User wants custom credentials
    current_ssid_ = new_ssid;
    current_password_ = new_password;
    use_custom_credentials_ = true;
    saveCredentials();
    
    Serial.println("WIFI: Custom credentials set");
    Serial.printf("WIFI: New SSID: %s\n", current_ssid_.c_str());
    
    // Send response and restart
    String response = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Restarting...</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #667eea;
      color: white;
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      text-align: center;
    }
    h1 { font-size: 24px; margin-bottom: 10px; }
    p { font-size: 16px; }
  </style>
</head>
<body>
  <div>
    <h1>✅ Configuration Saved!</h1>
    <p>Device is restarting with new credentials...</p>
    <p>Reconnect to: <strong>)";
    response += current_ssid_;
    response += R"(</strong></p>
  </div>
</body>
</html>
)";
    
    request->send(200, "text/html", response);
    
    // Restart device after 3 seconds
    delay(3000);
    ESP.restart();
    
  } else if (action == "keep" || action == "custom") {
    // Keep current credentials or invalid custom input
    use_custom_credentials_ = true;
    saveCredentials();
    
    // Redirect to dashboard
    request->redirect("/");
  } else {
    // Invalid request
    request->send(400, "text/plain", "Invalid request");
  }
}

} // namespace arcos::manager

#endif // WEB_PAGES_SETUP_HPP
