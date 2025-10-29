#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// WiFi Credentials - CHANGE THESE
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// Pin Definitions - ADJUST TO YOUR WIRING
#define SERVO1_PIN 13        // Boom gate 1
#define SERVO2_PIN 12        // Boom gate 2
#define MOTOR_PIN1 27        // Motor driver IN1
#define MOTOR_PIN2 26        // Motor driver IN2
#define MOTOR_ENABLE 25      // Motor driver EN
#define IR_SENSOR_PIN 14     // IR sensor input
#define RED_LED 32           // Red traffic light
#define GREEN_LED 33         // Green traffic light
#define BOAT_LED 15          // Boat indicator LED

// Servo objects
Servo servo1;
Servo servo2;

// System state variables
enum BridgeState {
  LOWERED,
  RAISING,
  RAISED,
  LOWERING
};

BridgeState currentState = LOWERED;
bool manualOverride = false;
bool boatDetected = false;
unsigned long stateChangeTime = 0;
unsigned long ledBlinkTime = 0;
bool boatLedState = false;

// Timing constants (milliseconds)
const unsigned long RAISE_TIME = 5000;
const unsigned long LOWER_TIME = 5000;
const unsigned long BOAT_WAIT_TIME = 3000;
const unsigned long BLINK_INTERVAL = 500;

WebServer server(80);

// HTML Web Interface
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Bridge Control System</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 800px;
      margin: 0 auto;
      padding: 20px;
      background: #f0f0f0;
    }
    .container {
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      text-align: center;
    }
    .status-box {
      background: #e8f4f8;
      padding: 15px;
      margin: 15px 0;
      border-radius: 5px;
      border-left: 4px solid #2196F3;
    }
    .status-label {
      font-weight: bold;
      color: #555;
    }
    .status-value {
      color: #2196F3;
      font-size: 1.2em;
    }
    .control-section {
      margin: 20px 0;
    }
    button {
      background: #4CAF50;
      color: white;
      border: none;
      padding: 12px 24px;
      font-size: 16px;
      border-radius: 5px;
      cursor: pointer;
      margin: 5px;
      width: calc(50% - 12px);
    }
    button:hover {
      background: #45a049;
    }
    button.danger {
      background: #f44336;
    }
    button.danger:hover {
      background: #da190b;
    }
    button.warning {
      background: #ff9800;
    }
    button.warning:hover {
      background: #e68900;
    }
    .indicator {
      display: inline-block;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      margin-right: 10px;
    }
    .green { background: #4CAF50; }
    .red { background: #f44336; }
    .yellow { background: #ff9800; }
    .gray { background: #999; }
  </style>
  <script>
    function sendCommand(cmd) {
      fetch('/' + cmd).then(() => updateStatus());
    }
    
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('bridgeState').innerHTML = 
            '<span class="indicator ' + getStateColor(data.state) + '"></span>' + data.state;
          document.getElementById('boatDetected').innerHTML = 
            '<span class="indicator ' + (data.boatDetected ? 'red' : 'green') + '"></span>' + 
            (data.boatDetected ? 'YES' : 'NO');
          document.getElementById('manualOverride').innerHTML = 
            '<span class="indicator ' + (data.manualOverride ? 'yellow' : 'gray') + '"></span>' + 
            (data.manualOverride ? 'ACTIVE' : 'DISABLED');
          document.getElementById('trafficLight').innerHTML = 
            '<span class="indicator ' + (data.trafficRed ? 'red' : 'green') + '"></span>' + 
            (data.trafficRed ? 'RED - STOP' : 'GREEN - GO');
        });
    }
    
    function getStateColor(state) {
      if (state === 'LOWERED') return 'green';
      if (state === 'RAISED') return 'red';
      return 'yellow';
    }
    
    setInterval(updateStatus, 1000);
    window.onload = updateStatus;
  </script>
</head>
<body>
  <div class='container'>
    <h1>🌉 Bridge Control System</h1>
    
    <div class='status-box'>
      <div class='status-label'>Bridge State:</div>
      <div class='status-value' id='bridgeState'>Loading...</div>
    </div>
    
    <div class='status-box'>
      <div class='status-label'>Boat Detected:</div>
      <div class='status-value' id='boatDetected'>Loading...</div>
    </div>
    
    <div class='status-box'>
      <div class='status-label'>Traffic Light:</div>
      <div class='status-value' id='trafficLight'>Loading...</div>
    </div>
    
    <div class='status-box'>
      <div class='status-label'>Manual Override:</div>
      <div class='status-value' id='manualOverride'>Loading...</div>
    </div>
    
    <div class='control-section'>
      <h2>Manual Controls</h2>
      <button onclick='sendCommand("raise")'>⬆️ RAISE BRIDGE</button>
      <button onclick='sendCommand("lower")'>⬇️ LOWER BRIDGE</button>
      <button class='warning' onclick='sendCommand("override")'>🔧 TOGGLE OVERRIDE</button>
      <button class='danger' onclick='sendCommand("emergency")'>🛑 EMERGENCY STOP</button>
    </div>
  </div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Setup pins
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BOAT_LED, OUTPUT);
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);
  pinMode(MOTOR_ENABLE, OUTPUT);
  
  // Attach servos
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  
  // Initialize positions
  servo1.write(0);   // Gates up (open for cars)
  servo2.write(0);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BOAT_LED, LOW);
  stopMotor();
  
  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Open this IP in your browser to control the bridge");
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/raise", handleRaise);
  server.on("/lower", handleLower);
  server.on("/override", handleOverride);
  server.on("/emergency", handleEmergency);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  
  // Read IR sensor
  boatDetected = digitalRead(IR_SENSOR_PIN) == HIGH;
  
  // Automatic operation (if not in manual override)
  if (!manualOverride) {
    if (boatDetected && currentState == LOWERED) {
      raiseBridge();
    } else if (!boatDetected && currentState == RAISED && 
               (millis() - stateChangeTime > BOAT_WAIT_TIME)) {
      lowerBridge();
    }
  }
  
  // Handle bridge raising/lowering timings
  if (currentState == RAISING && (millis() - stateChangeTime > RAISE_TIME)) {
    stopMotor();
    currentState = RAISED;
    Serial.println("Bridge fully raised");
  }
  
  if (currentState == LOWERING && (millis() - stateChangeTime > LOWER_TIME)) {
    stopMotor();
    currentState = LOWERED;
    servo1.write(0);  // Open boom gates
    servo2.write(0);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    Serial.println("Bridge fully lowered");
  }
  
  // Blink boat LED when bridge is raised
  if (currentState == RAISED || currentState == RAISING) {
    if (millis() - ledBlinkTime > BLINK_INTERVAL) {
      boatLedState = !boatLedState;
      digitalWrite(BOAT_LED, boatLedState);
      ledBlinkTime = millis();
    }
  } else {
    digitalWrite(BOAT_LED, LOW);
  }
}

