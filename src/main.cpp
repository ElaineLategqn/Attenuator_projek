#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LiquidCrystal.h>

#define LatchEnable 32
#define C16 33
#define C8 25
#define C4 26
#define C2 27
#define C1 14
#define C0_5 12
#define C0_25 13

// Keypad column pins
#define KeypadCol1 15
#define KeypadCol2 2
#define KeypadCol3 0
#define KeypadCol4 21

// Keypad row pins
#define KeypadRow1 35
#define KeypadRow2 34
#define KeypadRow3 39
#define KeypadRow4 36

// LCD pins
#define LcdRS 5
#define LcdE 18
#define LcdD4 19
#define LcdD5 4
#define LcdD6 22
#define LcdD7 23

LiquidCrystal lcd(LcdRS, LcdE, LcdD4, LcdD5, LcdD6, LcdD7);

const int lcdCols = 16;
const int lcdRows = 2;

const int values[] = {64, 32, 16, 8, 4, 2, 1};
const int pins[] = {C16, C8, C4, C2, C1, C0_5, C0_25};
const int numberOfPins = 7;

int pinValues[numberOfPins];

float currentValue = 0.0;

const char* wifiName = "ESP32-Control";
const char* wifiPassword = "12345678";

WebServer server(80);

// Keypad setup
const int keypadCols[] = {KeypadCol1, KeypadCol2, KeypadCol3, KeypadCol4};
const int keypadRows[] = {KeypadRow1, KeypadRow2, KeypadRow3, KeypadRow4};

const int numberOfKeypadCols = 4;
const int numberOfKeypadRows = 4;

const char keypadMap[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

String keypadInput = "";

char lastKey = '\0';
unsigned long lastKeyTime = 0;
const unsigned long debounceDelay = 250;

// Pulses latch enable so the attenuator accepts the new pin values
void latchOutput()
{
  delayMicroseconds(1);
  digitalWrite(LatchEnable, HIGH);
  delayMicroseconds(1);
  digitalWrite(LatchEnable, LOW);
  delayMicroseconds(1);
}

// Sets all ESP32 attenuator pins to output mode and starts them LOW
void setupPins()
{
  pinMode(LatchEnable, OUTPUT);
  digitalWrite(LatchEnable, LOW);

  for (int i = 0; i < numberOfPins; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
    pinValues[i] = 0;
  }

  latchOutput();
}

// Sets up the matrix keypad pins
void setupKeypad()
{
  for (int c = 0; c < numberOfKeypadCols; c++) {
    pinMode(keypadCols[c], OUTPUT);
    digitalWrite(keypadCols[c], HIGH);
  }

  pinMode(KeypadRow1, INPUT);
  pinMode(KeypadRow2, INPUT);
  pinMode(KeypadRow3, INPUT);
  pinMode(KeypadRow4, INPUT);
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
  int scaledInput = round(input * 4) - 4;

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

String formatDbValue(float value)
{
  if (abs(value - round(value)) < 0.001) {
    return String((int)round(value)) + " dB";
  }

  return String(value, 2) + " dB";
}

void clearLcdRow(int row)
{
  lcd.setCursor(0, row);

  for (int i = 0; i < lcdCols; i++) {
    lcd.print(" ");
  }
}

void updateLCD()
{
  // Top-left: current dB value
  clearLcdRow(0);
  lcd.setCursor(0, 0);
  lcd.print(formatDbValue(currentValue));

  // Bottom-right: value currently being typed
  clearLcdRow(lcdRows - 1);

  String displayInput = keypadInput;
  displayInput.replace(".", ",");

  if (displayInput.length() > lcdCols) {
    displayInput = displayInput.substring(displayInput.length() - lcdCols);
  }

  int startCol = lcdCols - displayInput.length();

  lcd.setCursor(startCol, lcdRows - 1);
  lcd.print(displayInput);
}

void setupLCD()
{
  lcd.begin(lcdCols, lcdRows);
  lcd.clear();
  updateLCD();
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
  latchOutput();
  updateLCD();

  Serial.print("Output set to: ");
  Serial.println(input, 2);

  return true;
}

// Reads the currently pressed keypad key
char readKeypad()
{
  for (int c = 0; c < numberOfKeypadCols; c++) {
    // Set all columns HIGH first
    for (int i = 0; i < numberOfKeypadCols; i++) {
      digitalWrite(keypadCols[i], HIGH);
    }

    // Pull one column LOW
    digitalWrite(keypadCols[c], LOW);
    delayMicroseconds(5);

    // Check each row
    for (int r = 0; r < numberOfKeypadRows; r++) {
      if (digitalRead(keypadRows[r]) == LOW) {
        // Return column to HIGH before leaving
        digitalWrite(keypadCols[c], HIGH);
        return keypadMap[r][c];
      }
    }

    digitalWrite(keypadCols[c], HIGH);
  }

  return '\0';
}

// Handles keypad input
void handleKeypadInput()
{
  char key = readKeypad();

  if (key == '\0') {
    lastKey = '\0';
    return;
  }

  // Debounce
  if (key == lastKey && millis() - lastKeyTime < debounceDelay) {
    return;
  }

  lastKey = key;
  lastKeyTime = millis();

  // Ignore A, B, C, D
  if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
    return;
  }

  // Star acts as decimal comma/point
  if (key == '*') {
    if (keypadInput.indexOf('.') == -1) {
      if (keypadInput.length() == 0) {
        keypadInput += "0";
      }

      keypadInput += ".";
      updateLCD();

      Serial.print("Keypad input: ");
      Serial.println(keypadInput);
    }

    return;
  }

  // Hash acts as enter
  if (key == '#') {
    if (keypadInput.length() == 0 || keypadInput == ".") {
      Serial.println("No keypad value entered.");
      keypadInput = "";
      updateLCD();
      return;
    }

    float input = keypadInput.toFloat();

    Serial.print("Keypad submitted: ");
    Serial.println(input, 2);

    bool success = setOutputFromInput(input);

    if (success) {
      Serial.println("Keypad command accepted.");
    } else {
      Serial.println("Keypad command rejected.");
    }

    keypadInput = "";
    updateLCD();
    return;
  }

  // Number keys
  if (key >= '0' && key <= '9') {
    if (keypadInput.length() < 6) {
      keypadInput += key;
    }

    updateLCD();

    Serial.print("Keypad input: ");
    Serial.println(keypadInput);
    return;
  }
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
  setupKeypad();
  setupLCD();
  setupWifiAccessPoint();
  setupWebServer();

  Serial.println("Ready.");
}

void loop()
{
  handleSerialInput();
  handleKeypadInput();
  server.handleClient();
}