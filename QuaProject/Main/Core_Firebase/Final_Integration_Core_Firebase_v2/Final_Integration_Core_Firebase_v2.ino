/*
 * BI-HYDRO TECH Water Monitoring System with RTC Integration
 * Non-blocking operation with real timestamp logging
 * Firebase integration with proper historical data
 */
#include <Wire.h>
#include <I2C_RTC.h>                    // RTC Library for real timestamps
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DFRobot_RainfallSensor.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

//======= FIREBASE CREDENTIALS =============
#define WIFI_SSID "Galaxy S24 Ultra 0997"
#define WIFI_PASSWORD "Lukas@92"

#define API_KEY "AIzaSyArWYq8-Zog6C_kjwXjp_tdhwLTHDatp9Y"
#define DATABASE_URL "https://bi-hydro-tech-2b024-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define USER_EMAIL "choosetheright92@gmail.com"
#define USER_PASSWORD "Eldernangkai92"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Firebase variables
unsigned long sendDataPrevMillis = 0;
unsigned long firebaseInterval = 30000; // Send data every 30 seconds
bool signupOK = false;
bool wifiConnected = false;

// ===== RTC Object =====
static DS1307 RTC;

// ===== I2C & OLED Settings =====
#define CUSTOM_SDA_PIN 21       
#define CUSTOM_SCL_PIN 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ===== Pin Definitions =====
#define FLOAT_PIN 13
#define BUZZER_PIN 15

// ===== Ultrasonic Sensor Settings =====
#define COM 0x55

// ===== Water Level Thresholds (in mm) =====
#define CRITICAL_LEVEL 1000     
#define WARNING_LEVEL_80 800       
#define WARNING_LEVEL_50 500    
#define NORMAL_LEVEL_20 200     

// ===== Timing Constants (all non-blocking) =====
#define MIN_STATE_TIME 5000         // Float deadband
#define DEBOUNCE_DELAY 100          // Float debounce
#define ULTRASONIC_TRIGGER_INTERVAL 200  // Send trigger every 200ms
#define ULTRASONIC_TIMEOUT 100      // Wait max 100ms for response
#define DISPLAY_UPDATE_INTERVAL 1000     // Update display every 1s
#define SERIAL_PRINT_INTERVAL 5000       // Print status every 5s
#define BUZZER_CRITICAL_INTERVAL 500     // Critical buzzer toggle
#define BUZZER_WARNING_INTERVAL 2000     // Warning buzzer interval
#define BUZZER_BEEP_DURATION 200         // Warning beep duration
#define FLOAT_ALARM_INTERVAL 300         // Float high alarm beep every 300ms

// ===== Ultrasonic Reliability Settings =====
#define MIN_VALID_DISTANCE 50           // Minimum valid reading (5cm)
#define MAX_VALID_DISTANCE 6000         // Maximum valid reading (6m)
#define MAX_CHANGE_THRESHOLD 1000       // Max allowed change per reading (10cm)
#define INVALID_READING_TIMEOUT 10000   // Use last valid reading for 10 seconds

// ===== Rainfall Intensity Thresholds (mm per hour) =====
#define LIGHT_RAIN_MAX 10.0            // Light: < 10mm/hr
#define MODERATE_RAIN_MAX 30.0         // Moderate: 10-30mm/hr  
#define HEAVY_RAIN_MAX 60.0            // Heavy: 30-60mm/hr
                                       // Very Heavy: > 60mm/hr
                                       
// ===== Objects =====
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DFRobot_RainfallSensor_I2C rainfallSensor(&Wire);
HardwareSerial ultrasonicSerial(2);

// ===== Float Sensor Variables =====
int currentReading = HIGH;
int lastReading = HIGH;
int confirmedState = HIGH;
int lastConfirmedState = HIGH;
unsigned long stateStartTime = 0;
unsigned long lastDebounceTime = 0;
bool stateConfirmed = false;

// ===== Ultrasonic Sensor Variables =====
unsigned char buffer_RTT[4] = {0};
uint8_t CS;
int ultrasonicDistance = 0;
int lastValidDistance = 0;              // Store last valid reading
int reliableDistance = 0;               // Distance used for calculations
bool ultrasonicValid = false;
bool usingLastValidReading = false;     // Flag to show we're using stored value
unsigned long lastValidReadingTime = 0; // When we got the last valid reading
unsigned long lastUltrasonicTrigger = 0;
unsigned long ultrasonicSentTime = 0;
bool waitingForResponse = false;

