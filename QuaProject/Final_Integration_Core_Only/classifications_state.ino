void classifyWaterLevel() {
  if (!ultrasonicValid) {
    currentWaterStatus = NORMAL;
    return;
  }

  // Use reliable distance for classification (not raw reading)
  if (reliableDistance >= CRITICAL_LEVEL) {
    currentWaterStatus = CRITICAL;
  } else if (reliableDistance >= WARNING_LEVEL_80) {
    currentWaterStatus = WARNING;
  } else if (reliableDistance >= WARNING_LEVEL_50) {
    currentWaterStatus = WATCH;
  } else {
    currentWaterStatus = NORMAL;
  }
}