void updateUltrasonicSensorNonBlocking(unsigned long currentTime) {
  // Send trigger command at regular intervals
  if (!waitingForResponse && (currentTime - lastUltrasonicTrigger) >= ULTRASONIC_TRIGGER_INTERVAL) {
    ultrasonicSerial.write(COM);
    ultrasonicSentTime = currentTime;
    waitingForResponse = true;
    lastUltrasonicTrigger = currentTime;
  }

  // Check for response (non-blocking)
  if (waitingForResponse) {
    if (ultrasonicSerial.available() >= 4) {
      // Try to read complete frame
      if (ultrasonicSerial.read() == 0xFF) {
        buffer_RTT[0] = 0xFF;

        // Read remaining bytes if available
        bool frameComplete = true;
        for (int i = 1; i < 4; i++) {
          if (ultrasonicSerial.available()) {
            buffer_RTT[i] = ultrasonicSerial.read();
          } else {
            frameComplete = false;
            break;
          }
        }

        if (frameComplete) {
          // Validate checksum
          CS = buffer_RTT[0] + buffer_RTT[1] + buffer_RTT[2];
          if (buffer_RTT[3] == CS) {
            int rawDistance = (buffer_RTT[1] << 8) + buffer_RTT[2];
            
            // Validate and filter the reading
            validateUltrasonicReading(rawDistance, currentTime);
          } else {
            ultrasonicValid = false;
          }
          waitingForResponse = false;
        }
      }
    }

    // Timeout check
    if ((currentTime - ultrasonicSentTime) >= ULTRASONIC_TIMEOUT) {
      ultrasonicValid = false;
      waitingForResponse = false;
    }
  }

  // Check if we should use last valid reading due to timeout
  if ((currentTime - lastValidReadingTime) > INVALID_READING_TIMEOUT) {
    // Been too long since valid reading - mark as invalid
    ultrasonicValid = false;
    usingLastValidReading = false;
    Serial.println("WARNING: Ultrasonic sensor timeout - no valid readings");
  }

  // Clear any remaining data (non-blocking)
  while (ultrasonicSerial.available()) {
    ultrasonicSerial.read();
  }
}

void validateUltrasonicReading(int rawReading, unsigned long currentTime) {
  bool isValidReading = true;
  
  // Check 1: Is reading within valid range?
  if (rawReading < MIN_VALID_DISTANCE || rawReading > MAX_VALID_DISTANCE) {
    Serial.print("Invalid range: ");
    Serial.print(rawReading);
    Serial.println("mm - using last valid reading");
    isValidReading = false;
  }
  
  // Check 2: Is change too sudden? (only if we have a previous valid reading)
  if (isValidReading && lastValidDistance > 0) {
    int change = abs(rawReading - lastValidDistance);
    if (change > MAX_CHANGE_THRESHOLD) {
      Serial.print("Sudden change detected: ");
      Serial.print(change);
      Serial.println("mm - using last valid reading");
      isValidReading = false;
    }
  }
  
  // Check 3: Special case for 0mm reading (often indicates sensor error)
  if (rawReading == 0) {
    Serial.println("Zero reading detected - using last valid reading");
    isValidReading = false;
  }
  
  if (isValidReading) {
    // Use the new valid reading
    ultrasonicDistance = rawReading;
    lastValidDistance = rawReading;
    reliableDistance = rawReading;
    lastValidReadingTime = currentTime;
    ultrasonicValid = true;
    usingLastValidReading = false;
  } else {
    // Use last valid reading if available and not too old
    if (lastValidDistance > 0 && (currentTime - lastValidReadingTime) <= INVALID_READING_TIMEOUT) {
      ultrasonicDistance = rawReading;  // Store raw reading for debugging
      reliableDistance = lastValidDistance;  // Use last valid for calculations
      ultrasonicValid = true;  // Mark as valid since we have backup
      usingLastValidReading = true;
    } else {
      // No valid backup available
      ultrasonicDistance = rawReading;
      reliableDistance = 0;
      ultrasonicValid = false;
      usingLastValidReading = false;
    }
  }
}