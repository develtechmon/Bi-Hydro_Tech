#include <Wire.h>
#include <I2C_RTC.h>

static DS1307 RTC;
//static DS3231 RTC;
//static PCF8523 RTC;
//static PCF8563 RTC;
//static MCP7940 RTC;

#define CUSTOM_SDA_PIN 21       
#define CUSTOM_SCL_PIN 22

int hours, minutes, seconds, day, month, year;
bool timeSet = false;

void setup()
{
    Serial.begin(9600);
    Wire.begin(CUSTOM_SDA_PIN, CUSTOM_SCL_PIN);

    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB
    }
    
    Serial.println("=== RTC Setup ===");
    Serial.print("Compile timestamp: ");
    Serial.println(__TIMESTAMP__);
    
    // Check if RTC time looks reasonable (not stuck in 2000)
    year = RTC.getYear();
    Serial.print("RTC current year: ");
    Serial.println(year);
    
    // If year is 2000 or less than 2024, time needs to be set
    if (year <= 2024) {
        Serial.println("RTC time appears incorrect. Setting new time...");
        
        RTC.setHourMode(CLOCK_H24); // Use 24-hour format for clarity
        RTC.setDateTime(__TIMESTAMP__); 
        RTC.startClock();
        
        Serial.println("Time updated with compile timestamp");
        Serial.println("NOTE: This is in your computer's timezone, not Malaysia time!");
        Serial.println("You may need to manually adjust for UTC+8 (Malaysia time)");
        
        timeSet = true;
        delay(2000); // Give RTC time to settle
    }
    
    Serial.println("\n=== Current RTC Reading ===");
    displayTime();
    
    if (timeSet) {
        Serial.println("\n=== IMPORTANT ===");
        Serial.println("If this time is not Malaysia time (UTC+8),");
        Serial.println("you need to:");
        Serial.println("1. Calculate the time difference");
        Serial.println("2. Recompile when your computer shows Malaysia time, OR");
        Serial.println("3. Add timezone offset in code");
    }
}

void loop()
{
    // Display time every 10 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 10000) {
        displayTime();
        lastUpdate = millis();
    }
}

void displayTime()
{
    if (!RTC.isRunning()) {
        Serial.println("ERROR: RTC is not running!");
        return;
    }
    
    // Get day of week
    switch (RTC.getWeek())
    {
        case 1: Serial.print("SUN"); break;
        case 2: Serial.print("MON"); break;
        case 3: Serial.print("TUE"); break;
        case 4: Serial.print("WED"); break;
        case 5: Serial.print("THU"); break;
        case 6: Serial.print("FRI"); break;
        case 7: Serial.print("SAT"); break;
        default: Serial.print("???"); break;
    }
    Serial.print(" ");
    
    // Get date
    day = RTC.getDay();
    month = RTC.getMonth();
    year = RTC.getYear();
    
    // Format date as DD-MM-YYYY
    if(day < 10) Serial.print("0");
    Serial.print(day);
    Serial.print("-");
    if(month < 10) Serial.print("0");
    Serial.print(month);
    Serial.print("-");
    Serial.print(year);
    Serial.print(" ");
    
    // Get time
    hours = RTC.getHours();
    minutes = RTC.getMinutes(); 
    seconds = RTC.getSeconds();
    
    // Format time as HH:MM:SS
    if(hours < 10) Serial.print("0");
    Serial.print(hours);
    Serial.print(":");
    if(minutes < 10) Serial.print("0");
    Serial.print(minutes);
    Serial.print(":");
    if(seconds < 10) Serial.print("0");
    Serial.print(seconds);
    
    // Show AM/PM if in 12-hour mode
    if (RTC.getHourMode() == CLOCK_H12) {
        switch (RTC.getMeridiem()) {
            case HOUR_AM: Serial.print(" AM"); break;
            case HOUR_PM: Serial.print(" PM"); break;
        }   
    }
    
    Serial.println();
}