#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Ethernet.h>
#include <PubSubClient.h>

#define fallingPin 3
#define windDirPin A7
#define MAC_6    0x73
//#define MAC_6    0x62
static uint8_t mymac[6] = { 0x44, 0x76, 0x58, 0x10, 0x00, MAC_6 };
char bufa[80];
char bufa2[80];
byte server[] = { 10,6,0,21 }; 
unsigned int Port = 1883;  
EthernetClient ethClient; 
PubSubClient client(server, Port, ethClient); 

#define outTopic   "ICT4_out_2020" // Aihe, jolle viesti lähetetään
int avgSpeed = 0;
int avgDir = 0;
char* clientId = "benguin24";
boolean mqttSend = false; 

void send_MQTT_message() {
    if (!client.connected()) { // Tarkistetaan onko yhteys MQTT-brokeriin muodostettu
        connect_MQTT_server(); // Jos yhteyttä ei ollut, kutsutaan yhdistä -funktiota
    }
    if (client.connected()) { // Jos yhteys on muodostettu
        if(mqttSend) {
          sprintf(bufa, "IOTJS={\"S_name1\":\"Benguin_windDir\",\"S_value1\":%d}", avgDir);
        } else {
          sprintf(bufa, "IOTJS={\"S_name1\":\"Benguin_windSpeed\",\"S_value1\":%d}", avgSpeed);
        }
        Serial.println(bufa);
        client.publish(outTopic, bufa); // Lähetetään viesti MQTT-brokerille
        Serial.println("Message sent to MQTT server."); // Tulostetaan viesti onnistuneesta lähettämisestä
    } else {
        Serial.println("Failed to send message: not connected to MQTT server."); // Ei yhteyttä -> Yhteysvirheilmoitus
    }
}

void connect_MQTT_server() { 
    Serial.println("Connecting to MQTT"); // Tulostetaan vähän info-viestiä
    if (client.connect(clientId)) { // Tarkistetaan saadaanko yhteys MQTT-brokeriin
        Serial.println("Connected OK"); // Yhdistetty onnistuneesti
    } else {
        Serial.println("Connection failed."); // Yhdistäminen epäonnistui
    }    
}

const byte ROWS = 1;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
};
byte rowPins[ROWS] = {A4}; 
byte colPins[COLS] = {A0, A1, A2, A3}; 
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

volatile unsigned long previousMillis = 0;
volatile unsigned long displayMillis = 0;
volatile unsigned long displayMillis2 = 0;
volatile unsigned long pulseWidth = 0;

volatile bool fallingEdgeDetected = false;
double frequency = 0;
double totalSpeed =0;
double totalDir =0;

boolean freqWindToggle = true;
boolean degDirToggle = false;

int analogVal = 0;
double windV = 0;

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
  char customKey = customKeypad.getKey();
  analogVal = analogRead(windDirPin);
  windV = analogVal / 204.6;

  if(customKey == '1'){
    freqWindToggle = !freqWindToggle;
  } 
  if(customKey == '2') {
    degDirToggle = !degDirToggle; 
  }
  if(customKey == '3'){
    mqttSend = !mqttSend;
  }
  if(customKey == 'A'){
    displayD();
  }

  if (fallingEdgeDetected) {
    volatile unsigned long currentMillis = millis();
    pulseWidth = currentMillis - previousMillis;
    previousMillis = currentMillis;
    fallingEdgeDetected = false;
    frequency = round(1000.0 / pulseWidth);
  }
  if(displayMillis+1000 <= millis()){
      totalSpeed += freqToWindSpeed(frequency);
      totalDir += voltToDeg (windV);
      displayLCD();
      displayMillis = millis();
  }
   if(displayMillis2+5000 <= millis()){
      avgSpeed = round(totalSpeed/5);
      avgDir = round(totalDir/5);
      Serial.println("Dir:" + String(avgDir));
      clearSpecificPart(3);
      lcd.setCursor(0,3);
      lcd.print("S: " + String(avgSpeed) + " D: " + String(avgDir));
      displayMillis2 = millis();
      totalSpeed =0;
      totalDir =0;
      send_MQTT_message();
      avgSpeed = 0;
      avgDir = 0;
  }
}

void displayLCD() {
   //lcd.clear();
   clearSpecificPart(0);
   clearSpecificPart(1);
   //clearSpecificPart(2);
   lcd.setCursor(0,0);
   if (freqWindToggle) {
      // Display speed
      lcd.print("Spd.: " + String(freqToWindSpeed(frequency)) + "ms " + String(windV) + "V");
   } else {
      // Display frequency
      lcd.print("Freq: " + String(frequency) + "Hz " + String(windV) + "V");
   }
   lcd.setCursor(0,1);
   lcd.print("Dir: ");
   if(degDirToggle) {
      lcd.print(String(voltToDeg(windV)) + "Deg");
   } else {
      lcd.print(String(voltToDir(windV)));
   }
}

double freqToWindSpeed(double freq) {
  //return ((0.0019*pow(freq, 3))-(0.0936*pow(freq,2)) + (1.9827*freq) - 5.0893);
  return -0.24+(freq * 0.699);
}


int voltToDeg(double windV) {
  if (windV <= 1.2) return 0;
  if (windV >= 5) return 360;
  return round((windV/5) * 360);
}

String voltToDir(double windV){
    String directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW", "N"};
    //int index = (int)(floor((voltToDeg(windV)) + 22.5) / 45) % 8;
    int index = round((voltToDeg(windV)) / 45);
    return directions[index];
}
void averageSpeedDir(double totalSpeed, double totalDir){
    clearSpecificPart(3);
    lcd.setCursor(0,3);
    lcd.print("S: " + String(totalSpeed/5) + " D: " + String(totalDir/5));
}

void detectFalling() {
  fallingEdgeDetected = true;
}
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
     lcd.setCursor(0,2);
   //lcd.print("IP: " + String(Ethernet.localIP()));
   lcd.print(Ethernet.localIP());
  delay(1500);
}
void clearSpecificPart(int row) {
  lcd.setCursor(0, row);
  for (int i = 0; i <= 19; i++) {
    lcd.print(' '); // Print a space character to clear the part
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
//    lcd.clear();
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
