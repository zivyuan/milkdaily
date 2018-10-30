#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

// #define ENV_DEVELOPMENT

#define API_BASE     String("http://gemdesign.cn/milk/dailylog.php")
// 心跳频率 10 分钟
#define HEART_RATE   600000

#define LED          2
#define DOOR_TOP     16
#define DOOR_BOTTOM  14

#define OPEN       0
#define CLOSE      1


void debug(String query) {
  HTTPClient http;

  query.replace(" ", "%20");

  http.begin("http://192.168.31.243/arduino/?v=" + query);
  http.GET();
  http.end();
}

// void reportThingsSpeak(String query) {
//   query.replace(" ", "%20");
//
//   http.begin("https://api.thingspeak.com/update?api_key=5CPEV0RT47HZN8DS&" + query);
//   http.GET();
//   http.end();
// }

void reportLog(String event, String data = "") {
  HTTPClient http;

  // String api = String(API_BASE + "?e=");

  event.replace(" ", "%20");
  data.replace(" ", "%20");

  // api = String(api + event);
  // api = String(api + "&d=");
  // api = String(api + data);
  http.begin(API_BASE + "?e=" + event + "&d=" + data);
  http.GET();
  http.end();

}


// =============================================================================

ESP8266WiFiMulti WiFiMulti;

int ledOn = 0;

void setup(void){
  pinMode(LED, OUTPUT);

  pinMode(DOOR_TOP, INPUT);
  pinMode(DOOR_BOTTOM, INPUT);

  digitalWrite(LED, LOW);

  Serial.begin(115200);

  while(!Serial) {
    ;
  }

  Serial.print("connecting ");

  WiFiMulti.addAP("Gozi2016", "pass4share");

  // Wait for connection
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    delay(500);
    ledOn = ledOn == 0 ? 1 : 0;
    digitalWrite(LED, ledOn);
    Serial.print(".");
  }

  debug("WIFI connect success");
  debug(String(WiFi.localIP()));

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println( String(WiFi.localIP()) );

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  reportLog("Milk Watcher Ready");
}

int topAct = OPEN;  // Top sensor default state
int topActKeep = 0;
int botAct = CLOSE; // Bottom sensor default state
unsigned long  botSensitivity = 700;
unsigned long botActTime = 0;
unsigned long shittingTime = 0;       // Milk state. 0, out; 1, push door in; 2, in toilet; 3, push door out
int ledCount = 1000;
int door = CLOSE;
int milkShitting = 0;
unsigned long heartRate = millis();

void loop(void){

  if (milkShitting == 1) {
    if (ledCount == 0) {
      if (ledOn) {
        digitalWrite(LED, LOW);
        ledOn = 0;

      } else {
        digitalWrite(LED, HIGH);
        ledOn = 1;
      }
      ledCount = 200;

    } else {
      ledCount --;
    }
  }

  int top = digitalRead(DOOR_TOP);
  if (top != topAct) {
    if (top == 1 && topActKeep != 1 && door == OPEN) {
      // debug("open direction in");
      topActKeep = 1;
    }
    topAct = top;
  }

  int bot = digitalRead(DOOR_BOTTOM);
  if (bot != botAct) {
    if (bot == 0 && door == CLOSE) {
      // 门运动开始
      door = OPEN;
      botAct = bot;
      // debug("door open");
      reportLog("Door Open", "");
      // digitalWrite(LED, HIGH);

    } else if (bot == 1 && door == OPEN) {
      botActTime = millis();
    }

    botAct = bot;

  } else {
    if (bot == 1 && door == OPEN) {
      // debug("door swing...");
      // 门回落
      unsigned long tt = millis();
      unsigned long distance = tt - botActTime;
      if (distance > botSensitivity) {
        door = CLOSE;
        // debug("door fall back");
        reportLog("Door Close", "");
        // digitalWrite(LED, LOW);

        if (milkShitting == 1) {
          milkShitting = 0;
          topActKeep = 0;
          shittingTime = millis() - shittingTime;
          shittingTime = shittingTime / 1000;
          digitalWrite(LED, HIGH);
          // debug("shitting end!");
          reportLog("Shit End", String(shittingTime));
          shittingTime = 0;
        }

        if (topActKeep == 1) {
          // Milk shitting
          milkShitting = 1;
          topActKeep = 0;
          shittingTime = millis();
          // debug("ready shitting...");
          reportLog("Shit Start", "");
        }

      }
    }
  }

  // Serial.print('.');

  unsigned long hd = millis() - heartRate;
  if (hd > HEART_RATE) {
    heartRate = millis();
    reportLog("HEART RATE");
  }

  delay(1);
}
