/* Sweep
 by BARRAGAN <http://barraganstudio.com>
 This example code is in the public domain.

 modified 8 Nov 2013
 by Scott Fitzgerald
 http://www.arduino.cc/en/Tutorial/Sweep
*/

const int IN = 34; //
int Irvalue = 0; //
const int OUT = 19;
void setup() {
  pinMode(OUT, OUTPUT);
  digitalWrite(OUT, HIGH);
  Serial.begin(115200);// attaches the servo on pin 9 to the servo object
}


void loop() {

  Irvalue = analogRead(IN);
  Serial.print("IR Value: ");
  Serial.println(Irvalue);

  
 
}
