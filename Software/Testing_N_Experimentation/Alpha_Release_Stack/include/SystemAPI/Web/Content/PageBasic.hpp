/*****************************************************************
 * @file PageBasic.hpp
 * @brief Basic tab page content
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_BASIC[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Basic</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <div class="container">
    <header>
      <div class="header-content">
        <div class="logo-section">
          <div class="logo-icon">&#x25C8;</div>
          <div class="logo-text">
            <h1>Lucidius</h1>
            <span class="model-tag" id="device-model">DX.3</span>
          </div>
        </div>
      </div>
    </header>
    
    <nav class="tabs">
      <a href="/" class="tab active">Basic</a>
      <a href="/advanced" class="tab">Advanced</a>
      <a href="/system" class="tab">System</a>
      <a href="/settings" class="tab">Settings</a>
    </nav>
    
    <section class="tab-content active">
      <div class="card-grid">
      <div class="card">
        <div class="card-header">
          <h2>Welcome</h2>
        </div>
        <div class="card-body">
          <p class="welcome-text">
            Connected to <strong id="welcome-ssid">Lucidius (DX.3)</strong>
          </p>
          <div class="info-grid">
            <div class="info-item">
              <span class="info-label">IP Address</span>
              <span class="info-value" id="info-ip">192.168.4.1</span>
            </div>
            <div class="info-item">
              <span class="info-label">Clients</span>
              <span class="info-value" id="info-clients">0</span>
            </div>
          </div>
        </div>
      </div>
      
      <div class="card placeholder-card">
        <div class="card-body center">
          <div class="placeholder-icon">&#127899;</div>
          <p class="placeholder-text">Basic controls coming soon</p>
        </div>
      </div>
      </div>
    </section>
    
    <footer>
      <p>Lucidius - ARCOS Framework</p>
    </footer>
  </div>
  
  <div id="toast" class="toast"></div>
  
  <script>
  var pollDelay = 200;
  function fetchState() {
    fetch('/api/state')
      .then(r => r.json())
      .then(data => {
        if (data.ssid) document.getElementById('welcome-ssid').textContent = data.ssid;
        if (data.ip) document.getElementById('info-ip').textContent = data.ip;
        document.getElementById('info-clients').textContent = data.clients || 0;
        setTimeout(fetchState, pollDelay);
      })
      .catch(err => {
        console.error('Fetch error:', err);
        setTimeout(fetchState, pollDelay * 2);
      });
  }
  fetchState();
  </script>
</body>
</html>
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