void raiseBridge() {
  if (currentState == LOWERED) {
    Serial.println("Raising bridge...");
    currentState = RAISING;
    stateChangeTime = millis();
    
    // Close boom gates
    servo1.write(90);
    servo2.write(90);
    
    // Traffic lights to red
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    
    delay(1000);  // Wait for gates to close
    
    // Start raising bridge
    digitalWrite(MOTOR_PIN1, HIGH);
    digitalWrite(MOTOR_PIN2, LOW);
    analogWrite(MOTOR_ENABLE, 255);
  }
}

void lowerBridge() {
  if (currentState == RAISED) {
    Serial.println("Lowering bridge...");
    currentState = LOWERING;
    stateChangeTime = millis();
    
    // Start lowering bridge
    digitalWrite(MOTOR_PIN1, LOW);
    digitalWrite(MOTOR_PIN2, HIGH);
    analogWrite(MOTOR_ENABLE, 255);
  }
}

void stopMotor() {
  digitalWrite(MOTOR_PIN1, LOW);
  digitalWrite(MOTOR_PIN2, LOW);
  analogWrite(MOTOR_ENABLE, 0);
}

// Web server handlers
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleStatus() {
  String state;
  switch(currentState) {
    case LOWERED: state = "LOWERED"; break;
    case RAISING: state = "RAISING"; break;
    case RAISED: state = "RAISED"; break;
    case LOWERING: state = "LOWERING"; break;
  }
  
  String json = "{";
  json += "\"state\":\"" + state + "\",";
  json += "\"boatDetected\":" + String(boatDetected ? "true" : "false") + ",";
  json += "\"manualOverride\":" + String(manualOverride ? "true" : "false") + ",";
  json += "\"trafficRed\":" + String(digitalRead(RED_LED) ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleRaise() {
  Serial.println("Manual raise command received");
  raiseBridge();
  server.send(200, "text/plain", "OK");
}

void handleLower() {
  Serial.println("Manual lower command received");
  lowerBridge();
  server.send(200, "text/plain", "OK");
}

void handleOverride() {
  manualOverride = !manualOverride;
  Serial.print("Manual override: ");
  Serial.println(manualOverride ? "ENABLED" : "DISABLED");
  server.send(200, "text/plain", "OK");
}

void handleEmergency() {
  Serial.println("EMERGENCY STOP!");
  stopMotor();
  currentState = (currentState == RAISED || currentState == RAISING) ? RAISED : LOWERED;
  server.send(200, "text/plain", "OK");
}