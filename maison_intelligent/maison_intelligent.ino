#include <HCSR04.h>
#include <AccelStepper.h>
#include <LCD_I2C.h>
#include <string.h>

HCSR04 hc(6, 7);
LCD_I2C lcd(0x27, 20, 21);
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

enum EtatPorte { FERMEE,
                 OUVERTURE,
                 OUVERTE,
                 FERMETURE };
EtatPorte currentState = OUVERTURE;


const String NUM_ETUDIANT = "2407822";
const float tour = 2038.0;
const int MAX_DEGREE = 170;
const int MIN_DEGREE = 10;
const long POSITION_OUVERTE = (tour * MAX_DEGREE) / 360.0;
const long POSITION_FERMEE = (tour * MIN_DEGREE) / 360.0;

const int MAX_SPEED = 500;
const int ACCELERATION = 300;
bool doorInMovement = false;

const int MAX_CENTIMETRE = 60;
const int MIN_CENTIMETRE = 30;
const int ALARM_DISTANCE_LIMIT = 15;

const unsigned long TIME_ALARME_OFF = 3000;
const unsigned long DISTANCE_TIME = 50;


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

  afficherDemarrage();
  delay(2000);
  lcd.clear();
}


void loop() {
  currentTime = millis();

  if (currentTime - distanceTime > DISTANCE_TIME) {
    distanceTime = currentTime;
    currentDistance = hc.dist();
  }
if (currentDistance>0){
  dist=currentDistance;
}
  gererAlarme();
  gestionEtat();
  afficherLCD();
  affichageSerie();

  if (doorInMovement) {
    myStepper.enableOutputs();
    myStepper.run();
    if (!myStepper.isRunning()) {
      doorInMovement = false;
      myStepper.disableOutputs();
    }
  }
}


void gestionEtat() {
 if (alarmState) return;

  switch (currentState) {
    case FERMEE:
      etatFermee();
      break;
    case OUVERTURE:
      etatOuverture();
      break;
    case OUVERTE:
      etatOuverte();
      break;
    case FERMETURE:
      etatFermeture();
      break;
  }
}

void etatFermee() {

 
  
  if (dist < MIN_CENTIMETRE){

    currentState = OUVERTURE;
    myStepper.moveTo(POSITION_OUVERTE);
    doorInMovement = true;
  }
}

void etatOuverture() {


  if (!doorInMovement && myStepper.distanceToGo() == 0) {
    currentState = OUVERTE;
    tempsOuverte = currentTime;
  }
}

void etatOuverte() {
  
  setLED(LOW, HIGH, LOW);  

  if (dist > MAX_CENTIMETRE) {
    currentState = FERMETURE;
    myStepper.moveTo(POSITION_FERMEE);
    doorInMovement = true;
  }
}

void etatFermeture() {


  if (!doorInMovement && myStepper.distanceToGo() == 0) {
    currentState = FERMEE;
  }
}

void gererAlarme() {
  if ( dist < ALARM_DISTANCE_LIMIT) {
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
  if (doorInMovement) {
    deg = map(myStepper.currentPosition(), POSITION_FERMEE, POSITION_OUVERTE, MIN_DEGREE, MAX_DEGREE);
    lcd.print("Angle : ");
    lcd.print(deg);
    lcd.print((char)223);
    lcd.print("      ");
  } else {
    if (abs(myStepper.currentPosition() - POSITION_FERMEE) < 5)
      lcd.print("Ferme           ");
    else if (abs(myStepper.currentPosition() - POSITION_OUVERTE) < 5)
      lcd.print("Ouverte         ");
  }
}

void affichageSerie() {
  static unsigned long dernierTemps = 0;
  if (currentTime - dernierTemps >= 100) {
    dernierTemps = currentTime;
    Serial.println("etd:" + NUM_ETUDIANT + ",dist:" + String(dist) + ",deg:" + String(deg));
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
  lcd.print("LAB 4B");
}
