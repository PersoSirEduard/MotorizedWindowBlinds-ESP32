#include <Wire.h>
#include <Adafruit_INA219.h> // Current sensor module
#include <IRremote.h> // Infrared library
#include <WiFi.h> // WiFi library for ESP32
#include <WebServer.h>

#define IR_PIN 15 // Infrared pin
// Directional motor control
#define MOTOR_DIR_UP 16
#define MOTOR_DIR_DOWN 17

// WiFi credentials
#define WIFI_SSID "ssid"
#define WIFI_PASS "password"

const int numReadings = 10; // Iterations for a measurement of current
const int globalAvgNumReadings = 20; // Iterations for average measurement of current
const float currentThreshold = 230; // in mA

float measurements[globalAvgNumReadings];
float currentMeasurement = 0;
float avgCurrent = 0;

Adafruit_INA219 currentSensor;
IRrecv irReceiver(IR_PIN);
decode_results ir_results;
WebServer server(80);

void wifiSetup() {
  WiFi.mode(WIFI_STA);
  Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) { // Connect to wifi
    Serial.print(".");
    delay(100);
  }
  
  WiFi.setSleep(false); // Keep Wifi awake for webserver to work!

  Serial.println();
  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

  // Webserver routes
  server.on("/", handle_OnConnect);
  server.on("/moveup", handle_moveup);
  server.on("/movedown", handle_movedown);
  server.on("/stop", handle_stop);
  server.onNotFound(handle_NotFound);

  server.begin();
}

void setup() {
  Serial.begin(115200); // Open serial port

  pinMode(IR_PIN, INPUT);
  pinMode(MOTOR_DIR_UP, OUTPUT);
  pinMode(MOTOR_DIR_DOWN, OUTPUT);

  if (!currentSensor.begin()) { // Initialize current sensor
    Serial.println("Failed to find INA219 chip");
    while (1) {
      delay(10);
    }
  }
  
  for (int i = 0; i < globalAvgNumReadings; i++) {
    measurements[i] = 0;
  }

  irReceiver.enableIRIn(); // Initialize IR receiver
  irReceiver.blink13(true);

  wifiSetup(); // Connect to WiFi
}

void loop() {
  server.handleClient();
  
  if (irReceiver.decode(&ir_results)) { // IR receiver event
    switch (ir_results.value) {
      case 0xFF18E7: // Up arrow -> Turn up motor
        digitalWrite(MOTOR_DIR_UP, HIGH);
        digitalWrite(MOTOR_DIR_DOWN, LOW);
        break;
      case 0xFF4AB5: // Down arrow -> Turn down motor
        digitalWrite(MOTOR_DIR_DOWN, HIGH);
        digitalWrite(MOTOR_DIR_UP, LOW);
        break;
      case 0xFF38C7: // OK button
        digitalWrite(MOTOR_DIR_DOWN, LOW);
        digitalWrite(MOTOR_DIR_UP, LOW);
        break;
      case 0xFF6897: // Star -> Calibration
        
        break;
      case 0xFFFFFFFF: // Button is kept pushed

        break;
    }
    irReceiver.resume();
  }

  if (digitalRead(MOTOR_DIR_UP) == HIGH || digitalRead(MOTOR_DIR_DOWN) == HIGH) { // If motor is running then measure current
    // Current measurement needs to be reworked
    float measuredCurrent = measureSmoothCurrent(); // Measure current at an instance
    int i = currentMeasurement;
    measurements[i] = measuredCurrent;
    avgCurrent = getAvgCurrent(); // Measure average current

    if (avgCurrent >= currentThreshold && currentMeasurement == globalAvgNumReadings - 1) { // Current threshold event
      digitalWrite(MOTOR_DIR_DOWN, LOW);
      digitalWrite(MOTOR_DIR_UP, LOW);
    }

    currentMeasurement++;
    if (currentMeasurement >= globalAvgNumReadings) {
      currentMeasurement = 0;  // Reset measurement counter
    }
  }
}

float measureSmoothCurrent() { // Smooth measured current
  float measuredCurrent = 0;
  for (int i = 0; i < numReadings; i++) {
    measuredCurrent += currentSensor.getCurrent_mA();
    delay(1);
  }
  measuredCurrent /= numReadings;
  return measuredCurrent;
}

float getAvgCurrent() { // Compute average current
  avgCurrent = 0;
  for (int i = 0; i < globalAvgNumReadings; i++) {
    avgCurrent += measurements[i];
  }
  avgCurrent /= globalAvgNumReadings;
  return avgCurrent;
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML()); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

void handle_moveup() {
  digitalWrite(MOTOR_DIR_UP, HIGH);
  digitalWrite(MOTOR_DIR_DOWN, LOW);
  server.send(200, "text/html", SendHTML()); 
}

void handle_movedown() {
  digitalWrite(MOTOR_DIR_UP, LOW);
  digitalWrite(MOTOR_DIR_DOWN, HIGH);
  server.send(200, "text/html", SendHTML()); 
}

void handle_stop() {
  digitalWrite(MOTOR_DIR_UP, LOW);
  digitalWrite(MOTOR_DIR_DOWN, LOW);
  server.send(200, "text/html", SendHTML()); 
}

// Following code based from https://randomnerdtutorials.com/esp32-web-server-arduino-ide/

String SendHTML(){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Blinds</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>Blinds</h1>\n";

  if (digitalRead(MOTOR_DIR_UP) == HIGH && digitalRead(MOTOR_DIR_DOWN) == LOW) {
    ptr +="<p>Current Status: Turning Up</p><a class=\"button button-on\" href=\"/moveup\">UP</a>\n";
    ptr +="<a class=\"button button-off\" href=\"/stop\">STOP</a>\n";
    ptr +="<a class=\"button button-off\" href=\"/movedown\">DOWN</a>\n";
  } else if (digitalRead(MOTOR_DIR_UP) == LOW && digitalRead(MOTOR_DIR_DOWN) == HIGH) {
    ptr +="<p>Current Status: Turning Down</p><a class=\"button button-off\" href=\"/moveup\">UP</a>\n";
    ptr +="<a class=\"button button-off\" href=\"/stop\">STOP</a>\n";
    ptr +="<a class=\"button button-on\" href=\"/movedown\">DOWN</a>\n";
  } else {
    ptr +="<p>Current Status: Stopped</p><a class=\"button button-off\" href=\"/moveup\">UP</a>\n";
    ptr +="<a class=\"button button-on\" href=\"/stop\">STOP</a>\n";
    ptr +="<a class=\"button button-off\" href=\"/movedown\">DOWN</a>\n";
  }

  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}
