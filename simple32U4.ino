#include "Magellan.h"

Magellan magel;
char auth[] = "......";   //Token Key you can get from magellan platform

String payload;

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated
unsigned long currentMillis = millis();

// constants won't change:
const long interval = 60000;           // interval at which to blink (milliseconds)

byte ptk_send = 0;
byte ptk_success = 0;
byte ptk_lost = 0;

// Functions used in processing readings
void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  Serial1.begin(9600);
  delay(5000);

  magel.begin(auth, &Serial1);         //init Magellan LIB
}

void loop() {
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    /*
      Example send random temperature and humidity to Magellan IoT platform
    */
    payload = "{\"temperature\":25,\"humidity\":80}";  //please provide payload with json format

    ptk_send++;
    Serial.println("Send payload");
    if (magel.post(payload)) {                        //post payload data to Magellan IoT platform
      Serial.println("Send payload ok");
      ptk_success++;
    } else {
      Serial.println("Send payload fail");
      ptk_lost++;
    }

    Serial.print("Send:");
    Serial.print(ptk_send);
    Serial.print(" Success:");
    Serial.print(ptk_success);
    Serial.print(" Lost:");
    Serial.println(ptk_lost);
  }
  delay(1000);
}