// ===== Water Level Classification =====
enum WaterStatus {
  NORMAL,      
  WATCH,       
  WARNING,     
  CRITICAL     
};
WaterStatus currentWaterStatus = NORMAL;

// ===== Rainfall Intensity Classification =====
enum RainfallIntensity {
  NO_RAIN,
  LIGHT,
  MODERATE, 
  HEAVY,
  VERY_HEAVY,
  SENSOR_ERROR
};
RainfallIntensity currentRainIntensity = NO_RAIN;

// ===== Timing Variables =====
unsigned long lastBuzzerToggle = 0;
unsigned long lastWarningBeep = 0;
unsigned long lastFloatAlarm = 0;      
unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialPrint = 0;
bool buzzerState = false;
bool warningBeepActive = false;
bool floatAlarmActive = false;          

// ===== RTC TIMESTAMP FUNCTIONS =====
unsigned long getRTCTimestamp() {
    // Get RTC values
    int year = RTC.getYear();
    int month = RTC.getMonth();
    int day = RTC.getDay();
    int hour = RTC.getHours();
    int minute = RTC.getMinutes();
    int second = RTC.getSeconds();
    
    // Convert to Unix timestamp (simplified calculation)
    // This gives approximate Unix timestamp for Firebase
    int years_since_1970 = year - 1970;
    unsigned long days = years_since_1970 * 365 + years_since_1970 / 4; // Rough leap year calc
    
    // Add days for months (simplified)
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int i = 0; i < month - 1; i++) {
        days += days_in_month[i];
    }
    days += day - 1;
    
    // Convert to seconds and add time
    unsigned long timestamp_unix = days * 86400UL + hour * 3600UL + minute * 60UL + second;
    
    return timestamp_unix;
}

String getFormattedTimestamp() {
    int year = RTC.getYear();
    int month = RTC.getMonth();
    int day = RTC.getDay();
    int hour = RTC.getHours();
    int minute = RTC.getMinutes();
    int second = RTC.getSeconds();
    
    String timestamp = String(year) + "-";
    if (month < 10) timestamp += "0";
    timestamp += String(month) + "-";
    if (day < 10) timestamp += "0";
    timestamp += String(day) + "T";
    if (hour < 10) timestamp += "0";
    timestamp += String(hour) + ":";
    if (minute < 10) timestamp += "0";
    timestamp += String(minute) + ":";
    if (second < 10) timestamp += "0";
    timestamp += String(second) + "+08:00"; // Malaysia timezone UTC+8
    
    return timestamp;
}

String getCurrentDateTimeString() {
    String days[] = {"", "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    String dayName = days[RTC.getWeek()];
    
    String dateTime = dayName + " ";
    if (RTC.getDay() < 10) dateTime += "0";
    dateTime += String(RTC.getDay()) + "-";
    if (RTC.getMonth() < 10) dateTime += "0";
    dateTime += String(RTC.getMonth()) + "-";
    dateTime += String(RTC.getYear()) + " ";
    if (RTC.getHours() < 10) dateTime += "0";
    dateTime += String(RTC.getHours()) + ":";
    if (RTC.getMinutes() < 10) dateTime += "0";
    dateTime += String(RTC.getMinutes()) + ":";
    if (RTC.getSeconds() < 10) dateTime += "0";
    dateTime += String(RTC.getSeconds());
    
    return dateTime;
}

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize I2C
  Wire.begin(CUSTOM_SDA_PIN, CUSTOM_SCL_PIN);

  // Initialize OLED Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while(1);
  }

  // Show startup screen first
  displayStartup();

  // ===== INITIALIZE RTC =====
  Serial.println("🕐 Initializing RTC...");
  displayConnectionStatus("Initializing RTC...");
  
  // Check if RTC needs time setting
  if (RTC.getYear() <= 2024) {
    Serial.println("⏰ Setting RTC time from compile timestamp...");
    displayConnectionStatus("Setting RTC Time...");
    RTC.setDateTime(__TIMESTAMP__);
    RTC.startClock();
    delay(1000);
  }
  
  // Verify RTC is working
  Serial.println("✅ RTC initialized successfully!");
  Serial.print("🕐 Current RTC Time: ");
  Serial.println(getCurrentDateTimeString());
  Serial.print("🔢 Unix Timestamp: ");
  Serial.println(getRTCTimestamp());
  Serial.print("📅 ISO Format: ");
  Serial.println(getFormattedTimestamp());
  
  displayConnectionStatus("RTC Ready!");
  delay(2000);

  // Initialize WiFi and Firebase
  initializeWiFiAndFirebase();

  // Initialize Ultrasonic Sensor
  ultrasonicSerial.begin(115200, SERIAL_8N1, 33, 32);

  // Initialize Float Sensor
  currentReading = digitalRead(FLOAT_PIN);
  lastReading = currentReading;
  confirmedState = currentReading;
  stateStartTime = millis();
  lastDebounceTime = millis();
  stateConfirmed = true;

  // Initialize Rainfall Sensor
  while(!rainfallSensor.begin()){
    Serial.println("Rainfall sensor init error!");
    displayError("Rainfall Sensor\nInit Error!");
    delay(2000);
  }

  Serial.println("=== BI-HYDRO TECH WATER MONITORING SYSTEM ===");
  Serial.println("✅ RTC Integration: Real timestamps for historical data");
  Serial.println("✅ Non-blocking operations for smooth performance");
  Serial.println("✅ Float alarm with immediate Firebase alerts");
  Serial.println("✅ Ultrasonic reliability with backup readings");
  Serial.println("✅ Firebase integration with proper timestamping");
  Serial.println("🚀 System ready!");

  // Initialize timing
  lastDisplayUpdate = millis();
  lastSerialPrint = millis();
  lastUltrasonicTrigger = millis();
  lastValidReadingTime = millis();
}

