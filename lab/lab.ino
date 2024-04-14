#include <Ethernet.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <PubSubClient.h>
#include <stdlib.h>

#define fallingPin 3
#define windDirPin A7

//-----------------------------Server-----------------------
byte server[] = { 10,6,0,21 };
unsigned int Port = 1883;  
EthernetClient ethClient; 
PubSubClient client(server, Port, ethClient); 

#define outTopic   "ICT4_out_2020"
double avgSpeed = 0;
double avgDir = 0;
boolean mqttSend = false; 
char bufa[100];

void send_MQTT_message() {
  char temp[5];
  dtostrf(avgSpeed, -5, 2, temp);
  char temp2[6];
  dtostrf(avgDir, -6, 2, temp2);
    if (!client.connected()) { 
        connect_MQTT_server(); 
    }
    if (client.connected()) { 
        if(mqttSend) {
          sprintf(bufa, "IOTJS={\"S_name1\":\"Benguin_windDir\",\"S_value1\":%s, \"S_name2\":\"Benguin_windSpeed\",\"S_value2\":%s}", temp2, temp);
        } else {
          sprintf(bufa, "IOTJS={\"S_name1\":\"Benguin_windSpeed\",\"S_value1\":%s}", temp);
        }
        client.publish(outTopic, bufa);
        Serial.println("Message sent to MQTT server."); 
    } else {
        Serial.println("Failed to send message: not connected to MQTT server."); 
    }
}

char* clientId = "benguin24";

void connect_MQTT_server() { 
    Serial.println("Connecting to MQTT");
    if (client.connect(clientId)) { 
        Serial.println("Connected OK");
    } else {
        Serial.println("Connection failed."); 
    }    
}

//---------------------------------ETHERNET----------------------------
static uint8_t mymac[6] = { 0x44, 0x76, 0x58, 0x10, 0x00, 0x73 };

void fetchIP() {
  byte connection = 1;
  connection = Ethernet.begin(mymac);
  Serial.print(F("\nW5100 Revision "));
  if (connection == 0) {
    Serial.println(F("Failed to access Ethernet controller"));
  }
  Serial.println(F("Setting up DHCP"));
  Serial.print("Connected with IP: ");
  Serial.println(Ethernet.localIP());
  delay(1500);
}


//----------------------------BOARD--------------------------------------
volatile unsigned long previousMillis = 0, displayMillis = 0, displayMillis2 = 0, pulseWidth = 0;
double frequency = 0, totalSpeed = 0, totalDir = 0, windV = 0;

boolean freqWindToggle = true;
boolean degDirToggle = false;

const byte ROWS = 1;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
};
byte rowPins[ROWS] = {A4}; 
byte colPins[COLS] = {A3, A2, A1, A0}; 
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

const int rs = 8, en = 6, d4 = 5, d5 = 4, d6 = 7, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
  lcd.begin(20, 4);
  pinMode(fallingPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(fallingPin), detectFalling, FALLING);
  Serial.begin(9600);
  fetchIP();
}

void loop() {
  windV = analogRead(windDirPin) / 204.6;

  switch(customKeypad.getKey()) {
      case '1':
          //Change between frequency and speed
          freqWindToggle = !freqWindToggle;
          break;
      case '2':
          //Change between degrees and cardinal direction
          degDirToggle = !degDirToggle;
          break;
      case '3':
          //change between speed and direction for mqtt send
          mqttSend = !mqttSend;
          break;
      case 'A':
          //pippeli
          displayD();
          break;
  }
  //Every 1 Second
  if(displayMillis+1000 <= millis()){
      totalSpeed += freqToWindSpeed(frequency);
      totalDir += voltToDeg (windV);
      displayLCD();
      displayMillis = millis();
  }
  //Every 5 Seconds
  if(displayMillis2+5000 <= millis()){
      avgSpeed = totalSpeed/5;
      avgDir = totalDir/5;
      clearSpecificPart(3);
      lcd.setCursor(0,3);
      lcd.print("S: " + String(avgSpeed) + " D: " + String(avgDir));
      displayMillis2 = millis();
      send_MQTT_message();
      totalSpeed = 0;
      totalDir = 0;
      avgSpeed = 0;
      avgDir = 0;
  }
}

