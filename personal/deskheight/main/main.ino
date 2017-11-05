// To install the Ultasonic library to work with a HC-SR04 ultrasonic depth sensor:
// Go to Sketch > Include Library > Manage Libraries...
// Then Install Ultrasonic by Erick Smoes (tested on v2.1.0)
// https://github.com/ErickSimoes/Ultrasonic
#include <Ultrasonic.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// --------------------------------------------------------------
// Read SECRET_WIFI_SSID and SECRET_WIFI_PASSWORD from secrets.h
// That file should look like:
// #define SECRET_WIFI_SSID "foo";
// #define SECRET_WIFI_PASSWORD "bar"
// #define SECRET_HTTP_HOST "10.0.0.1"
// #define SECRET_HTTP_PORT "80"
// Note that secrets.h is added to the .gitignore file, so that we
// don't accidentally upload sensitive data. Hence, you'll need to
// recreate the secrets.h file.
#include "secrets.h"

// --------------------------------------------------------------
// CONFIGURATION
// --------------------------------------------------------------
// WIFI
const char* WIFI_SSID              = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD          = SECRET_WIFI_PASSWORD;
const int   WIFI_MAX_WAIT_ATTEMPTS = 20;
const int   WIFI_BACKOFF_MSEC      = 5000;
const int   WIFI_STATUS_LED_PIN    = D6;  // LED that is used to convey wifi connectivity status (set to -1 to skip);
// HTTP
String      HTTP_HOST              = SECRET_HTTP_HOST;
const int   HTTP_PORT              = SECRET_HTTP_PORT;
String      HTTP_URI               = "stats/deskheight";

// MISC
const int   LOOP_INTERVAL_MSEC     = 5000; // How often do we measure desk height
const int   LED_STATUS_PIN         = D7;  // LED pin that is used to blink on success
// --------------------------------------------------------------

// With Wemos D1, use the provided constants, like D3 (and not 3) because the actual numbers are different
// Wire D3 up to Trig, D4 up to Ping on the HC-SR04 module
Ultrasonic ultrasonic(D3, D4);

// --------------------------------------------------------------
// WIFI Convenience functions

void ensureWifiConnection(){
  if (WiFi.status() == WL_CONNECTED){
    return;
  }
  Serial.println("Wifi connection lost. Reconnecting...");
  connectToWifi();
}

void connectToWifi(){
  if (WIFI_STATUS_LED_PIN > 0){
    digitalWrite(WIFI_STATUS_LED_PIN, LOW);
  }
  Serial.printf("Connecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  int lastStatusLEDState = LOW;

  while (WiFi.status() != WL_CONNECTED) {
    // If we have a status LED, then blink it
    if (WIFI_STATUS_LED_PIN > 0){
      if (lastStatusLEDState == LOW){
          digitalWrite(WIFI_STATUS_LED_PIN, HIGH);
          lastStatusLEDState = HIGH;
      } else {
          digitalWrite(WIFI_STATUS_LED_PIN, LOW);
          lastStatusLEDState = LOW;
      }
    }

    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts == WIFI_MAX_WAIT_ATTEMPTS) {
        Serial.println();
        Serial.printf("Wifi not connected after %d attempts.\n", attempts);
        Serial.printf("Backing off for %d msec...\n", WIFI_BACKOFF_MSEC);

        if (WIFI_STATUS_LED_PIN > 0){
          digitalWrite(WIFI_STATUS_LED_PIN, LOW);
        }
        delay(WIFI_BACKOFF_MSEC);
        Serial.println("Let's try this again...");
        attempts = 0;
    }
  }

  if (WIFI_STATUS_LED_PIN > 0){
    digitalWrite(WIFI_STATUS_LED_PIN, HIGH);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// --------------------------------------------------------------
// The actual party starts here

void setup(){
  Serial.begin(115200);
  Serial.println("Starting...");
  pinMode(WIFI_STATUS_LED_PIN, OUTPUT);
  pinMode(LED_STATUS_PIN, OUTPUT);
  connectToWifi();
}


void loop(){
    ensureWifiConnection();

    const int deskheight = ultrasonic.distanceRead();
    Serial.printf("Desk height: %d CM\n", deskheight);

    Serial.printf("Sending desk height of '%d' to %s:%i/%s\n", deskheight, HTTP_HOST.c_str(), HTTP_PORT, HTTP_URI.c_str());

    // Create http client
    HTTPClient http;
    http.begin(HTTP_HOST, HTTP_PORT, HTTP_URI);
    http.addHeader("User-Agent", "Desk height sensor - Wemos D1");

    int httpCode = http.POST(String(deskheight));

    // httpCode will be negative on error
    if(httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] POST... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println(payload);
        }
    } else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();

    Serial.printf("Next loop in %d msec", LOOP_INTERVAL_MSEC);

    // Blink success led
    digitalWrite(LED_STATUS_PIN, HIGH);
    delay(500);
    digitalWrite(LED_STATUS_PIN, LOW);

    delay(LOOP_INTERVAL_MSEC);

}

