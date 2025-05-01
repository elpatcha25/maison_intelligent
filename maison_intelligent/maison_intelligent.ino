#include <HCSR04.h>
#include <AccelStepper.h>
#include <LCD_I2C.h>
#include <string.h>
#include <U8g2lib.h>
#include <LedControl.h>

#define CLK_PIN 2
#define DIN_PIN 3
#define CS_PIN 4

LedControl matrix = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 1);

#define LED_R 31
#define LED_G 33
#define LED_B 35
#define BUZZER_PIN 5

#define MOTOR_INTERFACE_TYPE 4
#define IN_1 8
#define IN_2 9
#define IN_3 10
#define IN_4 11

AccelStepper myStepper(MOTOR_INTERFACE_TYPE, IN_1, IN_3, IN_2, IN_4);
HCSR04 hc(6, 7);
LCD_I2C lcd(0x27, 20, 21);

const String NUM_ETUDIANT = "2407822";
const float tour = 2038.0;
const int MAX_DEGREE = 170;
const int MIN_DEGREE = 10;
const long POSITION_OUVERTE = (tour * MAX_DEGREE) / 360.0;
const long POSITION_FERMEE = (tour * MIN_DEGREE) / 360.0;

int limInf = 30;
int limSup = 60;
int alarmLimit = 15;

const int MAX_SPEED = 500;
const int ACCELERATION = 300;
bool doorInMovement = false;

const unsigned long TIME_ALARME_OFF = 3000;
const unsigned long DISTANCE_TIME = 50;
const unsigned long TEMPS_MIN_OUVERTE = 5000;

int deg = 0;
int dist = 0;
bool alarmState = false;
bool flashing = true;

unsigned long lastObjectDetected = 0;
unsigned long distanceTime = 0;
unsigned long lastFlashing = 0;
unsigned long flashingTime = 200;
unsigned long tempsOuverte = 0;
unsigned long currentTime = 0;
unsigned long currentDistance = 0;


byte logoVrai[8] = {
  B01100000,
  B00011000,
  B00001100,
  B00000110,
  B00000011,
  B00001100,
  B00010000,
  B00000000
};

byte logoStop[8] = {  
  B00111100,
  B01000010,
  B10000101,
  B10001001,
  B10010001,
  B10100001,
  B01000010,
  B00111100
};
byte logoCroix[8] = { 
  B10000001,
  B01000010,
  B00100100,
  B00011000,
  B00011000,
  B00100100,
  B01000010,
  B10000001
};


bool afficherSymboleActif = false;
unsigned long tempsAffichageSymbole = 0;
byte symboleActuel[8];

void setup() {
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  eteindreLED();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

  myStepper.setMaxSpeed(MAX_SPEED);
  myStepper.setAcceleration(ACCELERATION);
  myStepper.setCurrentPosition(0);

  myStepper.moveTo(POSITION_FERMEE);
  myStepper.runToPosition();
  myStepper.setCurrentPosition(POSITION_FERMEE);

  matrix.shutdown(0, false);
  matrix.setIntensity(0, 5);
  matrix.clearDisplay(0);

  afficherDemarrage();
  delay(2000);
  lcd.clear();
}

enum EtatPorte { FERMEE, OUVERTURE, OUVERTE, FERMETURE };
EtatPorte currentState = FERMEE;

void loop() {
  currentTime = millis();

  if (currentTime - distanceTime > DISTANCE_TIME) {
    distanceTime = currentTime;
    currentDistance = hc.dist();
    if (currentDistance > 0) dist = currentDistance;
  }

  gererAlarme();
  gestionEtat();
  afficherLCD();
  afficherSymboleSiActif();

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    bool cmdOK = false;
    bool erreurLimites = false;

    if (cmd.equalsIgnoreCase("gDist")) {
      Serial.println(dist);
      cmdOK = true;

    } else if (cmd.startsWith("cfg;alm;")) {

      int valeur = cmd.substring(8).toInt();

      if (valeur > 0) { alarmLimit = valeur; cmdOK = true; }

    } else if (cmd.startsWith("cfg;lim_inf;")) {
      int valeur = cmd.substring(12).toInt();
      if (valeur < limSup) { limInf = valeur; cmdOK = true; }
      else erreurLimites = true;

    } else if (cmd.startsWith("cfg;lim_sup;")) {
      int valeur = cmd.substring(12).toInt();
      if (valeur > limInf) { limSup = valeur; cmdOK = true; }
      else erreurLimites = true;
    }

    if (erreurLimites) lancerAffichageSymbole(logoStop);
    else if (!cmdOK) lancerAffichageSymbole(logoCroix);
    else lancerAffichageSymbole(logoVrai);
  }

  if (doorInMovement) {
    myStepper.enableOutputs();
    myStepper.run();
    if (!myStepper.isRunning()) {
      doorInMovement = false;
      myStepper.disableOutputs();
    }
  }
}

