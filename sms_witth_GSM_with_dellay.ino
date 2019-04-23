#include "ThingSpeak.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <SPI.h>
#include <RF24.h>
#include <PubSubClient.h>
#include <Servo.h>

#define ALARM_RF 0x04
#define ALARM_BATARY 0x02
#define ALARM_ENTRY 0x01
#define ALL_RIGHT 0
#define NUM1SW 0x01
#define NUM2SW 0x02
#define NUM3SW 0x04
#define NUM4SW 0x08
#define LIGHT D1
#define SIRENA D8
#define swOn |=
#define swOff &=
#define resw ^=

#define NUM1 "296778856"   //director
#define NUM2 "299951125"   //Safronov
#define NUM3 "296099811"   //Ivanov
#define NUM4 "295331230"   //Gennadiy


//Ticker mqtt;

RF24 radio(D4, D3);

Servo servo;

struct rfMesage {
  unsigned char state = 0;
  unsigned char charge = 0;
  unsigned char attempt = 0;
  unsigned char id = 0;
};

unsigned char numberSwich = 0;

rfMesage data, cels, lojAlarm;

int netFlag = 0;
int sendFlag = 1;
int lojAlarmFlag = 0;
int now;
int pora = 0;
int err = 0;
int ok = 0;
int otv = 0;
int ran = 0;
int first[5] = {1, 1, 1, 1, 1};
int gradServ[5] = {10, 60, 100, 140, 180};
int points[5] = {0, 0, 0, 0, 0};
int ignored[5] = {0, 0, 0, 0, 0};
int entryes[5] = {0, 0, 0, 0, 0};
int charges[5] = {50, 50, 50, 50, 50};
int lightOn = 0;
int perebor = 1;
int otlov = 0;
int otlOn = 0;

unsigned long prewT[5] = {0, 0, 0, 0, 0};
unsigned long lightTime[5] = {0, 0, 0, 0, 0};
unsigned long strobing = 0;
unsigned long povorotT = 0;
unsigned long reconT = 0;
unsigned long TSwriteT = 0;
unsigned long sendT = 0;
unsigned long startTime = 0;


//const char* ssid = "DisplayUniFi";
//const char* password = "01235689";

//const char* ssid = "MikroTik-64AAB99";
//const char* password = "";

const char* ssid = "TP-Link_Extender";
const char* password = "12345678";

//const char* ssid = "Huawei";
//const char* password = "eeebirat";


const char* mqtt_server = "m15.cloudmqtt.com";
const char* mqttUser = "qmttxpoj";
const char* mqttPass = "GM0KLN7oAnzX";
int MQTTport = 13217;

unsigned long myChannelNumber1 = 637754;
const char * myWriteAPIKey1 = "Y65N9ECMW9WK2MSM";

WiFiClient client;
PubSubClient MQTTclient(client);


void setup() {
  ESP.wdtEnable(0);
  EEPROM.begin(512);
  Serial.begin(9600);
  servo.attach(D2);
  wif_init();
  ThingSpeak.begin(client);
  MQTTclient.setServer(mqtt_server, MQTTport);
  MQTTclient.setCallback(callback);
  numberSwich = EEPROM.read(0);
  for (int i = 0; i < 5; i++) {
    ignored[i] = EEPROM.read(i + 1);
  }
  Serial.println(numberSwich);
  pinMode (LIGHT, OUTPUT);
  digitalWrite(LIGHT, LOW);
  pinMode (D8, OUTPUT);
  digitalWrite(D8, LOW);
  netFlag = 0;
  reqest("at+creg?\r\n");
  delay (1000);
  ans();
  //GSM_init();
  GSM_set_SMS_mode();
  delay(1000);
  radioInit();
  //mqtt.attach(0.1, mqttLoop);
}

void loop() {
  mqttLoop();
  lightning();
  radioListen();
  TSwrite();
}





//////////////////functions////////////////////////////
void TSwrite() {
  if ((millis() - TSwriteT > 15000)) {
    //noInterrupts();

    for (int i = 0; i < 5; i++) {
      if (entryes[i]) {
        entryes[i] = 0;
        ThingSpeak.setField( i + 1, data.charge );
        ThingSpeak.setField( 6, i + 1 );
        break;
      }
    }
    int zarad = analogRead(A0);
    float voltage = zarad * (15.38 / 1024);
    ThingSpeak.setField( 7, voltage );
    ThingSpeak.writeFields(myChannelNumber1, myWriteAPIKey1);

    TSwriteT = millis();
    //interrupts();
  }
}


