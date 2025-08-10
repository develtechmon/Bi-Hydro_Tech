void handleBuzzerAlertsNonBlocking(unsigned long currentTime) {
  // PRIORITY 1: Float sensor high water alarm (overrides all other alarms)
  if (stateConfirmed && confirmedState == LOW) {
    // Float is HIGH = Water level HIGH = ALARM!
    if (currentTime - lastFloatAlarm >= FLOAT_ALARM_INTERVAL) {
      floatAlarmActive = !floatAlarmActive;
      digitalWrite(BUZZER_PIN, floatAlarmActive);
      lastFloatAlarm = currentTime;
    }
    return; // Exit early - float alarm takes priority over ultrasonic alarms
  }
  
  // PRIORITY 2: Ultrasonic sensor alarms (only when float is not high)
  switch (currentWaterStatus) {
    case CRITICAL:
      // Fast toggle for critical
      if (currentTime - lastBuzzerToggle >= BUZZER_CRITICAL_INTERVAL) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER_PIN, buzzerState);
        lastBuzzerToggle = currentTime;
      }
      break;

    case WARNING:
      // Handle warning beep timing
      if (!warningBeepActive && (currentTime - lastWarningBeep >= BUZZER_WARNING_INTERVAL)) {
        // Start new beep
        warningBeepActive = true;
        digitalWrite(BUZZER_PIN, HIGH);
        lastWarningBeep = currentTime;
      } else if (warningBeepActive && (currentTime - lastWarningBeep >= BUZZER_BEEP_DURATION)) {
        // End current beep
        warningBeepActive = false;
        digitalWrite(BUZZER_PIN, LOW);
      }
      break;

    case WATCH:
    case NORMAL:
    default:
      // Turn off buzzer for normal conditions
      digitalWrite(BUZZER_PIN, LOW);
      buzzerState = false;
      warningBeepActive = false;
      floatAlarmActive = false;
      break;
  }
}