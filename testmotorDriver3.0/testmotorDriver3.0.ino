#include <Arduino.h>

#define Serial //
const int pwm = 21;
const int pwmChan = 1;
const int dir = 22;
void setup() {
  // put your setup code here, to run once:
Serial.begin(115200);
pinMode(dir,OUTPUT);
ledcAttachPin(pwm, pwmChan);
ledcSetup(pwmChan, 12000, 8); // 12 kHz PWM, 8-bit resolution
//digitalWrite(dir, HIGH);

}

void loop() {
  // put your main code here, to run repeatedly:


for(int i = 0; i<=200; i++){
  digitalWrite(dir, LOW);
  ledcWrite(pwmChan, i);
  delay(10);
  Serial.println("forward");
}
//ledcWrite(pwmChan,0);
delay(300);
for(int i = 200; i > 0; i -= 1){
  digitalWrite(dir, LOW);
  ledcWrite(pwmChan, i);
  delay(10);
  Serial.println("forward");
}
ledcWrite(pwmChan,0);
delay(1000);
//digitalWrite(dir, HIGH);
for(int m = 0; m <= 200; m++){
  digitalWrite(dir,HIGH);
  ledcWrite(pwmChan, m);
  delay(10);
  Serial.println("backward");
}
delay(300);
for(int m = 200; m > 0; m -= 1){
  digitalWrite(dir, HIGH);
  ledcWrite(pwmChan, m);
  delay(10);
  Serial.println("forward");
}
ledcWrite(pwmChan, 0);
delay(1000);

/*
ledcWrite(pwmChan,100);
Serial.println("imworking");
*/
}