void radioListen () {

  data.id = 0;

  if (radio.available()) {
    radio.read(&data, sizeof(data));
    if (ignored[data.id - 1]) {
      data.id = 0;
    }
  }

  if (lojAlarmFlag) {
    lojAlarmFlag = 0;
    data.id = lojAlarm.id;
    data.state = lojAlarm.state;
    data.charge = lojAlarm.charge;
  }

  if (data.id > 0 && data.id < 6) {
    if (data.state & ALARM_ENTRY) {

      entryes[data.id - 1] = 1;
      charges[data.id - 1] = data.charge;

      if (first[data.id - 1]) {

        if ( (millis() - sendT) > 8000 ) {
          GSM_send_SMS();
          sendT = millis();
        }

        first[data.id - 1] = 0;
        prewT[data.id - 1] = millis();

      } else if ( ( millis() - prewT[data.id - 1]) > 300000 ) {

        if ( (millis() - sendT) > 8000 ) {
          GSM_send_SMS();
          sendT = millis();
        }

        prewT[data.id - 1] = millis();
      }
    }
  }
}


void radioInit() {
  radio.begin();
  radio.setChannel(55);
  radio.setDataRate     (RF24_250KBPS);
  radio.setPALevel      (RF24_PA_MAX);
  radio.openReadingPipe (1, 1);
  radio.openReadingPipe (2, 2);
  radio.openReadingPipe (3, 3);
  radio.openReadingPipe (4, 4);
  radio.openReadingPipe (5, 5);
  radio.startListening  ();
}

void lightning() {

  if ( (data.state & ALARM_ENTRY) ) {

    if (otlOn != 1) {
      startTime = millis();
      otlOn = 1;
    }

    data.state = 0;
    otlov = data.id - 1;
  }

  if ((millis() - startTime) > 40000 && otlOn) {
    lightOn = 1;
    otlOn = 0;
    points[data.id - 1] = 1;
    digitalWrite(SIRENA, HIGH);
    lightTime[otlov] = millis();
  }

  if (lightOn) {
    lightOn = 0;

    for (int i = 0; i < 5; i++) {
      points [i] = 0;

      if ( (millis() - lightTime[i]) < 70000 ) {
        lightOn = 1;
        points[i] = 1;
      }
    }
  }

  if ( !lightOn ) {
    //lightOn = 0;
    for (int i = 0; i < 5; i ++) points[i] = 0;
    data.state = 0;
    //Serial.print("led swich OFF\n----------------------");
    digitalWrite(SIRENA, LOW);
    digitalWrite(LIGHT, LOW);
  }

  if (lightOn) {

    if ( (millis() - strobing) > 300) {
      //Serial.println("мырг");
      digitalWrite(LIGHT, !digitalRead(LIGHT));
      servo.write(gradServ[now] + ran);
      ran = random(-10, 11);
      strobing = millis();
    }

    if ( (millis() - povorotT) > 2000) {

      if (perebor > 5) perebor = 1;

      int count = 0;

      while ( (points[perebor - 1] == 0) && (count < 5) ) {
        perebor++;
        count++;
        if (perebor > 5) perebor = 1;
      }

      now = perebor - 1;
      servo.write(gradServ[now]);
      perebor++;
      povorotT = millis();
    }

    //servo.write(gradServ[now] + random(-10, 11));

  }

}


void GSM_init () {

  while (netFlag == 0) {
    delay (10000);
    reqest("at+creg?\r\n");
    delay (10);
    ans();
  }

  netFlag = 0;

  while (netFlag == 0) {
    delay (1000);
    reqest("at+creg?\r\n");
    delay (10);
    ans();
  }
}

void GSM_set_SMS_mode() {
  delay(500);
  reqest(" AT+CMGF=1\r\n");
  delay(500);
  reqest("AT+CSCS=\"GSM\"\r\n");
  delay(500);
}

void GSM_send_SMS() {
  reqest("\x1a");
  delay(300);
  dedam();
  /*reqest("\x1a");
  reqest("AT+CMGS=\"+375336649858\"\r\n");
  clearRXbuf();
  char text[120] = "violation, point \x1a";

  for (int i = 0; i < 120; i++) {

    if (text[i] == '\x1a') {
      text[i] = '0' + ( (int)data.id);
      text[i + 1] = '\x1a';
      break;
    }
  }

  clearRXbuf();
  delay(1000);
  reqest(text);
  delay(300);
  // Serial.end();
*/
}

