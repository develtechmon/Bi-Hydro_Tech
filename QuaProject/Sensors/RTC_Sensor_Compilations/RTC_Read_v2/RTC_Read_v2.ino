#include <Wire.h>
#include <I2C_RTC.h>

static DS1307 RTC;

#define SDA_PIN 21       
#define SCL_PIN 22

void setup() {
    Serial.begin(9600);
    Wire.begin(SDA_PIN, SCL_PIN);
    
    // Check if RTC has correct time (not stuck in year 2000)
    if (RTC.getYear() <= 2024) {
        Serial.println("Setting RTC to current time...");
        RTC.setDateTime(__TIMESTAMP__);
        RTC.startClock();
    }
    
    Serial.println("RTC Ready!");
}

void loop() {
    showTime();
    delay(1000);  // Update every second
}

void showTime() {
    // Get day name
    String days[] = {"", "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    Serial.print(days[RTC.getWeek()]);
    Serial.print(" ");
    
    // Get date (DD-MM-YYYY)
    if (RTC.getDay() < 10) Serial.print("0");
    Serial.print(RTC.getDay());
    Serial.print("-");
    if (RTC.getMonth() < 10) Serial.print("0");
    Serial.print(RTC.getMonth());
    Serial.print("-");
    Serial.print(RTC.getYear());
    Serial.print(" ");
    
    // Get time (HH:MM:SS)
    if (RTC.getHours() < 10) Serial.print("0");
    Serial.print(RTC.getHours());
    Serial.print(":");
    if (RTC.getMinutes() < 10) Serial.print("0");
    Serial.print(RTC.getMinutes());
    Serial.print(":");
    if (RTC.getSeconds() < 10) Serial.print("0");
    Serial.print(RTC.getSeconds());
    
    Serial.println();
}