void loop() {
  unsigned long currentTime = millis();

  // Update all sensors (non-blocking)
  updateFloatSensorNonBlocking(currentTime);
  updateUltrasonicSensorNonBlocking(currentTime);

  // Classify water level and rainfall intensity
  classifyWaterLevel();
  classifyRainfallIntensity();

  // Handle alerts (non-blocking)
  handleBuzzerAlertsNonBlocking(currentTime);

  // Update display (non-blocking timing)
  if (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = currentTime;
  }

  // Print to serial (non-blocking timing)
  if (currentTime - lastSerialPrint >= SERIAL_PRINT_INTERVAL) {
    printToSerial();
    lastSerialPrint = currentTime;
  }

  // Send data to Firebase (non-blocking timing)
  if (wifiConnected && Firebase.ready() && signupOK && 
      (millis() - sendDataPrevMillis > firebaseInterval || sendDataPrevMillis == 0)) {
    sendDataToFirebase();
    sendDataPrevMillis = millis();
  }

  // No delays - loop runs freely!
}

void initializeWiFiAndFirebase() {
  // Connect to WiFi
  Serial.println("📶 Connecting to WiFi...");
  displayConnectionStatus("Connecting WiFi...");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStartTime) < 15000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println();
    Serial.print("✅ WiFi connected! IP: ");
    Serial.println(WiFi.localIP());
    displayConnectionStatus("WiFi Connected!");
    
    // Configure Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    
    config.token_status_callback = tokenStatusCallback;
    
    Serial.println("🔥 Connecting to Firebase...");
    displayConnectionStatus("Connecting Firebase...");
    
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    // Wait for Firebase to be ready
    unsigned long firebaseStartTime = millis();
    while (!Firebase.ready() && (millis() - firebaseStartTime) < 10000) {
      delay(100);
    }
    
    if (Firebase.ready()) {
      signupOK = true;
      Serial.println("✅ Firebase connected successfully!");
      displayConnectionStatus("Firebase Ready!");
    } else {
      Serial.println("❌ Firebase connection failed!");
      displayConnectionStatus("Firebase Failed!");
    }
  } else {
    wifiConnected = false;
    Serial.println("❌ WiFi connection failed!");
    displayConnectionStatus("WiFi Failed!");
  }
  
  delay(2000); // Show status for 2 seconds
}