void dedam () {

  char nums[5][10];
  strcpy(nums[0], NUM1);
  strcpy(nums[1], NUM2);
  strcpy(nums[2], NUM3);
  strcpy(nums[3], NUM4);

  for (int i = 0; i < 4; i++ ) {

    if (numberSwich & (1 << i) ) {
      delay(4000);
      reqest("\x1a");
      delay(300);
      char atNum[25] = "AT+CMGS=\"+375";

      for (int j = 0; j < 9; j++ ) {
        atNum[j + 13] = nums[i][j];
      }

      atNum[22] = '\"';
      atNum[23] = '\r';
      atNum[24] = '\n';

      delay(300);
      reqest(atNum);
      clearRXbuf();
      char text[120] = "violation, point \x1a";

      for (int i = 0; i < 120; i++) {

        if (text[i] == '\x1a') {
          text[i] = '0' + ( (int)data.id);
          text[i + 1] = '\x1a';
          break;
        }
      }

      clearRXbuf();
      delay(1000);
      reqest(text);
      delay(4000);
    }
  }
}


void wif_init() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint8_t i = 0;

  while (WiFi.status() != WL_CONNECTED && i++ < 20) delay(200);

  if (i == 5) {
    Serial.print("Could not connect to"); Serial.println(ssid);
  }

  ThingSpeak.begin(client);

}

void callback(char* topic, byte* payload, unsigned int length) {
  char mesaga[length];
  switch (payload[0]) {

    case 1:
      numberSwich = (payload[1] &
                     (NUM1SW |
                      NUM2SW |
                      NUM3SW |
                      NUM4SW));

      EEPROM.write(0, numberSwich);
      EEPROM.commit();
      break;

    case 2:

      for (int i = 0; i < 5; i++) {
        ignored[i] = (payload[i + 1] & 1);
        EEPROM.write( (i + 1), (payload[i + 1] & 1) );
      }

      EEPROM.commit();
      break;

    case 3:
      lojAlarmFlag = 1;
      lojAlarm.state |= ALARM_ENTRY;
      lojAlarm.charge = 50;
      lojAlarm.id = payload[1];
      break;
  }

  for (int i = 0; i < length; i ++) {
    mesaga[i] = payload[i];
  }

  netprint (mesaga);
}

void reconnect() {
  byte c = 0;

  while (!MQTTclient.connected() && (c < 2) ) {

    if (MQTTclient.connect("ESP8266Clien", mqttUser, mqttPass)) {
      netprint ("hello world");
      MQTTclient.subscribe("inTopic");
    } else {
      Serial.println("ёёёё");
      c++;
    }
  }
}

void netprint (const char* mesage) {

  if (MQTTclient.connected() && connectedWiFi()) {
    MQTTclient.publish("consoleOut", mesage);
  }
}


bool connectedWiFi() {

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  else {
    return true;
  }
}

void mqttLoop() {

  if (connectedWiFi()) {
    if (!MQTTclient.connected() && !lightOn) {
      
      if (millis() - reconT > 30000) {
        Serial.println("ёёё");
        reconnect();
        wif_init();
        reconT = millis();
      }
      
    } else {
      MQTTclient.loop();
    }
  } else {
    wif_init();
  }
}

void clearRXbuf() {
  
  if (Serial.available()) {
    size_t len = Serial.available();
    char sbuf[len];
    Serial.readBytes(sbuf, len);
  }
}

void ans () {


  if (Serial.available()) {
    otv = 1;
    size_t len = Serial.available();
    char sbuf[len];
    Serial.readBytes(sbuf, len);


    for (int i = 0; i < len; i++) {
      if (sbuf[i] == 0) sbuf[i] = ' ';
      if (sbuf[i] == '\n') sbuf[i] = ' ';
      if (sbuf[i] == '\r') sbuf[i] = ' ';
    }


    if (len > 20) {
      if (sbuf[20] == '1') {
        netFlag = 1;
      }
    }


    for (int i = 0; i < (len); i++) {
      if (sbuf [i] == '>') {
        pora = 1;
        break;
      }
    }


    if (len > 4) {
      for (int i = 0; i < (len - 2); i++) {
        if ((sbuf [i] == 'E') &&
            (sbuf [i + 1] == 'R') &&
            (sbuf [i + 2] == 'R')) {
          err = 1;
          break;
        }
      }
    }


    if (len > 1) {
      for (int i = 0; i < (len - 1); i++) {
        if (sbuf [i] == 'O' &&
            sbuf [i + 1] == 'K') ok = 1;
        break;
      }
    }
  }


}



void reqest (char req[400]) {
  Serial.print(req);
  delay(10);
}

