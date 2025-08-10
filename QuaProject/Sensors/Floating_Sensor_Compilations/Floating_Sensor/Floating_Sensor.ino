// Simple but robust float sensor code
#define FLOAT_PIN 14

int lastState = HIGH;
unsigned long lastChangeTime = 0;
unsigned long debounceDelay = 100;  // Prevent false readings

void setup() {
  Serial.begin(115200);
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  
  Serial.println("Float sensor ready on GPIO 14");
  Serial.println("===============================");
  
  // Show initial state
  int initialState = digitalRead(FLOAT_PIN);
  Serial.print("Initial water level: ");
  Serial.println(initialState == LOW ? "HIGH" : "LOW");
  
  lastState = initialState;
  lastChangeTime = millis();
}

void loop() {
  int currentState = digitalRead(FLOAT_PIN);
  
  // Only check for changes after debounce delay
  if (currentState != lastState && (millis() - lastChangeTime) > debounceDelay) {
    
    // Double-check the reading
    delay(50);
    currentState = digitalRead(FLOAT_PIN);
    
    // If still different, it's a real change
    if (currentState != lastState) {
      if (currentState == LOW) {
        Serial.println("*** WATER LEVEL: HIGH ***");
        // Add your actions here:
        // - Turn on LED
        // - Send SMS alert
        // - Stop pump
      } else {
        Serial.println("*** WATER LEVEL: LOW ***");
        // Add your actions here:
        // - Turn off LED  
        // - Send SMS alert
        // - Start pump
      }
      
      lastState = currentState;
      lastChangeTime = millis();
    }
  }
  
  // Optional: Show status every 30 seconds
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > 30000) {
    Serial.print("Status check - Water level: ");
    Serial.println(lastState == LOW ? "HIGH" : "LOW");
    lastStatusTime = millis();
  }
  
  delay(100);  // Check every 100ms
}