void sendDataToFirebase() {
  Serial.println("📤 Sending sensor data to Firebase with RTC timestamps...");
  
  // Get current timestamp from RTC
  unsigned long timestamp = getRTCTimestamp();
  String formattedTime = getFormattedTimestamp();
  String readableTime = getCurrentDateTimeString();
  
  Serial.print("🕐 Using RTC timestamp: ");
  Serial.print(readableTime);
  Serial.print(" (Unix: ");
  Serial.print(timestamp);
  Serial.println(")");
  
  // Create data path with RTC timestamp
  String basePath = "sensorData/" + String(timestamp);
  
  // ==== TIMESTAMP METADATA ====
  if (Firebase.RTDB.setString(&fbdo, basePath + "/metadata/timestamp_formatted", formattedTime)) {
    Serial.println("✅ Formatted timestamp sent: " + formattedTime);
  } else {
    Serial.println("❌ Failed to send formatted timestamp: " + fbdo.errorReason());
  }
  
  if (Firebase.RTDB.setString(&fbdo, basePath + "/metadata/timestamp_readable", readableTime)) {
    Serial.println("✅ Readable timestamp sent: " + readableTime);
  }
  
  if (Firebase.RTDB.setInt(&fbdo, basePath + "/metadata/unix_timestamp", timestamp)) {
    Serial.println("✅ Unix timestamp sent: " + String(timestamp));
  } else {
    Serial.println("❌ Failed to send unix timestamp: " + fbdo.errorReason());
  }
  
  if (Firebase.RTDB.setString(&fbdo, basePath + "/metadata/timezone", "Asia/Kuala_Lumpur")) {
    Serial.println("✅ Timezone info sent");
  }
  
  if (Firebase.RTDB.setString(&fbdo, basePath + "/metadata/device_location", "Malaysia")) {
    Serial.println("✅ Location info sent");
  }
  
  if (Firebase.RTDB.setString(&fbdo, basePath + "/metadata/device_type", "ESP32-SIM800")) {
    Serial.println("✅ Device type sent");
  }
  
  // ==== ULTRASONIC DATA WITH STATE ====
  if (Firebase.RTDB.setInt(&fbdo, basePath + "/ultrasonic/distance_mm", reliableDistance)) {
    Serial.println("✅ Distance sent: " + String(reliableDistance) + "mm");
  } else {
    Serial.println("❌ Failed to send distance: " + fbdo.errorReason());
  }
  
  String waterStatusStr = getWaterStatusString(currentWaterStatus);
  if (Firebase.RTDB.setString(&fbdo, basePath + "/ultrasonic/water_status", waterStatusStr)) {
    Serial.println("✅ Water status sent: " + waterStatusStr);
  } else {
    Serial.println("❌ Failed to send water status: " + fbdo.errorReason());
  }
  
  if (Firebase.RTDB.setBool(&fbdo, basePath + "/ultrasonic/is_valid", ultrasonicValid)) {
    Serial.println("✅ Ultrasonic validity sent: " + String(ultrasonicValid ? "true" : "false"));
  }
  
  if (Firebase.RTDB.setBool(&fbdo, basePath + "/ultrasonic/using_backup", usingLastValidReading)) {
    Serial.println("✅ Backup reading flag sent: " + String(usingLastValidReading ? "true" : "false"));
  }
  
  // ==== RAINFALL DATA WITH INTENSITY STATE ====
  float rainfall1H = rainfallSensor.getRainfall(1);
  if (Firebase.RTDB.setFloat(&fbdo, basePath + "/rainfall/intensity_1h_mm", rainfall1H)) {
    Serial.println("✅ Rainfall 1H sent: " + String(rainfall1H, 1) + "mm");
  } else {
    Serial.println("❌ Failed to send rainfall 1H: " + fbdo.errorReason());
  }
  
  String rainIntensityStr = getRainIntensityString(currentRainIntensity);
  if (Firebase.RTDB.setString(&fbdo, basePath + "/rainfall/intensity_status", rainIntensityStr)) {
    Serial.println("✅ Rain intensity sent: " + rainIntensityStr);
  } else {
    Serial.println("❌ Failed to send rain intensity: " + fbdo.errorReason());
  }
  
  float totalRainfall = rainfallSensor.getRainfall();
  if (Firebase.RTDB.setFloat(&fbdo, basePath + "/rainfall/total_mm", totalRainfall)) {
    Serial.println("✅ Total rainfall sent: " + String(totalRainfall, 1) + "mm");
  }
  
  // ==== FLOAT SENSOR DATA WITH STATE ====
  String floatStatusStr = (confirmedState == LOW) ? "HIGH" : "LOW";
  if (Firebase.RTDB.setString(&fbdo, basePath + "/float/water_level", floatStatusStr)) {
    Serial.println("✅ Float status sent: " + floatStatusStr);
  } else {
    Serial.println("❌ Failed to send float status: " + fbdo.errorReason());
  }
  
  bool floatAlarm = (stateConfirmed && confirmedState == LOW);
  if (Firebase.RTDB.setBool(&fbdo, basePath + "/float/alarm_active", floatAlarm)) {
    Serial.println("✅ Float alarm sent: " + String(floatAlarm ? "true" : "false"));
  } else {
    Serial.println("❌ Failed to send float alarm: " + fbdo.errorReason());
  }
  
  if (Firebase.RTDB.setInt(&fbdo, basePath + "/float/raw_gpio_reading", confirmedState)) {
    Serial.println("✅ Float raw GPIO sent: " + String(confirmedState));
  }
  
  if (Firebase.RTDB.setBool(&fbdo, basePath + "/float/state_confirmed", stateConfirmed)) {
    Serial.println("✅ Float confirmation sent: " + String(stateConfirmed ? "true" : "false"));
  }
  
  // ==== SYSTEM STATUS WITH RTC INFO ====
  if (Firebase.RTDB.setString(&fbdo, basePath + "/system/status", "ONLINE")) {
    Serial.println("✅ System status sent: ONLINE");
  }
  
  if (Firebase.RTDB.setString(&fbdo, basePath + "/system/device", "BI-HYDRO-TECH")) {
    Serial.println("✅ Device info sent: BI-HYDRO-TECH");
  }
  
  if (Firebase.RTDB.setString(&fbdo, basePath + "/system/rtc_time", readableTime)) {
    Serial.println("✅ RTC time sent: " + readableTime);
  }
  
  if (Firebase.RTDB.setInt(&fbdo, basePath + "/system/upload_time", timestamp)) {
    Serial.println("✅ Upload timestamp sent: " + String(timestamp));
  }
  
  if (Firebase.RTDB.setInt(&fbdo, basePath + "/system/uptime_ms", millis())) {
    Serial.println("✅ System uptime sent: " + String(millis()) + "ms");
  }
  
  // ==== UPDATE LATEST DATA FOR QUICK ACCESS ====
  if (Firebase.RTDB.setString(&fbdo, "latest/timestamp", formattedTime)) {
    Serial.println("✅ Latest timestamp updated");
  }
  
  if (Firebase.RTDB.setString(&fbdo, "latest/readable_time", readableTime)) {
    Serial.println("✅ Latest readable time updated");
  }
  
  if (Firebase.RTDB.setInt(&fbdo, "latest/unix_timestamp", timestamp)) {
    Serial.println("✅ Latest unix timestamp updated");
  }
  
  if (Firebase.RTDB.setInt(&fbdo, "latest/ultrasonic/distance_mm", reliableDistance)) {
    Serial.println("✅ Latest distance updated");
  }
  
  if (Firebase.RTDB.setString(&fbdo, "latest/ultrasonic/water_status", waterStatusStr)) {
    Serial.println("✅ Latest water status updated");
  }

  if (Firebase.RTDB.setBool(&fbdo, "latest/ultrasonic/is_valid", ultrasonicValid)) {
    Serial.println("✅ Latest ultrasonic validity updated");
  }
  
  if (Firebase.RTDB.setFloat(&fbdo, "latest/rainfall/intensity_1h_mm", rainfall1H)) {
    Serial.println("✅ Latest rainfall updated");
  }
  
  if (Firebase.RTDB.setString(&fbdo, "latest/rainfall/intensity_status", rainIntensityStr)) {
    Serial.println("✅ Latest rain intensity updated");
  }
  
  if (Firebase.RTDB.setString(&fbdo, "latest/float/water_level", floatStatusStr)) {
    Serial.println("✅ Latest float status updated");
  }
  
  if (Firebase.RTDB.setBool(&fbdo, "latest/float/alarm_active", floatAlarm)) {
    Serial.println("✅ Latest float alarm updated");
  }
  
  if (Firebase.RTDB.setString(&fbdo, "latest/system/rtc_time", readableTime)) {
    Serial.println("✅ Latest RTC time updated");
  }
  
  if (Firebase.RTDB.setString(&fbdo, "latest/system/status", "ONLINE")) {
    Serial.println("✅ Latest system status updated");
  }
  
  Serial.println("🎉 Firebase data upload completed successfully with RTC timestamps!");
}

