// Float sensor with deadband to prevent false switching
#define FLOAT_PIN 13

// Deadband settings
#define MIN_STATE_TIME 5000      // Must stay in state for 5 seconds before changing
#define DEBOUNCE_DELAY 100       // Basic debounce delay

// State variables
int currentReading = HIGH;
int confirmedState = HIGH;
int lastConfirmedState = HIGH;
unsigned long stateStartTime = 0;
unsigned long lastChangeTime = 0;
bool stateConfirmed = false;

void setup() {
  Serial.begin(115200);
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  
  Serial.println("Float sensor with deadband on GPIO 14");
  Serial.println("=====================================");
  Serial.println("Deadband: 5 seconds - prevents false switching");
  
  // Initialize state
  currentReading = digitalRead(FLOAT_PIN);
  confirmedState = currentReading;
  lastConfirmedState = currentReading;
  stateStartTime = millis();
  stateConfirmed = true;
  
  Serial.print("Initial water level: ");
  Serial.println(confirmedState == LOW ? "HIGH" : "LOW");
  Serial.println("=====================================");
}

void loop() {
  // Read current sensor state
  int newReading = digitalRead(FLOAT_PIN);
  
  // Basic debouncing
  if (newReading != currentReading) {
    delay(DEBOUNCE_DELAY);
    newReading = digitalRead(FLOAT_PIN);
    
    if (newReading != currentReading) {
      // Reading actually changed
      currentReading = newReading;
      stateStartTime = millis();
      stateConfirmed = false;
      
      Serial.print("State change detected, waiting for confirmation... ");
      Serial.println(currentReading == LOW ? "HIGH" : "LOW");
    }
  }
  
  // Check if we've been in new state long enough (deadband)
  if (!stateConfirmed && (millis() - stateStartTime) >= MIN_STATE_TIME) {
    // State has been stable for deadband time - confirm it
    confirmedState = currentReading;
    stateConfirmed = true;
    
    // Only announce if it's actually different from last confirmed state
    if (confirmedState != lastConfirmedState) {
      if (confirmedState == LOW) {
        Serial.println("*** CONFIRMED: WATER LEVEL HIGH ***");
        Serial.println("Float has been UP for 5+ seconds");
        // Add your HIGH water actions here:
        // - Stop filling pump
        // - Turn on overflow alarm
        // - Send HIGH level SMS
        
      } else {
        Serial.println("*** CONFIRMED: WATER LEVEL LOW ***"); 
        Serial.println("Float has been DOWN for 5+ seconds");
        // Add your LOW water actions here:
        // - Start filling pump
        // - Turn on low level alarm  
        // - Send LOW level SMS
      }
      
      lastConfirmedState = confirmedState;
      lastChangeTime = millis();
    }
  }
  
  // Show waiting status for unconfirmed changes
  if (!stateConfirmed) {
    static unsigned long lastDotTime = 0;
    if (millis() - lastDotTime > 1000) {
      unsigned long remaining = MIN_STATE_TIME - (millis() - stateStartTime);
      Serial.print("Confirming in ");
      Serial.print(remaining / 1000);
      Serial.println(" seconds...");
      lastDotTime = millis();
    }
  }
  
  // Optional: Status report every 60 seconds
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > 60000) {
    Serial.println("--- STATUS REPORT ---");
    Serial.print("Confirmed water level: ");
    Serial.println(confirmedState == LOW ? "HIGH" : "LOW");
    Serial.print("Current sensor reading: ");
    Serial.println(currentReading == LOW ? "HIGH" : "LOW");
    Serial.print("Time since last change: ");
    Serial.print((millis() - lastChangeTime) / 1000);
    Serial.println(" seconds");
    Serial.println("--------------------");
    lastStatusTime = millis();
  }
  
  delay(100);
}