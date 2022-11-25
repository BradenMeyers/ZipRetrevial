#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <serverESP.h>

#define READY 1
#define ZIPPING 2
#define RECOVERY 3
#define motorChannel 0
#define resolution 8
#define freq 5000
#define RECOVERY_TO_READY_AFTER_BEEN_GRABBED_TIME 1700


class Timer{
public:
  unsigned long currentTime;
  bool started = false;
  void start(){
    currentTime = millis();
  }
  unsigned long getTime(){
    return millis() - currentTime;
  }
};

byte readyLight = 25;
byte zipLight = 32;
byte recoveryLight = 23;
byte buzzer = 27;
byte motorDirection = 22;
byte motor = 21;
byte batteryMonitor = 34;
byte handleBarSensor = 14;
byte state = READY;
bool atTheTop = false;
bool someonOnStateChanged = false;
unsigned long stallRotationCount;
int motorAccelerationTimeLimit = 25;
byte motorsMaxSpeed = 200;
unsigned long afterZipDelay = 7000;
Timer recoveryTimer;
Timer atTheTopTimer;
Timer readyTimeOutTimer;
Timer stallTimer;
Timer handleBarTimer;

byte odometerLed = 19;
byte odometerSensor = 33;
int odometerSensorValue = 0;
unsigned long rotations = 0;
unsigned long beforeRotations;
unsigned long countToTopLimit;
unsigned long countToTopLimitOffset = 20;
bool crossedThreshold = false;
bool shouldCheckAtTheTop = true;
int highThreshold = 3000;
String highThresholdStr = String(highThreshold);
int lowThreshold = 300;
String lowThresholdStr = String(lowThreshold);
String currentIrStr;
Timer odometerTimer;

const char* ssid = "Zip-Line-Controls";
const char* password = "123456789";

const char* someoneOnLog = "Stopping motor because someone was on";
const char* wifiStopMotorLog = "Stopping motor because wifi";
const char* stalledLog = "Stopping motor because it is stalled";

String stateStr = "Ready";
String directionStr = "Up";
String motorMaxSpeedStr = String(motorsMaxSpeed);
String motorDirectionStr = "Up";
String afterZipDelayStr = String(afterZipDelay * 0.001);  //cause actual variable is in miliseconds but wifi is in seconds

bool wifiStopMotor = false;
bool wifiSkipToRecovery = false;

void countRotations(){
  for(int times=0; times<=4; times++){
  odometerSensorValue = analogRead(odometerSensor);
  if(odometerSensorValue > highThreshold and not crossedThreshold){
    //logger.logln(rotations);
    rotations++;
    crossedThreshold = true;
    Serial.println(rotations);
  }
  else if(odometerSensorValue < lowThreshold){crossedThreshold = false;}
  }
}

void startCount(){
  beforeRotations = rotations;
  logger.log("Before Rotations: ");
  logger.log(beforeRotations, true);
}

void stopCount(){
  if(rotations - beforeRotations < 10){   //This is if someone pulls it at the top and stuff dont actully go down
    countToTopLimit = rotations + 500;
    return;
  }
  countToTopLimit = (rotations + (rotations - beforeRotations)) - countToTopLimitOffset;
  logger.log("Count limit: \n");
  logger.log(countToTopLimit);
  logger.log("Rotatons: \n");
  logger.log(rotations);
}

bool isAtTheTop(){
  if(shouldCheckAtTheTop){
    if(rotations > countToTopLimit){return true;}
  }
  return false;
}

bool isStalled(){
  // If there have been no rotations in less than 160 milliseconds will return true, otherwise false
  if(not stallTimer.started){
    stallTimer.start();
    stallTimer.started = true;
    stallRotationCount = rotations;
    return false;
  }
  else if(stallTimer.getTime() > 160){
    stallTimer.started = false;
    if(rotations - stallRotationCount < 1){
      return true;
    }
  }
  return false;
}

bool someoneOn(int timeLimit){
  static byte handleBarSensorValue = 0;
  if(digitalRead(handleBarSensor) != handleBarSensorValue){
    if(handleBarTimer.getTime() > timeLimit){
      handleBarSensorValue = !handleBarSensorValue;
    }
  }
  else{handleBarTimer.start();}

  if(handleBarSensorValue){return true;}  //adjust handlebar to be opposite with the pullup
  return false;
}

void turnOnMotor(int speed){
  ledcWrite(motorChannel, speed);
  if(speed == 0){
    logger.log("MOTOR IS OFF------------------", true);
  }
}

void readyAtTheTop(){
  logger.checkLogLength();
  digitalWrite(readyLight, HIGH);
  readyTimeOutTimer.start();
  while(!someoneOn(1000)){
    if(/* readyTimeOutTimer.getTime() > 120000 or */ wifiSkipToRecovery){
      state = RECOVERY;
      wifiSkipToRecovery = false;
      logger.log("Going up the zip line because ready timed out.", true);
      return;
    }
  }
  state = ZIPPING;
  digitalWrite(readyLight, LOW);
}

