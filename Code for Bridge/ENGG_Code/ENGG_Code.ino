#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ===== Wi-Fi Credentials =====
const char* ssid = "AlanV";
const char* password = "jesalaalv76";

// ===== Pin Definitions =====
#define SERVO1_PIN 19
#define SERVO2_PIN 18
#define MOTOR_PIN1 27
#define MOTOR_PIN2 13
#define MOTOR_ENABLE 14
#define IR_SENSOR1_PIN 25
#define IR_SENSOR2_PIN 26
#define RED_LED 4 // do not change
#define GREEN_LED 16

// ===== Objects =====
Servo servo1;
Servo servo2;
WebServer server(80);

// ===== Bridge States =====
enum BridgeState { LOWERED, OPEN };
BridgeState currentState = LOWERED;

// ===== Variables =====
bool boatDetected = false;
unsigned long lastActionTime = 0;

// ===== Constants =====
const unsigned long MOVE_TIME = 4000;
const unsigned long BOAT_WAIT_TIME = 3000;

// ===== Function Prototypes =====
void raiseBridge();
void lowerBridge();
void stopMotor();
void stopServos();
void setTrafficLight(int activeLED);
void handleRoot();
void handleRaise();
void handleLower();
void handleStatus();
void updateWebPage();

// ===== Setup =====
void setup() {
  Serial.begin(115200);

  pinMode(IR_SENSOR1_PIN, INPUT);
  pinMode(IR_SENSOR2_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);
  pinMode(MOTOR_ENABLE, OUTPUT);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);

  // Start with bridge lowered (green, gates open)
  lowerBridge();

  // Wi-Fi Setup
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Web routes
  server.on("/", handleRoot);
  server.on("/raise", handleRaise);
  server.on("/lower", handleLower);
  server.on("/status", handleStatus);
  server.begin();

  Serial.println("Web server started");

  servo1.write(100);
  servo2.write(100);
}

// ===== Loop =====
void loop() {
  server.handleClient();

  // Check sensors
  boatDetected = (digitalRead(IR_SENSOR1_PIN) == HIGH || digitalRead(IR_SENSOR2_PIN) == HIGH);

  // Auto operation
  if (boatDetected && currentState == LOWERED) {
    raiseBridge();
  } else if (!boatDetected && currentState == OPEN && millis() - lastActionTime > BOAT_WAIT_TIME) {
    lowerBridge();
  }
}

// ===== Actions =====
void raiseBridge() {
  Serial.println("Raising bridge...");
  currentState = OPEN;
  lastActionTime = millis();

  // Close boom gates
  servo1.write(100);
  servo2.write(100);
  delay(1000);
  stopServos();

  // Turn light red
  setTrafficLight(RED_LED);

  // Run motor to raise
  digitalWrite(MOTOR_PIN1, HIGH);
  digitalWrite(MOTOR_PIN2, LOW);
  analogWrite(MOTOR_ENABLE, 255);
  delay(MOVE_TIME);
  stopMotor();

  Serial.println("Bridge raised.");
  updateWebPage();
}

void lowerBridge() {
  Serial.println("Lowering bridge...");
  currentState = LOWERED;
  lastActionTime = millis();

  // Run motor to lower
  digitalWrite(MOTOR_PIN1, LOW);
  digitalWrite(MOTOR_PIN2, HIGH);
  analogWrite(MOTOR_ENABLE, 255);
  delay(MOVE_TIME);
  stopMotor();

  // Open boom gates
  servo1.write(0);
  servo2.write(0);
  delay(1000);
  stopServos();

  // Green light
  setTrafficLight(GREEN_LED);
  Serial.println("Bridge lowered.");
  updateWebPage();
}

void stopMotor() {
  analogWrite(MOTOR_ENABLE, 0);
  digitalWrite(MOTOR_PIN1, LOW);
  digitalWrite(MOTOR_PIN2, LOW);
}

void stopServos() {
  servo1.write(90);
  servo2.write(90);
}

void setTrafficLight(int activeLED) {
  digitalWrite(RED_LED, activeLED == RED_LED ? HIGH : LOW);
  digitalWrite(GREEN_LED, activeLED == GREEN_LED ? HIGH : LOW);
}

// ===== Web UI =====
void handleRoot() { updateWebPage(); }

void handleRaise() {
  raiseBridge();
  server.send(200, "text/plain", "Bridge Raised");
}

void handleLower() {
  lowerBridge();
  server.send(200, "text/plain", "Bridge Lowered");
}

void handleStatus() {
  String state = (currentState == LOWERED) ? "LOWERED" : "OPEN";
  String light = digitalRead(GREEN_LED) ? "GREEN" : "RED";

  String json = "{";
  json += "\"state\":\"" + state + "\",";
  json += "\"boatDetected\":" + String(boatDetected ? "true" : "false") + ",";
  json += "\"light\":\"" + light + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void updateWebPage() {
  String html = R"rawliteral(
  <!DOCTYPE html><html><head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Smart Bridge</title>
  <style>
    body { font-family: Arial; text-align: center; background: #f0f0f0; }
    button {
      width: 160px; height: 50px; margin: 10px; font-size: 18px;
      border: none; border-radius: 10px; cursor: pointer;
    }
    .raise { background: #d9534f; color: white; }
    .lower { background: #5cb85c; color: white; }
    .status { font-size: 18px; margin-top: 15px; font-weight: bold; }
    .info { margin-top: 10px; font-size: 16px; }
  </style></head><body>
  <h2>🌉 Smart Bridge Control</h2>
  <button class='raise' onclick="location.href='/raise'">Raise Bridge</button>
  <button class='lower' onclick="location.href='/lower'">Lower Bridge</button>
  <div class='status'>Bridge Status: )rawliteral";

  // Bridge state
  html += (currentState == LOWERED) ? "<span style='color:green'>LOWERED</span>" :
                                      "<span style='color:red'>OPEN</span>";
  html += R"rawliteral(</div>)rawliteral";

  // Boom gate state
  String gateState = (servo1.read() == 0 && servo2.read() == 0) ? "OPEN" : "CLOSED";
  html += "<div class='info'>Boom Gates: " + gateState + "</div>";

  // Traffic light
  String lightState = digitalRead(GREEN_LED) ? "GREEN" : "RED";
  html += "<div class='info'>Traffic Light: " + lightState + "</div>";

  // Sensor
  String sensorState = boatDetected ? "Boat detected" : "No boat detected";
  html += "<div class='info'>Sensor: " + sensorState + "</div>";

  html += R"rawliteral(</body></html>)rawliteral";

  server.send(200, "text/html", html);
}


