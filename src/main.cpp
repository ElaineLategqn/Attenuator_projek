#include <Arduino.h>
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

void setup() {
  pinMode(LatchEnable, OUTPUT);
  Serial.begin(921600);
  Serial.println("Enter a value between 0 and 31.75:");

  for (int i = 0; i < numberOfPins; i++){
    pinMode(pins[i], OUTPUT);
    pinValues[i] = 0;
  }

  digitalWrite(LatchEnable, LOW);
  for (int i = 0; i < numberOfPins; i++) {
  pinMode(pins[i], OUTPUT);
  digitalWrite(pins[i], LOW);
  pinValues[i] = 0;
}
}

void loop() {
  if (Serial.available() > 0) {
    String inputString = Serial.readStringUntil('\n');
    inputString.trim();

    float input = inputString.toFloat();

    int scaledInput = round(input * 4);

    if (scaledInput < 0 || scaledInput > 127) {
      Serial.println("Invalid input: value must be between 0 and 31.75");
      return;
    }

    if (abs(input * 4 - scaledInput) > 0.001) {
      Serial.println("Invalid input: value must be a multiple of 0.25");
      return;
    }

    for (int j = 0; j < numberOfPins; j++){
      if (scaledInput - values[j] >= 0){
        pinValues[j] = 1;
        scaledInput = scaledInput - values[j];
      } else
        pinValues[j] = 0;
    }

    for (int k = 0; k < numberOfPins; k++){
      digitalWrite(pins[k], pinValues[k]);
    }
  }  
}