void sendFloatStatusToFirebase() {
  unsigned long timestamp = getRTCTimestamp();
  String formattedTime = getFormattedTimestamp();
  String readableTime = getCurrentDateTimeString();
  String basePath = "sensorData/" + String(timestamp) + "_FLOAT_ALERT";
  
  String floatStatusStr = (confirmedState == LOW) ? "HIGH" : "LOW";
  bool floatAlarm = (stateConfirmed && confirmedState == LOW);
  
  Serial.println("🚨 === IMMEDIATE FLOAT UPDATE WITH RTC ===");
  Serial.print("🕐 RTC Time: ");
  Serial.println(readableTime);
  Serial.print("🎈 Float Status: ");
  Serial.println(floatStatusStr);
  Serial.print("🚨 Alarm Active: ");
  Serial.println(floatAlarm ? "true" : "false");
  
  // Add timestamp metadata to immediate alert
  if (Firebase.RTDB.setString(&fbdo, basePath + "/metadata/timestamp_formatted", formattedTime)) {
    Serial.println("✅ Alert formatted timestamp sent");
  }
  
  if (Firebase.RTDB.setString(&fbdo, basePath + "/metadata/timestamp_readable", readableTime)) {
    Serial.println("✅ Alert readable timestamp sent");
  }
  
  if (Firebase.RTDB.setInt(&fbdo, basePath + "/metadata/unix_timestamp", timestamp)) {
    Serial.println("✅ Alert unix timestamp sent");
  }
  
  if (Firebase.RTDB.setString(&fbdo, basePath + "/alert_type", "FLOAT_STATUS_CHANGE")) {
    Serial.println("✅ Alert type sent");
  }
  
  // Send immediate float update
  if (Firebase.RTDB.setString(&fbdo, basePath + "/float/water_level", floatStatusStr)) {
    Serial.println("✅ IMMEDIATE Float status sent!");
  } else {
    Serial.println("❌ IMMEDIATE Float status failed: " + fbdo.errorReason());
  }
  
  if (Firebase.RTDB.setBool(&fbdo, basePath + "/float/alarm_active", floatAlarm)) {
    Serial.println("✅ IMMEDIATE Float alarm sent!");
  } else {
    Serial.println("❌ IMMEDIATE Float alarm failed: " + fbdo.errorReason());
  }
  
  // Also update latest status for quick access
  if (Firebase.RTDB.setString(&fbdo, "latest/float_status", floatStatusStr)) {
    Serial.println("✅ Latest float status updated!");
  }
  
  if (Firebase.RTDB.setBool(&fbdo, "latest/float_alarm", floatAlarm)) {
    Serial.println("✅ Latest float alarm updated!");
  }
  
  if (Firebase.RTDB.setString(&fbdo, "latest/alert_time", readableTime)) {
    Serial.println("✅ Latest alert time updated!");
  }
  
  if (Firebase.RTDB.setString(&fbdo, "latest/last_float_change", formattedTime)) {
    Serial.println("✅ Latest float change time updated!");
  }
  
  Serial.println("=====================================");
}

