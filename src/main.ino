#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

#define SENSOR_HUMAN 2
#define SENSOR_LED   15
#define SENSON_TEMPERATURE 4

// Human active continue event gap mark
#define ACTIVE_CONTINUE_GAP 10000

ESP8266WiFiMulti WiFiMulti;

const int led = 13;

int blink_state = 0;

void setup(void){
  pinMode(led, OUTPUT);
  pinMode(SENSOR_HUMAN, INPUT);
  pinMode(SENSOR_LED, OUTPUT);

  digitalWrite(led, LOW);
  Serial.begin(115200);
  while (!Serial);
  
  Serial.print("connecting ");

  WiFiMulti.addAP("gem", "6.Z:)Kjb4yvc");
  WiFiMulti.addAP("Gozi2016", "pass4share");

  // Wait for connection
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    delay(500);
    blink_state = blink_state == 0 ? 1 : 0;
    digitalWrite(led, blink_state);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println( WiFi.localIP() );

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  delay(200);

  logStatus("Milk Watcher Start");
}

int detector_active = 0;   // 传感器状态
int living_active = 0;     // 生物活动状态
int living_deactive_measure = 0;

void loop(void){

  int _ht = digitalRead(SENSOR_HUMAN);

  if (_ht == 0 && detector_active == 1) {
    detector_active = 0;
    // Serial.println("Deactive: " + String(millis()));
    logStatus("Deactive: " + String(millis()));
    digitalWrite(SENSOR_LED, LOW);
  } else if (_ht == 1 && detector_active == 0) {
    detector_active = 1;
    // Serial.println("__Active: " + String(millis()));
    logStatus("__Active: " + String(millis()));
    digitalWrite(SENSOR_LED, HIGH);
  }

  if (detector_active == 1 && living_active == 0) {
    living_active = 1;
    living_deactive_measure = 0;
    // Log living beings status as Active
    logStatus(":: __Active");

  } else if (detector_active == 1 && living_active == 1) {
    if (living_deactive_measure > 0) {
      living_deactive_measure = 0;
    }

  } else if (detector_active == 0 && living_active == 1) {
    // Living beings' active may stoped
    if (living_deactive_measure == 0) {
      living_deactive_measure = millis();
    } else {
      int _time = millis();
      if ((_time - living_deactive_measure) > ACTIVE_CONTINUE_GAP) {
        living_active = 0;
        living_deactive_measure = 0;
        // Log living beings status as Deactive
        logStatus(":: Deactive");
      }
    }

  } else {
    // detector_active == 0 && living_active == 0
    // No action needed
  }

}

// ======
void logStatus(String status) {
  HTTPClient http;

  status.replace(" ", "%20");
  http.begin("http://www.gemdesign.cn/arduino/log.php?mark=" + status);
  http.GET();
  http.end();
}
