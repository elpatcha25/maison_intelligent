// main.ino
#include <HCSR04.h>
#include <LCD_I2C.h>
#include <U8g2lib.h>

#include "Alarm.h"
#include "PorteAutomatique.h"


#define TRIGGER_PIN 6
#define ECHO_PIN 7
#define BUZZER_PIN 5
#define RED_PIN 31
#define GREEN_PIN 33
#define BLUE_PIN 35
#define IN_1 8
#define IN_2 9
#define IN_3 10
#define IN_4 11
#define CLK_PIN 2
#define DIN_PIN 3
#define CS_PIN 4


HCSR04 hc(TRIGGER_PIN, ECHO_PIN);
LCD_I2C lcd(0x27, 16, 2);
U8G2_MAX7219_8X8_1_4W_SW_SPI u8g2(U8G2_R0, CLK_PIN, DIN_PIN, CS_PIN, U8X8_PIN_NONE);

float distance = 0;
unsigned long currentTime = 0;
const char* NUM_ETUDIANT = "2407822";

Alarm alarm(RED_PIN, GREEN_PIN, BLUE_PIN, BUZZER_PIN, distance);
PorteAutomatique porte(IN_1, IN_2, IN_3, IN_4, distance);


enum EtatAffichage { AUCUN, OK, ERREUR, INVALIDE };
EtatAffichage affichageActif = AUCUN;
unsigned long affichageDebut = 0;
const unsigned long DUREE_AFFICHAGE = 3000;


const unsigned long INTERVALLE_DIST = 50;
const unsigned long INTERVALLE_LCD = 100;


int limiteBas = 30;
int limiteHaut = 60;

void setup() {
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();
  u8g2.begin();
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setContrast(5);

  porte.setPasParTour(2038);
  porte.setAngleFerme(10);
  porte.setAngleOuvert(170);
  porte.setDistanceOuverture(limiteBas);
  porte.setDistanceFermeture(limiteHaut);

  alarm.setColourA(255, 0, 0);
  alarm.setColourB(0, 0, 255);
  alarm.setDistance(15);
  alarm.setTimeout(3000);
  alarm.setVariationTiming(200);

  afficherDemarrage();
}

void loop() {
  currentTime = millis();
  mesurerDistance();
  porte.update();
  alarm.update();
  afficherLCD();
  gestionAffichageMatrix();
  lireCommandesSerie();
}

void afficherDemarrage() {
  lcd.setCursor(0, 0);
  lcd.print(NUM_ETUDIANT);
  lcd.setCursor(0, 1);
  lcd.print("LABO 07");
  delay(2000);
  lcd.clear();
}

void mesurerDistance() {
  static unsigned long lastMesure = 0;
  if (millis() - lastMesure >= INTERVALLE_DIST) {
    float d = hc.dist();
    if (d > 0) distance = d;
    lastMesure = millis();
  }
}

void afficherLCD() {
  static unsigned long lastAffichage = 0;
  if (millis() - lastAffichage >= INTERVALLE_LCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dist: ");
    lcd.print(distance);
    lcd.print(" cm");

    lcd.setCursor(0, 1);
    const char* etat = porte.getEtatTexte();
    if (strcmp(etat, "Ouverture") == 0 || strcmp(etat, "Fermeture") == 0) {
      lcd.print("Angle: ");
      lcd.print((int)porte.getAngle());
      lcd.print((char)223);
    } else {
      lcd.print("Porte: ");
      lcd.print(etat);
    }

    lastAffichage = millis();
  }
}

void gestionAffichageMatrix() {
  if (affichageActif == AUCUN) return;

  if (millis() - affichageDebut > DUREE_AFFICHAGE) {
    u8g2.clearDisplay();
    affichageActif = AUCUN;
    return;
  }

  u8g2.clearBuffer();
  switch (affichageActif) {
    case OK:
      u8g2.drawLine(1, 5, 3, 7);
      u8g2.drawLine(3, 7, 7, 1);
      break;
    case ERREUR:
      u8g2.drawCircle(3, 3, 3);
      u8g2.drawLine(0, 0, 7, 7);
      break;
    case INVALIDE:
      u8g2.drawLine(7, 7, 0, 0);
      u8g2.drawLine(0, 7, 7, 0);
      break;
    default:
      break;
  }
  u8g2.sendBuffer();
}

void afficherEtat(EtatAffichage etat) {
  affichageActif = etat;
  affichageDebut = millis();
}

void lireCommandesSerie() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "g_dist") {
    Serial.print("Arduino: ");
    Serial.println(distance);
    afficherEtat(OK);
  } else if (cmd.startsWith("cfg;alm;")) {
    int v = cmd.substring(8).toInt();
    if (v > 0) {
      alarm.setDistance(v);
      afficherEtat(OK);
    } else afficherEtat(ERREUR);
  } else if (cmd.startsWith("cfg;lim_inf;")) {
    int v = cmd.substring(12).toInt();
    if (v > 0 && v < limiteHaut) {
      limiteBas = v;
      porte.setDistanceOuverture(v);
      afficherEtat(OK);
    } else afficherEtat(ERREUR);
  } else if (cmd.startsWith("cfg;lim_sup;")) {
    int v = cmd.substring(12).toInt();
    if (v > limiteBas) {
      limiteHaut = v;
      porte.setDistanceFermeture(v);
      afficherEtat(OK);
    } else afficherEtat(ERREUR);
  } else {
    afficherEtat(INVALIDE);
  }
}