void movingDown(){
  digitalWrite(zipLight, HIGH);
  startCount();
  while(someoneOn(1000)){
    countRotations();
  }
  stopCount();
  logger.log("Total Rotations that were zipped: ");
  logger.log(rotations - beforeRotations, true);
  state = RECOVERY;
  digitalWrite(zipLight, LOW);
}

void stopTheMotor(){
  turnOnMotor(0);
  digitalWrite(recoveryLight, LOW);
  digitalWrite(buzzer, LOW);
  /* if(!wifiStopMotor){
    if(atTheTopTimer.getTime() > 2000){countToTopLimitOffset += 2;}
    else if(atTheTopTimer.getTime() < 1000){countToTopLimitOffset -= 2;}
  } */
  wifiStopMotor = false;
  recoveryTimer.start();
  while(recoveryTimer.getTime() < RECOVERY_TO_READY_AFTER_BEEN_GRABBED_TIME){}
  state = READY;
}

void moveToTop(){
  digitalWrite(recoveryLight, HIGH);
  digitalWrite(buzzer, HIGH);
  atTheTop = false;
  recoveryTimer.start();
  while(recoveryTimer.getTime() < afterZipDelay){
    if(wifiStopMotor){
      wifiStopMotor = false;
      stopTheMotor();
      logger.log(wifiStopMotorLog, true);
      return;
    }
    if(someoneOn(1000)){
      stopTheMotor();
      logger.log(someoneOnLog, true);
      return;
    }
  }
  logger.log("Begin speeding motor up", true);
  digitalWrite(buzzer, LOW);
  for(int motorSpeed=50; motorSpeed<=motorsMaxSpeed; motorSpeed++){
    turnOnMotor(motorSpeed);
    recoveryTimer.start();
    while(recoveryTimer.getTime() < motorAccelerationTimeLimit){
      countRotations();
      if(someoneOn(50)){
        stopTheMotor();
        logger.log(someoneOnLog, true);
        return;
      }
      if(wifiStopMotor){
        stopTheMotor();
        logger.log(wifiStopMotorLog, true);
        return;
      }
      if(motorSpeed > motorsMaxSpeed - 30 and isStalled()){
        logger.log(stalledLog, true);
        stopTheMotor();
        return;
      }
    }
  }
  logger.log("Motor has reached max speed", true);
  while(true){
    countRotations();
    if(someoneOn(50)){
      logger.log(someoneOnLog, true);
      break;
    }
    if(wifiStopMotor){
      logger.log(wifiStopMotorLog, true);
      break;
    }
    if(isStalled()){
        logger.log(stalledLog, true);
        stopTheMotor();
        break;
      }
    // if(isAtTheTop() and not atTheTop){
    //   turnOnMotor(motorsMaxSpeed - 70);
    //   logger.log("Turning motor down because were at the top", true);
    //   atTheTop = true;
    //   atTheTopTimer.start();
    // }
  }
  stopTheMotor();
}

void updateVariableStrings(){
  if(state == READY){stateStr = "Ready";}
  else if(state == ZIPPING){stateStr = "Zipping";}
  else if(state == RECOVERY){stateStr = "Recovery";}
}

// Replaces placeholder with button section in your web page
String processor(const String& var){
  //logln(var);
  if (var == "MAXSPEEDVALUE"){return String(motorsMaxSpeed);}
  else if(var == "STATE"){return stateStr;}
  else if(var == "DIRECTION"){return directionStr;}
  else if(var == "AFTERZIPDELAY"){return afterZipDelayStr;}
  else if(var == "CURRENTIRVALUE"){return String(analogRead(odometerSensor));}
  else if(var == "LOWTHRESHOLD"){return lowThresholdStr;}
  else if(var == "HIGHTHRESHOLD"){return highThresholdStr;}
  else if(var == "LOGSTRING"){return logger.logStr;}
  return String();
}

void setup(){
  Serial.begin(9600);
  analogReadResolution(12);

  pinMode(odometerLed, OUTPUT);
  digitalWrite(odometerLed, HIGH);  //this light stays on all the time
  pinMode(handleBarSensor, INPUT);
  pinMode(readyLight, OUTPUT);
  pinMode(zipLight, OUTPUT);
  pinMode(recoveryLight, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(motorDirection, OUTPUT);
  pinMode(motor, OUTPUT);
  pinMode(batteryMonitor, INPUT);
  digitalWrite(motor, LOW);
  digitalWrite(motorDirection, HIGH);
  ledcSetup(motorChannel, freq, resolution);
  ledcAttachPin(motor, motorChannel);
  ledcWrite(motorChannel, 0);



}

void loop(){
  updateVariableStrings();
  logger.log("Current state : ");
  logger.log(stateStr, true);

  if(state == READY){readyAtTheTop();}
  else if(state == ZIPPING){movingDown();}
  else if(state == RECOVERY){moveToTop();}
}