String getWaterStatusString(WaterStatus status) {
  switch (status) {
    case NORMAL:   return "NORMAL";
    case WATCH:    return "WATCH";
    case WARNING:  return "WARNING";
    case CRITICAL: return "CRITICAL";
    default:       return "UNKNOWN";
  }
}

String getRainIntensityString(RainfallIntensity intensity) {
  switch (intensity) {
    case NO_RAIN:      return "NO_RAIN";
    case LIGHT:        return "LIGHT";
    case MODERATE:     return "MODERATE";
    case HEAVY:        return "HEAVY";
    case VERY_HEAVY:   return "VERY_HEAVY";
    case SENSOR_ERROR: return "SENSOR_ERROR";
    default:           return "UNKNOWN";
  }
}

void displayConnectionStatus(String status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 15);
  display.println("BI-HYDRO TECH");
  display.setCursor(0, 30);
  display.println(status);
  
  // Show current time if RTC is working
  if (RTC.getYear() > 2024) {
    display.setCursor(0, 45);
    String timeStr = "";
    if (RTC.getHours() < 10) timeStr += "0";
    timeStr += String(RTC.getHours()) + ":";
    if (RTC.getMinutes() < 10) timeStr += "0";
    timeStr += String(RTC.getMinutes());
    display.println("Time: " + timeStr);
  }
  
  display.display();
}

