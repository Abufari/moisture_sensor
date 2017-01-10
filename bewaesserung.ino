#include <EEPROM.h>

#define MOISTURE_THRESHOLD_ON 600
#define MOISTURE_THRESHOLD_OFF 750
#define MAXIMUM_PUMP_COUNTS 5
#define PUMP_TIME (4UL * 1000) //sekunden
#define MINUTES_UNDER_THRESHOLD (5UL * 60 * 1000)
#define SECONDS_OVER_THRESHOLD (10UL * 1000)
#define DAYS_BETWEEN_PUMP (3UL * 24 * 3600 * 1000)
#define ONE_DAY (24UL * 3600 * 1000)
#define CHECK_MOISTURE_DURATION (30UL * 1000)


int TOO_MANY_PUMP_CYCLES_ERROR = 0;
int SHUT_OFF_FOR_ONE_DAY = 0;

const int moisturePin = A0;
const int moisturePowerPin = 8;
const int pumpPin = 7;
const int statusLED = 13;

int pumpState = LOW;
int moistureState = 700;
int lastMoistureState = 0;
int checkRate = 5UL * 1000;        ///// auf 3600 ändern!
int numberOfPumps = 0;
int currentlyPumping = 0;
unsigned long pumpStart;                // Startzeit der Pumpe
unsigned long lastDebounceTime = 0;
unsigned long lastPumpTime = 0;         // Endzeit des letzten gesamten Pumpvorgangs
unsigned long lastCheckTime = 0;
unsigned long shutOffTime = 0;          // Startzeit eintägiges Pumpverbot

// EEPROM Stuff
int addr = 0;


void setup() {
  // put your setup code here, to run once:
  pinMode(moisturePin, INPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(statusLED, OUTPUT);
  pinMode(moisturePowerPin, OUTPUT);

  digitalWrite(statusLED, pumpState);
  digitalWrite(pumpPin, LOW);
  digitalWrite(moisturePowerPin, LOW);


  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (SHUT_OFF_FOR_ONE_DAY) {
    if ((millis() - shutOffTime) > ONE_DAY) { // reset auf 24*3600*1000
      SHUT_OFF_FOR_ONE_DAY = 0;
    }
  }
  if ((pumpState == LOW) && ((millis() - lastCheckTime) > checkRate) &&
      !SHUT_OFF_FOR_ONE_DAY && !TOO_MANY_PUMP_CYCLES_ERROR) {
    lastCheckTime = millis();
    checkMoisture();
  }
  if (pumpState == HIGH) {
    pump();
  }
  if (currentlyPumping && (millis() - pumpStart) > 2 * PUMP_TIME) {
    turnPumpsOff();
    pumpState = LOW;
    TOO_MANY_PUMP_CYCLES_ERROR = 1;
  }
}

void checkMoisture() {
  digitalWrite(moisturePowerPin, HIGH);
  delay(1000);
  int moisture = analogRead(moisturePin);
  digitalWrite(moisturePowerPin, LOW);

  Serial.println(moisture);
  if (moisture < MOISTURE_THRESHOLD_ON && !lastMoistureState) {
    lastMoistureState = 1;
    return;
  }
  if (moisture < MOISTURE_THRESHOLD_ON) {
    moistureState = moisture;
    pumpState = HIGH;
  }
  lastMoistureState = 0;
}

void checkMoistureDuringPump() {
  digitalWrite(moisturePowerPin, HIGH);
  delay(1000);
  int i, arrayLength = 100;
  unsigned long moistureValues = 0;
  for (i = 0; i < arrayLength; ++i) {
    moistureValues += analogRead(moisturePin);
    delay(CHECK_MOISTURE_DURATION / arrayLength);
  }
  moistureState = moistureValues / arrayLength;
  Serial.println(moistureState);
  digitalWrite(moisturePowerPin, LOW);
}


void pump() {
  if (pumpState == LOW) return;
  if (numberOfPumps != 0 && (millis() - lastPumpTime) < DAYS_BETWEEN_PUMP) {
    SHUT_OFF_FOR_ONE_DAY = 1;
    shutOffTime = millis();
    pumpState = LOW;
    return;
  }

  static int pumpCounts = 0;
  if (moistureState < MOISTURE_THRESHOLD_OFF) { // Pumpen nötig
    if (pumpCounts > MAXIMUM_PUMP_COUNTS) {
      TOO_MANY_PUMP_CYCLES_ERROR = 1;
      return;
    }
    if (!currentlyPumping) {
      turnPumpsOn();
    }
    if ((millis() - pumpStart) > PUMP_TIME) {
      turnPumpsOff();
    }
    if (!currentlyPumping) {
      checkMoistureDuringPump();             // Feuchtigkeit überprüfen
      ++pumpCounts;
    }
  } else {                                       // gießen abgeschlossen
    lastPumpTime = millis();                   // Endzeit des Pumpvorgangs
    pumpCounts = 0;
    pumpState = LOW;
    ++numberOfPumps;
    //updateEEPROM();
  }
}

void turnPumpsOn() {
  digitalWrite(statusLED, HIGH);
  digitalWrite(pumpPin, HIGH);
  pumpStart = millis();
  currentlyPumping = 1;
}

void turnPumpsOff() {
  digitalWrite(statusLED, LOW);
  digitalWrite(pumpPin, LOW);
  currentlyPumping = 0;
}

void updateEEPROM() {
  EEPROM.update(addr, numberOfPumps);
}


