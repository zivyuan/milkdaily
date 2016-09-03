#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>

// #define ENV_DEVELOPMENT

#define SENSOR_HUMAN 2
#define SENSOR_LED   15

#define DHTPIN 0
#define DHTTYPE DHT22

// Human active continue event gap mark
#define ACTIVE_CONTINUE_GAP 10000

ESP8266WiFiMulti WiFiMulti;

const int led = 13;

int blink_state = 0;

void setup(void){
  pinMode(led, OUTPUT);
  pinMode(SENSOR_HUMAN, INPUT);
  pinMode(SENSOR_LED, OUTPUT);
  pinMode(DHTPIN, INPUT);

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
  Serial.println( String(WiFi.localIP()) );

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }


  logStatus("Milk Watcher Start");
}

void loop(void){

	handleToiletActive();

	// handleTemperature();

}

void reportToThingSpeak(String query) {
  HTTPClient http;

  http.begin("http://api.thingspeak.com/update?api_key=5CPEV0RT47HZN8DS&" + query);
  http.GET();
  http.end();
}


// ======
void logStatus(String status) {

  HTTPClient http;

  status.replace(" ", "%20");

  http.begin("http://www.gemdesign.cn/arduino/log.php?mark=" + status);
  http.GET();
  http.end();

}

void reportToiletActive() {
  reportToThingSpeak("field1=1");
}

void reportToiletDuration(int duration) {
  reportToThingSpeak("field2=" + String(duration / 1000));
}


//
//  == Toilet event handle
//
int detector_active = 0;   // 传感器状态
int living_active = 0;     // 生物活动状态
int living_deactive_measure = 0;
int living_active_time = 0;

void handleToiletActive() {

  int _ht = digitalRead(SENSOR_HUMAN);

  if (_ht == 0 && detector_active == 1) {
    detector_active = 0;
  } else if (_ht == 1 && detector_active == 0) {
    detector_active = 1;
  }

  if (detector_active == 1 && living_active == 0) {
    living_active = 1;
    living_deactive_measure = 0;
    living_active_time = millis();
    // Log living beings status as Active
    reportToiletActive();
    logStatus("+: __Active");

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
        reportToiletDuration(millis() - living_active_time);
        logStatus("+: Deactive");
      }
    }

  } else {
    // detector_active == 0 && living_active == 0
    // No action needed
  }
}

//
// == Toilet event handle END
//


//
// Temperature Detect
//
DHT dht(DHTPIN, DHTTYPE);
int dht_report_time = 0;
#define DHT_REPORT_INTERVAL    30000      // 温度上报间隔

void handleTemperature() {

	int interval = millis() - dht_report_time;
	if (interval < DHT_REPORT_INTERVAL)
	{
		return;
	}

  Serial.println("Meaure temperature and humidity...");
	float h = dht.readHumidity();
	float t = dht.readTemperature();

	if (isnan(h) || isnan(t))
	{
		Serial.println("Failed to read form DHT sensor!");
    logStatus("Temperature test Failed");
	} else {
		// humidity = h;
		// temperature = t;
		Serial.println("DHT Humidity: " + String(h));
		Serial.println("DHT Temperature: " + String(t));

    logStatus("H: " + String(h) + ", T: " + String(t));
		reportDHTData(h, t);
	}

	dht_report_time = millis();

}

void reportDHTData(int h, int t) {

	HTTPClient http;

	String query = "?api_key=5CPEV0RT47HZN8DS";
	query = "&" + query + "field4=" + String(h);
	query = "&" + query + "field3=" + String(t);

	http.begin("http://api.thingspeak.com/update" + query);
	http.GET();
	http.end();

}