void detectFalling() {
  volatile unsigned long currentMillis = millis();
  pulseWidth = currentMillis - previousMillis;
  previousMillis = currentMillis;
  frequency = round(1000.0 / pulseWidth);
}

void displayLCD() {
   clearSpecificPart(0);
   clearSpecificPart(1);
   lcd.setCursor(0,0);
   if (freqWindToggle) {
      lcd.print("Spd.: " + String(freqToWindSpeed(frequency)) + "ms " + String(windV) + "V");
   } else {
      lcd.print("Freq: " + String(frequency) + "Hz " + String(windV) + "V");
   }
   lcd.setCursor(0,1);
   lcd.print("Dir: ");
   if(degDirToggle) {
      lcd.print(String(voltToDeg(windV)));
      lcd.write(0xDF);
   } else {
      lcd.print(voltToDir(windV));
   }
   lcd.setCursor(0,2);
   lcd.print(Ethernet.localIP());
}

double freqToWindSpeed(double freq) {
  return -0.24+(freq * 0.699);
}

double voltToDeg(double windV) {
  if (windV <= 1.2) return 0;
  if (windV > 1.2 && windV <= 1.67)  return ((windV - 1.2) / 0.47) * 45;
  if (windV > 1.67 && windV <= 2.15) return ((windV - 1.67)/ 0.48) * 45 + 45;
  if (windV > 2.15 && windV <= 2.63) return ((windV - 2.15)/ 0.48) * 45 + 90;
  if (windV > 2.63 && windV <= 3.10) return ((windV - 2.63)/ 0.47) * 45 + 135;
  if (windV > 3.10 && windV <= 3.58) return ((windV - 3.10)/ 0.48) * 45 + 180;
  if (windV > 3.58 && windV <= 4.05) return ((windV - 3.58)/ 0.47) * 45 + 225;
  if (windV > 4.05 && windV <= 4.53) return ((windV - 4.05)/ 0.48) * 45 + 270;
  if (windV > 4.53 && windV < 5) return ((windV - 4.53)/(0.47)) * 45 + 315;
  return 360;
}

String voltToDir(double windV){
    String directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW", "N"};
    int index = round(voltToDeg(windV) / 45);
    return directions[index];
}

void clearSpecificPart(int row) {
  lcd.setCursor(0, row);
  for (int i = 0; i <= 19; i++) {
    lcd.print(' ');
  }
}

void displayD(){
     byte customCharacters[8][8] = {
        {B00111, B01000, B10000, B10000, B10000, B10000, B01000, B00110}, // bU1
        {B00110, B01000, B10000, B10000, B10000, B10000, B01000, B00111}, // bD1
        {B00000, B10000, B01000, B01000, B01000, B01111, B10000, B00000}, // bU2
        {B00000, B10000, B01111, B01000, B01000, B01000, B10000, B00000}, // bD2
        {B00000, B00000, B00000, B00000, B00000, B11111, B00000, B00000}, // shaftU
        {B00000, B00000, B11111, B00000, B00000, B00000, B00000, B00000}, // shaftD
        {B00000, B00000, B00000, B01100, B10010, B10011, B00001, B00111}, // tU1
        {B00011, B00001, B10011, B10010, B01100, B00000, B00000, B00000}, // tD1
    };
    for(int j =0; j< 16; j++){
    clearSpecificPart(0);
    clearSpecificPart(1);
    int positions[8][2] = {{j, 0},{j, 1}, {j+1, 0},{j+1, 1},{j+2, 0},{j+2, 1},{j+3, 0},{j+3, 1}};
    for (int i = 0; i < 8; i++) {
        lcd.createChar(i, customCharacters[i]);
        lcd.setCursor(positions[i][0], positions[i][1]);
        lcd.write((char)i);
    }
    delay(300);
    }
}
