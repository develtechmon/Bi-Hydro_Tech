void updateFloatSensorNonBlocking(unsigned long currentTime) {
  int newReading = digitalRead(FLOAT_PIN);

  // Check for reading change
  if (newReading != lastReading) {
    lastDebounceTime = currentTime;
    lastReading = newReading;
  }

  // Non-blocking debounce check
  if ((currentTime - lastDebounceTime) >= DEBOUNCE_DELAY) {
    if (newReading != currentReading) {
      // Reading has changed and is stable
      currentReading = newReading;
      stateStartTime = currentTime;
      stateConfirmed = false;

      Serial.print("Float state change detected: ");
      Serial.println(currentReading == LOW ? "UP (High Water)" : "DOWN (Low Water)");
    }
  }

  // Non-blocking deadband check
  if (!stateConfirmed && (currentTime - stateStartTime) >= MIN_STATE_TIME) {
    confirmedState = currentReading;
    stateConfirmed = true;

    if (confirmedState != lastConfirmedState) {
      if (confirmedState == LOW) {
        Serial.println("*** FLOAT: WATER HIGH - ALARM TRIGGERED! ***");
      } else {
        Serial.println("*** FLOAT: WATER LOW - ALARM CLEARED ***");
      }
      lastConfirmedState = confirmedState;
    }
  }
}