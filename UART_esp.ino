#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#ifndef STASSID
#define STASSID "INTELBRAS"
#define STAPSK "Pbl-Sistemas-Digitais"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
const char* host = "ESP-10.0.0.107";

int led_pin = LED_BUILTIN;
#define N_DIMMERS 3
int dimmer_pin[] = { 14, 5, 15 };

void setup() {
  Serial.begin(9600);

  /* switch on led */
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);

  Serial.println("Booting");
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("Retrying connection...");
  }
  /* switch off led */
  digitalWrite(led_pin, HIGH);

  /* configure dimmers, and OTA server events */
  analogWriteRange(1000);
  analogWrite(led_pin, 990);

  for (int i = 0; i < N_DIMMERS; i++) {
    pinMode(dimmer_pin[i], OUTPUT);
    analogWrite(dimmer_pin[i], 50);
  }

  ArduinoOTA.setHostname(host);
  ArduinoOTA.onStart([]() {  // switch off all the PWMs during upgrade
    for (int i = 0; i < N_DIMMERS; i++) {
      analogWrite(dimmer_pin[i], 0);
    }
    analogWrite(led_pin, 0);
  });

  ArduinoOTA.onEnd([]() {  // do a fancy thing with our board led at end
    for (int i = 0; i < 30; i++) {
      analogWrite(led_pin, (i * 100) % 1001);
      delay(50);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    (void)error;
    ESP.restart();
  });

  /* setup the OTA server */
  ArduinoOTA.begin();
  Serial.println("Ready");

  pinMode(D0, INPUT);
  pinMode(D1, INPUT);
}


#define NODE_ID 0x1
#define DESELECT_NODE 0x81

unsigned char recvd = '0';
bool selectedUnit = false;
int analogData;
unsigned char quocient;
unsigned char rest;

void loop() {
  ArduinoOTA.handle();
  if(Serial.available() > 0) {
    recvd = Serial.read();
    if(recvd == NODE_ID) {
      selectedUnit = true;
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.write(NODE_ID);
    }
    if(recvd == DESELECT_NODE) {
      selectedUnit = false;
      Serial.write(DESELECT_NODE);
      digitalWrite(LED_BUILTIN, LOW);
      delay(2000);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(2000);
    }
    if(selectedUnit && recvd != NODE_ID) {
      switch(recvd) {
        // Acender LED_BUILTIN
        case(0xC0):
            digitalWrite(LED_BUILTIN, LOW);
            break;
        // Consultar D0
        case(0xC3):
            Serial.write(digitalRead(D0));
            break;
        // Consultar D1
        case(0xC5):
            Serial.write(digitalRead(D1));
            break;
        case(0xC1):
            analogData = analogRead(A0);
            quocient = analogData / 10;
            rest = analogData % 10;
            Serial.write(quocient);
            delay(2);
            Serial.write(rest);
            break;
      }
    }
  }
}
