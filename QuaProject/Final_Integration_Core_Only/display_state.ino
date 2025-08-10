void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WATER MONITOR");
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // Ultrasonic Distance with reliability indicator
  display.setCursor(0, 12);
  display.print("Distance: ");
  if (ultrasonicValid) {
    display.print(reliableDistance);  // Show reliable distance
    display.print("mm");
    // Show if using backup reading
    if (usingLastValidReading) {
      display.print("*");  // Asterisk indicates backup reading
    }
  } 
  // else {
  //   display.print("ERROR");
  // }
  display.println();

  // Water Level Status with visual indicator
  display.setCursor(0, 22);
  display.print("Level: ");

  switch (currentWaterStatus) {
    case NORMAL:
      display.print("NORMAL");
      display.drawCircle(120, 25, 3, SSD1306_WHITE);
      break;
    case WATCH:
      display.print("WATCH");
      display.fillCircle(120, 25, 3, SSD1306_WHITE);
      break;
    case WARNING:
      display.print("WARNING");
      display.fillRect(115, 22, 6, 6, SSD1306_WHITE);
      break;
    case CRITICAL:
      display.print("CRITICAL");
      display.fillTriangle(117, 19, 123, 19, 120, 28, SSD1306_WHITE);
      break;
  }

  // Float Status with alarm indicator
  display.setCursor(0, 32);
  display.print("Float: ");
  if (stateConfirmed) {
    if (confirmedState == LOW) {
      display.print("HIGH ");
      // Show blinking alarm indicator
      if (floatAlarmActive) {
        display.print("ALARM!");
      } else {
        display.print("alarm");
      }
    } else {
      display.print("LOW");
    }
  } else {
    unsigned long remaining = MIN_STATE_TIME - (millis() - stateStartTime);
    display.print("WAIT ");
    display.print(remaining / 1000);
    display.print("s");
  }

  // Rainfall Data
  display.setCursor(0, 42);
  display.print("Rain 1H: ");
  display.print(rainfallSensor.getRainfall(1), 1);
  display.println("mm");

  display.setCursor(0, 52);
  display.print("Total: ");
  display.print(rainfallSensor.getRainfall(), 1);
  display.println("mm");

  display.display();
}

void displayStartup() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 15);
  display.println("BI-HYDRO");
  display.setCursor(10, 35);
  display.println("  TECH");
  display.display();
  delay(2000); // Only blocking delay
}

void displayError(String message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ERROR:");
  display.setCursor(0, 20);
  display.println(message);
  display.display();
}

void printToSerial() {
  Serial.println("=== NON-BLOCKING STATUS ===");

  Serial.print("Ultrasonic: ");
  if (ultrasonicValid) {
    Serial.print("Raw=");
    Serial.print(ultrasonicDistance);
    Serial.print("mm, Reliable=");
    Serial.print(reliableDistance);
    Serial.print("mm");
    if (usingLastValidReading) {
      Serial.print(" (using backup)");
    }
    Serial.println();
  } else {
    Serial.println("INVALID");
  }

  Serial.print("Water Status: ");
  switch (currentWaterStatus) {
    case NORMAL:   Serial.println("NORMAL"); break;
    case WATCH:    Serial.println("WATCH"); break;
    case WARNING:  Serial.println("WARNING"); break;
    case CRITICAL: Serial.println("CRITICAL"); break;
  }

  Serial.print("Float: ");
  if (stateConfirmed) {
    if (confirmedState == LOW) {
      Serial.println("HIGH - ALARM ACTIVE!");
    } else {
      Serial.println("LOW");
    }
  } else {
    Serial.println("CONFIRMING");
  }

  Serial.print("Loop performance: Smooth (no blocking delays)");
  Serial.println();
  Serial.println("===========================");
}