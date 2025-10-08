// Define LED pin
const int redLED = 2; // Change this to your LED pin number

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Set LED pin as output
  pinMode(redLED, OUTPUT);
  
  Serial.println("Turning LED ON (Red)");
  
  // Turn LED ON
  digitalWrite(redLED, HIGH);
}

void loop() {
  // LED stays on
  // Nothing needed in loop
}