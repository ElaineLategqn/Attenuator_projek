#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define LatchEnable 32
#define C16 33
#define C8 25
#define C4 26
#define C2 27
#define C1 14
#define C0_5 12
#define C0_25 13

const int values[] = {64, 32, 16, 8, 4, 2, 1};
const int pins[] = {C16, C8, C4, C2, C1, C0_5, C0_25};
const int numberOfPins = 7;

int pinValues[numberOfPins];

float currentValue = 0.0;

const char* wifiName = "ESP32-Control";
const char* wifiPassword = "12345678";

WebServer server(80);

// Sets all ESP32 pins to output mode and starts them LOW
void setupPins()
{
  pinMode(LatchEnable, OUTPUT);
  digitalWrite(LatchEnable, LOW);

  for (int i = 0; i < numberOfPins; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
    pinValues[i] = 0;
  }
}

// Checks whether the user value is allowed
bool isValidInput(float input)
{
  int scaledInput = round(input * 4);

  if (scaledInput < 0 || scaledInput > 127) {
    return false;
  }

  if (abs(input * 4 - scaledInput) > 0.001) {
    return false;
  }

  return true;
}

// Converts the user's number into binary-style pin outputs
void calculatePinValues(float input)
{
  int scaledInput = round(input * 4);

  for (int j = 0; j < numberOfPins; j++) {
    if (scaledInput - values[j] >= 0) {
      pinValues[j] = 1;
      scaledInput = scaledInput - values[j];
    } else {
      pinValues[j] = 0;
    }
  }
}

// Writes the calculated pin values to the physical ESP32 pins
void writePins()
{
  for (int k = 0; k < numberOfPins; k++) {
    digitalWrite(pins[k], pinValues[k]);
  }
}

// Main function that controls the output from a user value
bool setOutputFromInput(float input)
{
  if (!isValidInput(input)) {
    Serial.println("Invalid input: value must be between 0 and 31.75 in steps of 0.25");
    return false;
  }

  currentValue = input;

  calculatePinValues(input);
  writePins();

  Serial.print("Output set to: ");
  Serial.println(input, 2);

  return true;
}

// Builds and sends the main web page
void handleRootPage()
{
  String page = "";

  page += "<!DOCTYPE html>";
  page += "<html>";
  page += "<head>";
  page += "<title>ESP32 Control</title>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";

  page += "<style>";
  page += "body {";
  page += "font-family: Arial, sans-serif;";
  page += "background-color: #f2f2f2;";
  page += "text-align: center;";
  page += "padding-top: 40px;";
  page += "}";

  page += ".card {";
  page += "background-color: white;";
  page += "max-width: 360px;";
  page += "margin: auto;";
  page += "padding: 25px;";
  page += "border-radius: 12px;";
  page += "box-shadow: 0 0 10px rgba(0,0,0,0.15);";
  page += "}";

  page += "h1 {";
  page += "font-size: 24px;";
  page += "}";

  page += "input {";
  page += "font-size: 22px;";
  page += "padding: 10px;";
  page += "width: 180px;";
  page += "text-align: center;";
  page += "}";

  page += "button {";
  page += "font-size: 20px;";
  page += "padding: 12px 24px;";
  page += "margin-top: 20px;";
  page += "border: none;";
  page += "border-radius: 8px;";
  page += "background-color: #2b7cff;";
  page += "color: white;";
  page += "}";

  page += ".small {";
  page += "font-size: 14px;";
  page += "color: #666;";
  page += "}";

  page += "</style>";
  page += "</head>";

  page += "<body>";
  page += "<div class='card'>";
  page += "<h1>ESP32 Control</h1>";

  page += "<p>Current value:</p>";
  page += "<h2>";
  page += String(currentValue, 2);
  page += "</h2>";

  page += "<form action='/set' method='GET'>";
  page += "<input type='number' name='value' min='0' max='31.75' step='0.25' required>";
  page += "<br>";
  page += "<button type='submit'>Set Output</button>";
  page += "</form>";

  page += "<p class='small'>Enter a value from 0 to 31.75 in steps of 0.25.</p>";

  page += "</div>";
  page += "</body>";
  page += "</html>";

  server.send(200, "text/html", page);
}

// Handles the value submitted from the webpage
void handleSetValue()
{
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "Missing value");
    return;
  }

  float input = server.arg("value").toFloat();

  bool success = setOutputFromInput(input);

  String page = "";

  page += "<!DOCTYPE html>";
  page += "<html>";
  page += "<head>";
  page += "<title>ESP32 Control</title>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";

  page += "<style>";
  page += "body {";
  page += "font-family: Arial, sans-serif;";
  page += "background-color: #f2f2f2;";
  page += "text-align: center;";
  page += "padding-top: 40px;";
  page += "}";

  page += ".card {";
  page += "background-color: white;";
  page += "max-width: 360px;";
  page += "margin: auto;";
  page += "padding: 25px;";
  page += "border-radius: 12px;";
  page += "box-shadow: 0 0 10px rgba(0,0,0,0.15);";
  page += "}";

  page += "a {";
  page += "display: inline-block;";
  page += "margin-top: 20px;";
  page += "font-size: 18px;";
  page += "text-decoration: none;";
  page += "color: #2b7cff;";
  page += "}";

  page += "</style>";
  page += "</head>";

  page += "<body>";
  page += "<div class='card'>";

  if (success) {
    page += "<h1>Output Updated</h1>";
    page += "<p>Value set to:</p>";
    page += "<h2>";
    page += String(input, 2);
    page += "</h2>";
  } else {
    page += "<h1>Invalid Value</h1>";
    page += "<p>Please enter a value from 0 to 31.75 in steps of 0.25.</p>";
  }

  page += "<a href='/'>Back</a>";

  page += "</div>";
  page += "</body>";
  page += "</html>";

  if (success) {
    server.send(200, "text/html", page);
  } else {
    server.send(400, "text/html", page);
  }
}

// Starts the ESP32's own Wi-Fi network
void setupWifiAccessPoint()
{
  WiFi.softAP(wifiName, wifiPassword);

  Serial.println();
  Serial.println("Wi-Fi access point started.");
  Serial.print("Wi-Fi name: ");
  Serial.println(wifiName);
  Serial.print("Wi-Fi password: ");
  Serial.println(wifiPassword);
  Serial.print("Open this address on your phone: ");
  Serial.println(WiFi.softAPIP());
}

// Connects webpage addresses to functions
void setupWebServer()
{
  server.on("/", handleRootPage);
  server.on("/set", handleSetValue);

  server.begin();

  Serial.println("Web server started.");
}

void handleSerialInput()
{
  if (Serial.available() > 0) {
    String inputString = Serial.readStringUntil('\n');
    inputString.trim();

    if (inputString.length() == 0) {
      return;
    }

    float input = inputString.toFloat();

    bool success = setOutputFromInput(input);

    if (success) {
      Serial.println("Serial command accepted.");
    } else {
      Serial.println("Serial command rejected.");
    }
  }
}

void setup()
{
  Serial.begin(921600);

  Serial.println("ESP32 starting...");

  setupPins();
  setupWifiAccessPoint();
  setupWebServer();

  Serial.println("Ready.");
}

void loop()
{
  handleSerialInput();
  server.handleClient();
}