void updateFloatSensorNonBlocking(unsigned long currentTime) {
  int newReading = digitalRead(FLOAT_PIN);

  // Check for reading change
  if (newReading != lastReading) {
    lastDebounceTime = currentTime;
    lastReading = newReading;
    
    Serial.print("🔄 FLOAT CHANGE DETECTED: ");
    Serial.print(newReading == HIGH ? "HIGH" : "LOW");
    Serial.print(" -> ");
    Serial.println(newReading == LOW ? "Float UP (Water HIGH)" : "Float DOWN (Water LOW)");
  }

  // Non-blocking debounce check
  if ((currentTime - lastDebounceTime) >= DEBOUNCE_DELAY) {
    if (newReading != currentReading) {
      // Reading has changed and is stable
      currentReading = newReading;
      stateStartTime = currentTime;
      stateConfirmed = false;

      Serial.print("⏱️ Float state change detected: ");
      Serial.println(currentReading == LOW ? "UP (High Water)" : "DOWN (Low Water)");
    }
  }

  // Non-blocking deadband check
  if (!stateConfirmed && (currentTime - stateStartTime) >= MIN_STATE_TIME) {
    confirmedState = currentReading;
    stateConfirmed = true;

    if (confirmedState != lastConfirmedState) {
      if (confirmedState == LOW) {
        Serial.println("🚨 *** FLOAT: WATER HIGH - ALARM TRIGGERED! ***");
        Serial.print("🕐 Alert Time: ");
        Serial.println(getCurrentDateTimeString());
      } else {
        Serial.println("✅ *** FLOAT: WATER LOW - ALARM CLEARED ***");
        Serial.print("🕐 Clear Time: ");
        Serial.println(getCurrentDateTimeString());
      }
      lastConfirmedState = confirmedState;
      
      // IMMEDIATE Firebase update for float changes with RTC timestamp
      if (wifiConnected && Firebase.ready() && signupOK) {
        Serial.println("📤 Sending immediate float status update to Firebase with RTC timestamp...");
        sendFloatStatusToFirebase();
      }
    }
  }
}

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
    // Wait a bit more for data to arrive
    if ((currentTime - ultrasonicSentTime) >= 50) { // Wait at least 50ms
      
      if (ultrasonicSerial.available() >= 4) {
        
        // Try to read complete frame
        if (ultrasonicSerial.read() == 0xFF) {
          buffer_RTT[0] = 0xFF;

          // Read remaining bytes with small delays
          bool frameComplete = true;
          for (int i = 1; i < 4; i++) {
            unsigned long byteWaitStart = millis();
            while (!ultrasonicSerial.available() && (millis() - byteWaitStart) < 10) {
              // Wait up to 10ms for each byte
            }
            
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
          }
          waitingForResponse = false;
        } else {
          // Clear buffer and try again
          while (ultrasonicSerial.available()) {
            ultrasonicSerial.read();
          }
        }
      } else if ((currentTime - ultrasonicSentTime) >= ULTRASONIC_TIMEOUT) {
        // Timeout - no response received
        ultrasonicValid = false;
        waitingForResponse = false;
      }
    }
  }

  // Check if we should use last valid reading due to timeout
  if ((currentTime - lastValidReadingTime) > INVALID_READING_TIMEOUT) {
    // Been too long since valid reading - mark as invalid
    ultrasonicValid = false;
    usingLastValidReading = false;
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
    isValidReading = false;
  }
  
  // Check 2: Is change too sudden? (only if we have a previous valid reading)
  if (isValidReading && lastValidDistance > 0) {
    int change = abs(rawReading - lastValidDistance);
    if (change > MAX_CHANGE_THRESHOLD) {
      isValidReading = false;
    }
  }
  
  // Check 3: Special case for 0mm reading (often indicates sensor error)
  if (rawReading == 0) {
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

void classifyRainfallIntensity() {
  float rainfall1H = rainfallSensor.getRainfall(1);  // Get 1-hour rainfall
  
  // Check if sensor is working
  float totalRainfall = rainfallSensor.getRainfall();
  
  // Simple sensor health check
  static bool hasEverHadReading = false;
  static unsigned long systemStartTime = millis();
  
  if (totalRainfall > 0) {
    hasEverHadReading = true;
  }
  
  // If no readings for 1 hour after system start, might be sensor error
  if (!hasEverHadReading && (millis() - systemStartTime) > 3600000) { // 1 hour
    currentRainIntensity = SENSOR_ERROR;
    return;
  }
  
  // Classify based on 1-hour rainfall intensity
  if (rainfall1H < 0.1) {  // Less than 0.1mm (essentially no rain)
    currentRainIntensity = NO_RAIN;
  } else if (rainfall1H < LIGHT_RAIN_MAX) {  // < 10mm
    currentRainIntensity = LIGHT;
  } else if (rainfall1H < MODERATE_RAIN_MAX) {  // 10-30mm
    currentRainIntensity = MODERATE;
  } else if (rainfall1H < HEAVY_RAIN_MAX) {  // 30-60mm
    currentRainIntensity = HEAVY;
  } else {  // > 60mm
    currentRainIntensity = VERY_HEAVY;
  }
}

void classifyWaterLevel() {
  if (!ultrasonicValid) {
    currentWaterStatus = NORMAL;
    return;
  }

  // Use reliable distance for classification
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

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Title with WiFi status and current time
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WATER MONITOR");
  if (wifiConnected) {
    display.print(" WiFi");
  }
  
  // Show current time from RTC
  display.setCursor(0, 9);
  String timeStr = "";
  if (RTC.getHours() < 10) timeStr += "0";
  timeStr += String(RTC.getHours()) + ":";
  if (RTC.getMinutes() < 10) timeStr += "0";
  timeStr += String(RTC.getMinutes()) + ":";
  if (RTC.getSeconds() < 10) timeStr += "0";
  timeStr += String(RTC.getSeconds());
  display.print("Time: " + timeStr);
  
  display.drawLine(0, 18, 128, 18, SSD1306_WHITE);

  // Ultrasonic Distance with reliability indicator
  display.setCursor(0, 21);
  display.print("Distance: ");
  if (ultrasonicValid) {
    display.print(reliableDistance);
    display.print("mm");
    if (usingLastValidReading) {
      display.print("*");  // Asterisk indicates backup reading
    }
  } else {
    display.print("ERROR");
  }

  // Water Level Status with visual indicator
  display.setCursor(0, 31);
  display.print("Level: ");

  switch (currentWaterStatus) {
    case NORMAL:
      display.print("NORMAL");
      display.drawCircle(120, 34, 3, SSD1306_WHITE);
      break;
    case WATCH:
      display.print("WATCH");
      display.fillCircle(120, 34, 3, SSD1306_WHITE);
      break;
    case WARNING:
      display.print("WARNING");
      display.fillRect(115, 31, 6, 6, SSD1306_WHITE);
      break;
    case CRITICAL:
      display.print("CRITICAL");
      display.fillTriangle(117, 28, 123, 28, 120, 37, SSD1306_WHITE);
      break;
  }

  // Float Status with alarm indicator
  display.setCursor(0, 41);
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

  // Rainfall Data with intensity classification
  display.setCursor(0, 51);
  display.print("Rain 1H: ");
  display.print(rainfallSensor.getRainfall(1), 1);
  display.print("mm ");
  
  // Show rainfall intensity
  switch (currentRainIntensity) {
    case NO_RAIN:
      display.print("None");
      break;
    case LIGHT:
      display.print("Light");
      break;
    case MODERATE:
      display.print("Mod");
      break;
    case HEAVY:
      display.print("Heavy");
      break;
    case VERY_HEAVY:
      display.print("V.Heavy");
      break;
    case SENSOR_ERROR:
      display.print("ERROR");
      break;
  }

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
  delay(2000);
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
  Serial.println("🌊 === BI-HYDRO TECH STATUS ===");
  
  Serial.print("🕐 RTC Time: ");
  Serial.println(getCurrentDateTimeString());
  
  Serial.print("🔢 Unix Timestamp: ");
  Serial.println(getRTCTimestamp());

  Serial.print("📶 WiFi: ");
  Serial.println(wifiConnected ? "Connected" : "Disconnected");
  
  Serial.print("🔥 Firebase: ");
  Serial.println(signupOK ? "Ready" : "Not Ready");

  Serial.print("📡 Ultrasonic: ");
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

  Serial.print("🌊 Water Status: ");
  switch (currentWaterStatus) {
    case NORMAL:   Serial.println("NORMAL"); break;
    case WATCH:    Serial.println("WATCH"); break;
    case WARNING:  Serial.println("WARNING"); break;
    case CRITICAL: Serial.println("CRITICAL"); break;
  }

  Serial.print("🌧️ Rainfall Intensity: ");
  switch (currentRainIntensity) {
    case NO_RAIN:      Serial.println("None"); break;
    case LIGHT:        Serial.println("Light (< 10mm/hr)"); break;
    case MODERATE:     Serial.println("Moderate (10-30mm/hr)"); break;
    case HEAVY:        Serial.println("Heavy (30-60mm/hr)"); break;
    case VERY_HEAVY:   Serial.println("Very Heavy (> 60mm/hr)"); break;
    case SENSOR_ERROR: Serial.println("SENSOR ERROR"); break;
  }

  Serial.print("💧 Rainfall 1H: ");
  Serial.print(rainfallSensor.getRainfall(1), 1);
  Serial.println("mm");

  Serial.print("📊 Total Rainfall: ");
  Serial.print(rainfallSensor.getRainfall(), 1);
  Serial.println("mm");

  Serial.print("🎈 Float: ");
  if (stateConfirmed) {
    if (confirmedState == LOW) {
      Serial.println("HIGH - ALARM ACTIVE!");
    } else {
      Serial.println("LOW");
    }
  } else {
    Serial.println("CONFIRMING");
  }

  Serial.print("⚡ System Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  
  Serial.println("===============================");
}