#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

// Development config
// 本地网络调试
// #define LOCAL_DEBUG   1
// 是否记录日志
// #define NO_REPORT     1


#define API_BASE     String("http://gemdesign.cn/milk/dailylog.php")
#define AP_NAME       "Gozi2016"
#define PASSWORD      "pass4share"
// 心跳频率 10 分钟
#define HEART_RATE   600000

#define LED          2
#define DOOR_TOP     16
#define DOOR_BOTTOM  14

#define CLOSED        1
#define OPEN          0

#define ATTACHED       1
#define DETACHED       0

// 门磁感激活超过指定时间后判定为门完全关闭. Default value: 1000ms
#define FLAG_DOOR_CLOSE_TIMER    1000
// 顶部磁感激活超过指定时间后判定为维护状态. Default value: 18000ms
#define FLAG_TOP_ATTACH_TIMER    18000
// 使用超时. 兼容设置, 如果一次使用时间超过5分钟则自动标记为停止使用
#define FLAG_USING_OVERTIME      300000


#define IDLE            0
#define USING           1
#define MAINTAIN        2
#define UNCERTAIN       3

#define UNLOCK          0
#define LOCK            1


#ifndef LOCAL_DEBUG

#define debug(message)    // http_debug(message)  // Code ignored

#else

void http_debug(String query) {
  HTTPClient http;

  query.replace(" ", "%20");

  http.begin("http://192.168.31.243/arduino/?v=" + query);
  http.GET();
  http.end();
}

#define debug(message)    http_debug(message)

#endif

// =============================================================================

ESP8266WiFiMulti WiFiMulti;

int ledState = LOW;
unsigned long ledTimer = 0;
String ID = "";
int topAct = DETACHED;  // Top sensor default state
int botAct = CLOSED; // Bottom sensor default state
unsigned long botCloseTimer = 0;
unsigned long topAttachTimer = 0;
int toiletState = IDLE;
int toiletUsingLock = UNLOCK;
unsigned long toiletUsingStart = 0;
int doorState = CLOSED;

unsigned long heartRate = millis();


/**
 * Convert IP from int
 */
String ipFromInt(unsigned int ip) {
    unsigned int a = int( ip / 16777216 );
    unsigned int b = int( (ip % 16777216) / 65536 );
    unsigned int c = int( ((ip % 16777216) % 65536) / 256 );
    unsigned int d = int( ((ip % 16777216) % 65536) % 256 );
    String ipstr = String(d) + '.' + String(c) + '.' + String(b) + '.' + String(a);

    return ipstr;
}

// Data report handle
void report(String event, String data = "") {
#ifndef NO_REPORT

  HTTPClient http;

  event.replace(" ", "%20");
  data.replace(" ", "%20");

  String request = API_BASE + "?id=" + ID + "&e=" + event + "&d=" + data;

  http.begin(request);
  http.GET();
  http.end();

#endif
}

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

  WiFiMulti.addAP(AP_NAME, PASSWORD);

  // Wait for connection
  int ledOn = 0;
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    delay(500);
    ledOn = ledOn == 0 ? 1 : 0;
    digitalWrite(LED, ledOn);
    Serial.print(".");
  }

  ID = WiFi.macAddress();
  debug(" ");
  debug("WIFI connect success");
  debug(ipFromInt(WiFi.localIP()));
  debug(ID);

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println( ipFromInt(WiFi.localIP()) );
  //
  // if (MDNS.begin("esp8266")) {
  //   Serial.println("MDNS responder started");
  // }

  report("Watch dog " + ID + " report!");

  topAct = digitalRead(DOOR_TOP);
  botAct = digitalRead(DOOR_BOTTOM);
  doorState = botAct == OPEN ? OPEN : CLOSED;
}

void loop(void){
  int top = digitalRead(DOOR_TOP);
  int bot = digitalRead(DOOR_BOTTOM);


  unsigned long elapse;

  // !! Door state control block START !!
  if (bot != botAct) {
    debug("B: " + String(bot));

    if (bot == CLOSED) {
      botCloseTimer = millis();

    } else {
      botCloseTimer = 0;
      if (doorState == CLOSED) {
        doorState = OPEN;
        toiletUsingLock = UNLOCK;
        debug("Door open.");
        report("Door Open");
      }
    }

    botAct = bot;
  }

  if (botAct == CLOSED) {
    elapse = millis() - botCloseTimer;
    if (elapse > FLAG_DOOR_CLOSE_TIMER && doorState == OPEN) {
      doorState = CLOSED;
      debug("Door closed.");
      report("Door Closed");
    }
  }
  // !! Door state control block END !!

  if (top != topAct) {
    debug("T: " + String(top));
    topAct = top;
  }

  if (topAct == ATTACHED && doorState == OPEN) {
    if (toiletState == IDLE) {
      topAttachTimer = millis();
      toiletState = UNCERTAIN;
      debug("Some body go in toilet...");
    } else {
      elapse = millis() - topAttachTimer;
      if (elapse > FLAG_TOP_ATTACH_TIMER && toiletState == UNCERTAIN) {
        toiletState = MAINTAIN;
        toiletUsingStart = millis();
        debug("Toilet maintain start...");
        report("Maintain Start");
      }
    }
  } else if (topAct == DETACHED ) { // && doorState == OPEN
    if (toiletState == UNCERTAIN) {
      toiletState = USING;
      toiletUsingLock = LOCK;
      toiletUsingStart = millis();
      debug("Toilet in using...");
      report("Using Start");
    }
  }

  if (doorState == CLOSED && toiletState == USING) {
    if (toiletUsingLock == UNLOCK) {
      toiletState = IDLE;
      debug("Toilet using complete.");
      elapse = (millis() - toiletUsingStart) / 1000;
      report("Using Complete", String(elapse));
    // } else {
      // debug("Door close ignore by using.");
    } else {
      elapse = millis() - toiletUsingStart;
      if (elapse > FLAG_USING_OVERTIME) {
        toiletState = IDLE;
        debug("Toilet using complete with overtime.");
        elapse = (millis() - toiletUsingStart) / 1000;
        report("Using Complete", String(elapse) + ", Overtime");
      }

    }

  } else if (doorState == CLOSED && toiletState == MAINTAIN) {
    toiletState = IDLE;
    debug("Toilet maintain complete.");
    elapse = (millis() - toiletUsingStart) / 1000;
    report("Maintain Complete", String(elapse));
  }

  if (toiletState == UNCERTAIN) {
    elapse = millis() - ledTimer;
    if (elapse > 500) {
      ledTimer = millis();
      ledState = !ledState;
      digitalWrite(LED, ledState);
    }

  } else if (toiletState == USING) {
    elapse = millis() - ledTimer;
    if (elapse > 1000) {
      ledTimer = millis();
      ledState = !ledState;
      digitalWrite(LED, ledState);
    }

  } else if (toiletState == MAINTAIN) {
    if (ledState == HIGH) {
      ledState = LOW;
      digitalWrite(LED, ledState);
    }

  } else {
    if (doorState == OPEN) {
      elapse = millis() - ledTimer;
      if (elapse > 150) {
        ledTimer = millis();
        ledState = !ledState;
        digitalWrite(LED, ledState);
      }

    } else if (ledState == LOW) {
      ledState = HIGH;
      digitalWrite(LED, HIGH);
    }
  }

  unsigned long hd = millis() - heartRate;
  if (hd > HEART_RATE) {
      heartRate = millis();
      report("HEART RATE");
    }

  delay(1);
}
