#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define MAGIC_NUMBER 0xDEADBEEF

struct Config {
  uint32_t magic = 0;
  char ssid[32] = "";
  char password[64] = "";
  char switchName[32] = "Switch_Anonimo";
  uint8_t behavior = 0; // 0: ON/OFF, 1: Click
};

ESP8266WebServer server(80);
Config config;
const int relayPin = 0; // GPIO0 (collegare al relè)

// Dichiarazioni delle funzioni
void serveWifiConfig();
void serveSwitchConfig();
void handleScan();
void handleConnect();
void serveControlPage();
void handleSave();
void handleReset();
void handleOn();
void handleOff();
void handleClick();
void serveCSS();

void setup() {
  Serial.begin(115200);
  
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // Assicurati che il relè sia spento inizialmente
  
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, config);

  if (config.magic != MAGIC_NUMBER) {
    startAPMode();
  } else {
    connectToWiFi();
  }
}

void loop() {
  server.handleClient();
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP-Switch");
  server.on("/", serveWifiConfig); // Serve la pagina di configurazione WiFi
  server.on("/scan", handleScan); // Gestisce la scansione delle reti WiFi
  server.on("/connect", handleConnect); // Gestisce la connessione alla rete WiFi
  server.on("/style.css", serveCSS); // Serve il file CSS
  server.begin();
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    startWebServer();
  } else {
    startAPMode();
  }
}

void startWebServer() {
  server.on("/", serveControlPage); // Pagina principale
  server.on("/config", serveSwitchConfig); // Pagina di configurazione dello switch
  server.on("/save", handleSave); // Salva la configurazione
  server.on("/reset", handleReset); // Reset completo
  server.on("/on", handleOn); // Accendi il relè
  server.on("/off", handleOff); // Spegni il relè
  server.on("/click", handleClick); // Toggle del relè (singolo click)
  server.on("/style.css", serveCSS); // Serve il file CSS
  server.begin();
}

void serveWifiConfig() {
  String html = R"rawliteral(
    <html>
  <head>
    <link rel="stylesheet" type="text/css" href="/style.css">
  </head>
  <body>
    <h1>Configura WiFi</h1>
    <select id="networks"></select>
    <input type="password" id="password" placeholder="Password">
    <button onclick="connect()">Connetti</button>
      <script>
        fetch("/scan").then(r => r.json()).then(networks => {
          networks.forEach(n => {
            let option = document.createElement("option");
            option.value = n;
            option.text = n;
            document.getElementById("networks").appendChild(option);
          });
        });
        
        function connect() {
          let form = new FormData();
          form.append("ssid", document.getElementById("networks").value);
          form.append("password", document.getElementById("password").value);
          fetch("/connect", {method: "POST", body: form})
            .then(r => r.text()).then(t => alert(t));
        }
      </script>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleScan() {
  String json = "[";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    if (i) json += ",";
    json += "\"" + WiFi.SSID(i) + "\"";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleConnect() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid));
    strncpy(config.password, password.c_str(), sizeof(config.password));
    config.magic = MAGIC_NUMBER;
    EEPROM.put(0, config);
    EEPROM.commit();
    server.send(200, "text/plain", "Successo!\nCollegati alla rete " + WiFi.SSID()+" alla pagina: " + WiFi.localIP().toString());
    delay(1000);
    ESP.restart();
  } else {
    server.send(200, "text/plain", "Errore di connessione");
  }
}

