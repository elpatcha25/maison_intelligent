#include "PorteAutomatique.h"
#include <Arduino.h>

PorteAutomatique::PorteAutomatique(int p1, int p2, int p3, int p4, float& distancePtr)
  : _stepper(AccelStepper::HALF4WIRE, p1, p3, p2, p4), _distance(distancePtr) {
  _stepper.setMaxSpeed(800);
  _stepper.setAcceleration(400);
  _stepper.setCurrentPosition(_angleEnSteps(_angleFerme));
  _stepper.moveTo(_angleEnSteps(_angleFerme));
  _stepper.enableOutputs();
}

void PorteAutomatique::update() {
  _currentTime = millis();
  _mettreAJourEtat();

  switch (_etat) {
    case FERMEE: _fermeState(); break;
    case OUVERTE: _ouvertState(); break;
    case EN_OUVERTURE: _ouvertureState(); break;
    case EN_FERMETURE: _fermetureState(); break;
  }

  _stepper.run();
}

void PorteAutomatique::setAngleOuvert(float angle) {
  _angleOuvert = angle;
}

void PorteAutomatique::setAngleFerme(float angle) {
  _angleFerme = angle;
}

void PorteAutomatique::setPasParTour(int steps) {
  _stepsPerRev = steps;
}

void PorteAutomatique::setDistanceOuverture(float distance) {
  _distanceOuverture = distance;
}

void PorteAutomatique::setDistanceFermeture(float distance) {
  _distanceFermeture = distance;
}

const char* PorteAutomatique::getEtatTexte() const {
  switch (_etat) {
    case FERMEE: return "Fermee";
    case OUVERTE: return "Ouverte";
    case EN_OUVERTURE: return "Ouverture";
    case EN_FERMETURE: return "Fermeture";
    default: return "Inconnu";
  }
}

float PorteAutomatique::getAngle() const {
  return map(_stepper.currentPosition(), _angleEnSteps(_angleFerme), _angleEnSteps(_angleOuvert), _angleFerme, _angleOuvert);
}

void PorteAutomatique::_mettreAJourEtat() {
  if (_etat == FERMEE && _distance < _distanceOuverture && _distance > 0) {
    _etat = EN_OUVERTURE;
    _ouvrir();
  } else if (_etat == OUVERTE && _distance > _distanceFermeture) {
    _etat = EN_FERMETURE;
    _fermer();
  }

  if (_etat == EN_OUVERTURE && _stepper.distanceToGo() == 0) {
    _etat = OUVERTE;
  } else if (_etat == EN_FERMETURE && _stepper.distanceToGo() == 0) {
    _etat = FERMEE;
  }
}

void PorteAutomatique::_ouvrir() {
  _stepper.moveTo(_angleEnSteps(_angleOuvert));
  _stepper.enableOutputs();
}

void PorteAutomatique::_fermer() {
  _stepper.moveTo(_angleEnSteps(_angleFerme));
  _stepper.enableOutputs();
}

void PorteAutomatique::_ouvertState() {
  _stepper.disableOutputs();
}
void PorteAutomatique::_fermeState() {
  _stepper.disableOutputs();
}
void PorteAutomatique::_ouvertureState() {
  if (!_stepper.isRunning()) {
    _etat = OUVERTE;
  }
}
void PorteAutomatique::_fermetureState() {
  if (!_stepper.isRunning()) {
    _etat = FERMEE;
  }
}

long PorteAutomatique::_angleEnSteps(float angle) const {
  return static_cast<long>((angle / 360.0) * _stepsPerRev);
}
