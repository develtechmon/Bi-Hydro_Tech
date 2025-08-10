// Replace SoftwareSerial with HardwareSerial for ESP32
HardwareSerial mySerial(2);

unsigned char buffer_RTT[4] = {0};
uint8_t CS;
#define COM 0x55
int Distance = 0;

void setup() {
  Serial.begin(115200);
  // Use TXD2/RXD2 pins on TTGO T-Call (GPIO 33/34)
  //mySerial.begin(115200, SERIAL_8N1, 44, 43);  // RX=44, TX=43
  mySerial.begin(115200, SERIAL_8N1, 33, 32);  // RX=44, TX=43 //12-White //13-Yellow
  Serial.println("TTGO T-Call sensor ready - put sensor underwater!");
}

void loop() {
  mySerial.write(COM);
  delay(100);
  
  if(mySerial.available() > 0){
    delay(4);
    if(mySerial.read() == 0xff){    
      buffer_RTT[0] = 0xff;
      for (int i=1; i<4; i++){
        buffer_RTT[i] = mySerial.read();   
      }
      CS = buffer_RTT[0] + buffer_RTT[1]+ buffer_RTT[2];  
      if(buffer_RTT[3] == CS) {
        Distance = (buffer_RTT[1] << 8) + buffer_RTT[2];
        Serial.print("Distance:");
        Serial.print(Distance);
        Serial.println("mm");
      }
    }
  }
}