void lancerAffichageSymbole(byte symbole[8]) {
  for (int i = 0; i < 8; i++) {
    symboleActuel[i] = symbole[i];
    matrix.setRow(0, i, symbole[i]);
  }
  afficherSymboleActif = true;
  tempsAffichageSymbole = millis();
}

void afficherSymboleSiActif() {
  if (afficherSymboleActif && (millis() - tempsAffichageSymbole >= 3000)) {
    matrix.clearDisplay(0);
    afficherSymboleActif = false;
  }
}

void gestionEtat() {
  if (alarmState) return;
  switch (currentState) {
    case FERMEE:
      if (dist < limInf) {
        currentState = OUVERTURE;
        myStepper.moveTo(POSITION_OUVERTE);
        doorInMovement = true;
      }
      break;
    case OUVERTURE:
      if (!doorInMovement && myStepper.distanceToGo() == 0) {
        currentState = OUVERTE;
        tempsOuverte = currentTime;
      }
      break;
    case OUVERTE:
      setLED(LOW, HIGH, LOW);
      if (dist > limSup && currentTime - tempsOuverte >= TEMPS_MIN_OUVERTE) {
        currentState = FERMETURE;
        myStepper.moveTo(POSITION_FERMEE);
        doorInMovement = true;
      }
      break;
    case FERMETURE:
      if (!doorInMovement && myStepper.distanceToGo() == 0) {
        currentState = FERMEE;
      }
      break;
  }
}

void gererAlarme() {
  if (dist < alarmLimit) {
    lastObjectDetected = currentTime;
    if (!alarmState) {
      alarmState = true;
      digitalWrite(BUZZER_PIN, LOW);
    }
    if (currentTime - lastFlashing >= flashingTime) {
      clignoterGyrophare();
      lastFlashing = currentTime;
    }
  } else if (alarmState && currentTime - lastObjectDetected > TIME_ALARME_OFF) {
    alarmState = false;
    digitalWrite(BUZZER_PIN, HIGH);
    eteindreLED();
  }
}

void clignoterGyrophare() {
  if (flashing) {
    setLED(HIGH, LOW, LOW);
  } else {
    setLED(LOW, LOW, HIGH);
  }
  flashing = !flashing;
}

void afficherLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Dist  : ");
  lcd.print(dist);
  lcd.print("cm     ");

  lcd.setCursor(0, 1);
  if (doorInMovement || currentState == OUVERTURE || currentState == FERMETURE) {
    deg = map(myStepper.currentPosition(), POSITION_FERMEE, POSITION_OUVERTE, MIN_DEGREE, MAX_DEGREE);
    lcd.print("Angle : ");
    lcd.print(deg);
    lcd.print((char)223);
    lcd.print("      ");
  } else {
    if (abs(myStepper.currentPosition() - POSITION_FERMEE) < 5)
      lcd.print("Fermee          ");
    else if (abs(myStepper.currentPosition() - POSITION_OUVERTE) < 5)
      lcd.print("Ouverte         ");
  }
}

void setLED(bool red, bool green, bool blue) {
  digitalWrite(LED_R, red);
  digitalWrite(LED_G, green);
  digitalWrite(LED_B, blue);
}

void eteindreLED() {
  setLED(LOW, LOW, LOW);
}

void afficherDemarrage() {
  lcd.setCursor(0, 0);
  lcd.print(NUM_ETUDIANT);
  lcd.setCursor(0, 1);
  lcd.print("LAB 6");
}