void serveControlPage() {
  String html = R"rawliteral(
    <html>
    <head>
      <link rel="stylesheet" type="text/css" href="/style.css">
    </head>
    <body>
      <h1>)rawliteral" + String(config.switchName) + R"rawliteral(</h1>
  )rawliteral";
  
  if (config.behavior == 0) {
    html += R"rawliteral(
      <button onclick="fetch('/on')">ON</button>
      <button onclick="fetch('/off')">OFF</button>
    )rawliteral";
  } else {
    html += R"rawliteral(
      <button onclick="fetch('/click')">CLICK</button>
    )rawliteral";
  }
  
  html += R"rawliteral(
      <p><a href='/config'>Configurazione</a></p>
      <form action="/reset" method="post">
        <button type="submit" style="color:white;background-color:red;">Reset Completo</button>
      </form>
    </body>
    </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void serveSwitchConfig() {
  String html = R"rawliteral(
    <html>
    <head>
      <link rel="stylesheet" type="text/css" href="/style.css">
    </head>
    <body>
      <h1>Configurazione Switch</h1>
      <form action="/save" method="post">
        <label for="name">Nome Switch:</label><br>
        <input type="text" id="name" name="name" value=")rawliteral" + String(config.switchName) + R"rawliteral("><br><br>
        
        <label for="behavior">Comportamento:</label><br>
        <select id="behavior" name="behavior">
          <option value="0")rawliteral" + (config.behavior == 0 ? " selected" : "") + R"rawliteral(>Modalita' ON/OFF</option>
          <option value="1")rawliteral" + (config.behavior == 1 ? " selected" : "") + R"rawliteral(>Click Singolo</option>
        </select><br><br>
        
        <button type="submit">Salva Configurazione</button>
      </form>
    </body>
    </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("name")) {
    String newName = server.arg("name");
    newName.toCharArray(config.switchName, sizeof(config.switchName));
  }

  if (server.hasArg("behavior")) {
    config.behavior = server.arg("behavior").toInt();
  }

  EEPROM.put(0, config);
  EEPROM.commit();

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleReset() {
  config.magic = 0;
  memset(config.ssid, 0, sizeof(config.ssid));
  memset(config.password, 0, sizeof(config.password));
  strcpy(config.switchName, "Switch");
  config.behavior = 0;

  EEPROM.put(0, config);
  EEPROM.commit();
  server.send(200, "text/plain", "Reset completato con successo!");
  server.sendHeader("Location", "/");
  server.send(303);
  delay(1000);
  ESP.restart();
}

void handleOn() {
  digitalWrite(relayPin, LOW); // Accendi il relè (logica negativa)
  server.send(200, "text/plain", "Relè acceso");
}

void handleOff() {
  digitalWrite(relayPin, HIGH); // Spegni il relè (logica negativa)
  server.send(200, "text/plain", "Relè spento");
}

void handleClick() {
  // Leggi lo stato attuale del pin e invertilo
  bool currentState = digitalRead(relayPin);
  digitalWrite(relayPin, !currentState); // Inverte lo stato del relè
  server.send(200, "text/plain", currentState ? "Relè spento" : "Relè acceso");
}

//CSS
void serveCSS() {
  String css = R"rawliteral(
    /* style.css */
    body {
      background-color: #F2F2F2;
      font-family: Arial, sans-serif;
      color: #F28705;
      margin: 0;
      padding: 20px;
      text-align: center;
    }

    h1 {
      color: #F28705;
    }

    button {
      background-color: #F28705;
      color: white;
      border: none;
      border-radius: 8px;
      padding: 10px 20px;
      margin: 5px;
      font-size: 16px;
      cursor: pointer;
      box-shadow: 3px 3px 5px rgba(0, 0, 0, 0.2);
      transition: background-color 0.3s ease, transform 0.2s ease;
    }

    button:hover {
      background-color: #e67600;
      transform: translateY(-2px);
    }

    button:active {
      transform: translateY(0);
    }

    a {
      color: #F28705;
      text-decoration: none;
    }

    a:hover {
      text-decoration: underline;
    }

    form {
      margin-top: 20px;
    }

    input[type="text"], input[type="password"], select {
      padding: 8px;
      border: 1px solid #F28705;
      border-radius: 4px;
      margin-bottom: 10px;
      width: 200px;
    }

    input[type="submit"] {
      background-color: #F28705;
      color: white;
      border: none;
      border-radius: 8px;
      padding: 10px 20px;
      font-size: 16px;
      cursor: pointer;
      box-shadow: 3px 3px 5px rgba(0, 0, 0, 0.2);
      transition: background-color 0.3s ease, transform 0.2s ease;
    }

    input[type="submit"]:hover {
      background-color: #e67600;
      transform: translateY(-2px);
    }

    input[type="submit"]:active {
      transform: translateY(0);
    }
  )rawliteral";
  server.send(200, "text/css", css);
}
