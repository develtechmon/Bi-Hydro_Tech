#include <Wire.h>
#include <I2C_RTC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Hardware setup
static DS1307 RTC;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Alarm settings
#define BUZZER_PIN 15
int alarmHour = 2;      // Set your desired alarm hour (24-hour format)
int alarmMinute = 15;   // Set your desired alarm minute
bool alarmActive = false;
unsigned long alarmStartTime = 0;

void setup() {
    Serial.begin(9600);
    Wire.begin(21, 22);  // SDA=21, SCL=22
    
    // Setup buzzer pin
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Start OLED
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // Check RTC time
    if (RTC.getYear() <= 2024) {
        showMessage("Setting time...");
        RTC.setDateTime(__TIMESTAMP__);
        RTC.startClock();
        delay(1000);
    }
    
    // Show alarm time on startup
    String alarmMsg = "Alarm: ";
    if (alarmHour < 10) alarmMsg += "0";
    alarmMsg += String(alarmHour) + ":";
    if (alarmMinute < 10) alarmMsg += "0";
    alarmMsg += String(alarmMinute);
    
    showMessage(alarmMsg);
    delay(2000);
    showMessage("Clock Ready!");
    delay(1000);
}

void loop() {
    checkAlarm();
    showClock();
    delay(1000);
}

void showClock() {
    display.clearDisplay();
    
    // Day name - big text at top
    String days[] = {"", "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    display.setTextSize(2);
    display.setCursor(25, 0);
    display.print(days[RTC.getWeek()]);
    
    // Date - middle
    display.setTextSize(1);
    display.setCursor(15, 25);
    if (RTC.getDay() < 10) display.print("0");
    display.print(RTC.getDay());
    display.print("-");
    if (RTC.getMonth() < 10) display.print("0");
    display.print(RTC.getMonth());
    display.print("-");
    display.print(RTC.getYear());
    
    // Time - big text at bottom
    display.setTextSize(2);
    display.setCursor(5, 40);
    if (RTC.getHours() < 10) display.print("0");
    display.print(RTC.getHours());
    display.print(":");
    if (RTC.getMinutes() < 10) display.print("0");
    display.print(RTC.getMinutes());
    display.print(":");
    if (RTC.getSeconds() < 10) display.print("0");
    display.print(RTC.getSeconds());
    
    // Show alarm indicator if active
    if (alarmActive) {
        display.setTextSize(1);
        display.setCursor(90, 55);
        display.print("ALARM!");
    }
    
    display.display();
}

void showMessage(String msg) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.print(msg);
    display.display();
}

void checkAlarm() {
    int currentHour = RTC.getHours();
    int currentMinute = RTC.getMinutes();
    
    // Check if current time matches alarm time
    if (currentHour == alarmHour && currentMinute == alarmMinute) {
        if (!alarmActive) {
            // Start alarm
            alarmActive = true;
            alarmStartTime = millis();
            Serial.println("ALARM TRIGGERED!");
        }
    }
    
    // Handle alarm buzzing
    if (alarmActive) {
        // Buzz for 60 seconds (60000 milliseconds)
        if (millis() - alarmStartTime < 60000) {
            // Create beeping pattern: 500ms on, 500ms off
            if ((millis() / 500) % 2 == 0) {
                digitalWrite(BUZZER_PIN, HIGH);
            } else {
                digitalWrite(BUZZER_PIN, LOW);
            }
        } else {
            // Stop alarm after 60 seconds
            alarmActive = false;
            digitalWrite(BUZZER_PIN, LOW);
            Serial.println("Alarm stopped");
        }
    }
}