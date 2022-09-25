// Load Wi-Fi library
#include <WiFi.h>
#include <Preferences.h> 
Preferences preferences;
//States
#define READY 1
#define ZIPPING 2
#define RECOVERY 3
#define MANUAL 4
//Timer that can be used for different function
class Timer{
  unsigned long currentTime;
  public:
    void start(){
      currentTime = millis();
    }
    unsigned long getTime(){
      return millis() - currentTime;
    }
};
//FUNCTIONS
Timer ready;
Timer recovery;
Timer zipping;
Timer reDelay;
Timer spencer;
Timer stall;

int readyDelay = 750;       //How long to wait after recovery state is deactivated before going into recovery. 
int recoveryDelay = 1000;   //how long before starting up the motor when recovery state is initiated. 
int menu = 1;               // Wifi menu setting
int handlebarbuttonvalue = 0;   //Variable to see if the handlebar is pulled. 0 if pulled and 1 if not pulled
int state = READY;          //intial state
int maxSpeed = 195;         //Max speed of motor but is a stored value on the ESP32 after the code has run before. 
int irStop = 0;          //counter for how many times the ir lights stop the pulley
bool zipStop = false;     //A test to see if the recovery mode was deactivated before motor startup to detect posible problems 
unsigned long totalZip = 0;    //A stat timer to see how much zipping time was done the whole day

//Pin assignments
int motor = 21;
int handlebarbutton = 14;
int readylight = 32;
int ziplight = 25;        
int recoverylight = 26;    
const int AIN = 34;       // For photodiode
const int LED_OUT = 19;
const int direction = 22;
const int speaker = 27;

//PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

bool annoy = false;
bool isBeep = false;
int annoyDelay = 500;

void sBuzz(){       //Funciton designed for SPENCER so the system will beep and scare him when a button is pressed on the web page :)
  if(annoy){
    if(spencer.getTime() >= annoyDelay){
      isBeep = !isBeep;     //TURN OFF AND ON THE SPEAKER EVERY 19 
      spencer.start();
      if(isBeep){
        digitalWrite(speaker, HIGH);
         if(annoyDelay > 19){
           annoyDelay -= 19;
         }
         else{
           annoyDelay = 6000;
           digitalWrite(speaker, HIGH);
         }
      }
      else{
        digitalWrite(speaker,LOW);
      }
    }
  }
}

void checkbuttons(){
  handlebarbuttonvalue = digitalRead(handlebarbutton);
}

//ALL IR FUNCTIONS
unsigned long rotStart = 0;   //IR rotation counting, how many total rotations had happened when started. 
int previousRot = 0;          // the difference between current and starting rotations. 
double previousZip = 0;       // Time on the last zipline run
int maxRot = 0;               //Find the fastest speed reached on the zi
int runs = 0;
int maxRotR = 0;
unsigned long pulses = 0;

void startRot(){
  rotStart = pulses;
}
void getRot(){
  previousRot = pulses - rotStart;
}

//Checking for a system break or motor stall.
int checkStallCount = 3;
bool setTime = true;
int freqSpeed = 500;
bool trigger = false;
void checkStall(){           //check stall is counting the difference in the pulses from one time to another. changes bool-trigger to true when not enough rotations happen
  long pulseDifference = 0;
  unsigned long pastRot = 0;
  if (setTime == false){        //only will happen on the initialization of the function
      stall.start();
      pastRot = pulses;
      setTime = true;
      //Serial.println("set time");
  }
  if (stall.getTime() >= freqSpeed){
      pulseDifference = (pulses - pastRot);
      Serial.println(pulseDifference);
      if(state == ZIPPING){
        if(pulseDifference > maxRot){
          maxRot = pulseDifference;
        }
      }
      if(state == RECOVERY){
        if(pulseDifference > maxRotR){      //See if the
          maxRotR = pulseDifference;
        }
      }
      if (pulseDifference < checkStallCount){     //If the spin count of the back wheel does not exceed the preset value then a trigger will be set 
          trigger = true;                         //This will stop the motor so because the system is not working properly.
          readyDelay = 2500;
          Serial.println("trigger on");
      }
      setTime = false;
      //Serial.print("trueee");
  }
}
// 2 crossing trigger.
double upper_threshold = 2500; // TODO get real values
double lower_threshold = 1000; // TODO get real values
bool crossed_upper = false;
double lock_in_data = 0;

// General variables
unsigned long nowTime = 0;

// LED flashing
int f_led = 2000; // LED oscilation frequency in Hz
unsigned long led_period = (unsigned long)(1e6/(2*f_led)); // microseconds that it spends in on/off state.
bool LED_STATUS = LOW; // Have it turned off at the start
unsigned long next_toggle = 0; // Toggling the LED

// Lock-in detection / collecting the data //*** not using currently 
unsigned long collectrate = led_period * 22; // Arbitrayily decided 22 times the led period. It just needs to be larger than the led period
unsigned long next_collect = 0;
unsigned long num_on_measures = 0;
unsigned long num_off_measures = 0;
double on_values = 0;
double off_values = 0;

void simpleCount(){           //simple version of Lockin code. checking to see if value is going over upper threshold and below lower threshold
  digitalWrite(LED_OUT, HIGH);  //IR light always stays on in this code
  lock_in_data = analogRead(AIN);
  //Serial.print("IR value: "), Serial.println(lock_in_data);
  if(crossed_upper){
    if(lock_in_data < lower_threshold)
    {
      //Uncomment to count pulses *****
      ++pulses;
      Serial.println(pulses);
      crossed_upper = false;
    }
  }
  else if (lock_in_data > upper_threshold){
    crossed_upper = true;
  }
}
void spinCount()  //Way to count use the IR lights to count rotations when lockin data is needed due to excessive light. 
{
  nowTime = micros();
  //Serial.println(LED_STATUS);
  //Serial.println(analogRead(AIN));
  // LED status
  if (nowTime > next_toggle) // Toggles the LED
  {
    LED_STATUS = not LED_STATUS;
    next_toggle += led_period;
    digitalWrite(LED_OUT,LED_STATUS);
  }
  if(LED_STATUS)
  {
    on_values += analogRead(AIN);
    num_on_measures++;
  }
  else
  {
    off_values += analogRead(AIN);
    num_off_measures++;
  }
  // Collection and lock-in detection
  if (nowTime > next_collect)
  {
    // Get the lock-in data
    lock_in_data = on_values/num_on_measures - off_values/num_off_measures;
    next_collect += collectrate;
    on_values = 0L;
    num_on_measures = 0L;
    off_values = 0L;
    num_off_measures = 0L;

    //Uncomment to display the raw data *****
     //Serial.println(lock_in_data);
    
    // Check if the data was a rotation
    if(crossed_upper)
    {
      if(lock_in_data < lower_threshold)
      {
        //Uncomment to count pulses *****
        ++pulses;
        Serial.println(pulses);
        crossed_upper = false;
      }
    }else if (lock_in_data > upper_threshold)
    {
      crossed_upper = true;
    }
  }
}

String lowerThreshold = String(lower_threshold);
String upperThreshold = String(upper_threshold);
String maxString = String(maxSpeed);
String stateS = "READY";
int currentIr;             //  Variable to see the current IR light reciever value
// Variable to store the HTTP request
String header;
// html variables
String directionState = "off";
String output28State = "auto";
String directionString = "Forward";
String IRstring = "0";
//slider variables
int lower1 = 0;   //lower threshold
int lower2 = 0;
int upper1 = 0;    //upper threshold
int upper2 = 0;
int inputS1 = 0;    //checkStallCount
int inputF1 = 0;
String inputStringUno;
int inputS2 = 0;
int inputF2 = 0;
String inputStringDos;
int pos1 = 0;       //motor Speed
int pos2 = 0;
String valueString = String(0);
int max1 = 0;       //max motor speed
int max2 = 0;

// WIFI variables
// Replace with your network credentials
const char* ssid     = "ESP32-Access-Point";
const char* password = "123456789";
// Set web server port number to 80
WiFiServer server(80);
//html timers
unsigned long Time = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

//WIFI SETTINGS AND FUNCTIONS
void checkClient(){ 
                          //THIS FUNCTION HAS ALL THE WIFI STUFF                    
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          
    Time = millis();
    previousTime = Time;
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()  && Time - previousTime <= timeoutTime) {            // loop while the client's connected
      Time = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             
        Serial.write(c);                   
        header += c;
        if (c == '\n') {                    
          // if the current line is blank, you got two newline characters in a row.
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            //EVERTHING ABOVE HAS TO STAY BUT YOU CAN MESS WITH THIS STUFF BELOW
            //changes the direction
            if (header.indexOf("GET /26/on") >= 0) {  
              Serial.println("GPIO 26 on");
              directionState = "on";
              directionString = "Backward";
              digitalWrite(direction, HIGH);
            } else if (header.indexOf("GET /26/off") >= 0) {
              Serial.println("GPIO 26 off");
              directionState = "off";
              directionString = "Forward";
              digitalWrite(direction, LOW);
            } 
            //Stop button
            else if (header.indexOf("GET /27/off") >= 0) {
              Serial.println("stop");
              ledcWrite(ledChannel, 0);
              state = READY;
              readyDelay = 2000;
            } 
            //  Button to turn on the beeper annoying sound          
            else if (header.indexOf("GET /72/off") >= 0) {
              if(annoy){
                annoy = false;
                digitalWrite(speaker, LOW);
              }
              else{
                annoy = true;
                annoyDelay = 500;
                spencer.start();
              }
            }
            //MENU BUTTONS            
            else if (header.indexOf("GET /menu/1") >= 0) {
              menu = 1;
            } 
            else if (header.indexOf("GET /menu/2") >= 0) {
              menu = 2;
            }
            else if (header.indexOf("GET /menu/3") >= 0) {
              menu = 3;
            }
            //Reset Statistics
            else if (header.indexOf("GET /reset/1") >= 0) {
              runs = 0;
              maxRotR = 0;
              totalZip = 0;
              pulses = 0;
              irStop = 0;
            } 
            //Manual and Auto controlled motor
            else if (header.indexOf("GET /28/on") >= 0) {
              Serial.println("Manual on");
              output28State = "manual";
              state = MANUAL;
              stateS = "Manual";
            }else if (header.indexOf("GET /28/off") >= 0) {
              Serial.println("Manual off");
              output28State = "auto";
              state = READY;
              stateS = "Ready";
              readyDelay = 2000;
              ledcWrite(ledChannel, 0);
              valueString = "0";
            }
            //motor speed slider
            if(header.indexOf("GET /?value=")>=0) {
              pos1 = header.indexOf('=');
              pos2 = header.indexOf('&');
              valueString = header.substring(pos1+1, pos2);
              if (state == MANUAL){
                ledcWrite(ledChannel, valueString.toInt());
              }
              Serial.print("Motor Speed: ");
              Serial.println(valueString);
            }
            //max speed slider
            if(header.indexOf("GET /?value1=")>=0) {
              max1 = header.indexOf('=');
              max2 = header.indexOf('&');
              maxString = header.substring(max1+1, max2);
              maxSpeed = maxString.toInt();
              Serial.print("Max Speed: ");
              Serial.println(maxString);
              preferences.putUInt("max", maxSpeed);
            }
            //lower threshold slider for IR reciever
            if(header.indexOf("GET /?value2=")>=0) {
              lower1 = header.indexOf('=');
              lower2 = header.indexOf('&');
              lowerThreshold = header.substring(lower1+1, lower2);
              lower_threshold = lowerThreshold.toInt();
              Serial.print("Lower Threshold: ");
              Serial.println(lowerThreshold);
              preferences.putUInt("lower", lower_threshold);
            }
            //upper threshold slider for IR reciever
            if(header.indexOf("GET /?value3=")>=0) {
              upper1 = header.indexOf('=');
              upper2 = header.indexOf('&');
              upperThreshold = header.substring(upper1+1, upper2);
              upper_threshold = upperThreshold.toInt();
              Serial.print("Upper Threshold: ");
              Serial.println(upperThreshold);
              preferences.putUInt("upper", upper_threshold);
            }
            //check stall count input text field
            if(header.indexOf("GET /?input1=")>=0) {
              inputS1 = header.indexOf('=');
              inputF1 = header.indexOf('#');
              inputStringUno = header.substring(inputS1+1, inputF1);
              checkStallCount = inputStringUno.toInt();
              Serial.print("Stall Minimum: ");
              Serial.println(checkStallCount);
              preferences.putUInt("stall", checkStallCount);
            }
            if(header.indexOf("GET /?input2=")>=0) {
              inputS2 = header.indexOf('=');
              inputF2 = header.indexOf('#');
              inputStringDos = header.substring(inputS2+1, inputF2);
              recoveryDelay = inputStringDos.toInt();
              Serial.print("Recovery Delay: ");
              Serial.println(recoveryDelay);
              preferences.putUInt("rDelay", recoveryDelay);
            }
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 17px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #e62117;}");
            client.println(".button4 {background-color: #f28500;}");
            client.println(".button5 {background-color: #f2eb18;}");
            client.println(".button3 {background-color: #0b45d9;}</style></head>");
            client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial; margin-left:auto; margin-right:auto;}");
            client.println(".slider { width: 300px; }</style>");
            client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>");
            client.println("<body>");
            
            if(menu == 1){ 
              // Web Page Heading Text
              client.println("<h1>Pulley Controls</h1>");
              client.println("<p> State " + stateS + "</p>");         
              client.println("<p>Direction - State " + directionString + "</p>");
              // Direction Button     
              if (directionState=="off") {
                client.println("<p><a href=\"/26/on\"><button class=\"button\">BACKWARD</button></a></p>");
              } else {
                client.println("<p><a href=\"/26/off\"><button class=\"button button3\">FORWARD</button></a></p>");
              } 
              //Stop buttton
              client.println("<p><a href=\"/27/off\"><button class=\"button button2\">STOP</button></a></p>");
              client.println("<p><a href=\"/72/off\"><button class=\"button button5\">SPENCER</button></a></p>");
              //Manual and Auto button
              client.println("<p>Mode - " + output28State + "</p>");
              if (output28State == "auto") {
                client.println("<p><a href=\"/28/on\"><button class=\"button\">MANUAL</button></a></p>");
              } else {
                client.println("<p><a href=\"/28/off\"><button class=\"button button4\">AUTO</button></a></p>");
              } 
              //Motor speed slider
              client.println("<p>Motor Speed: <span id=\"servoPos\"></span></p>");          
              client.println("<input type=\"range\" min=\"0\" max=\"255\" class=\"slider\" id=\"servoSlider\" onchange=\"servo(this.value)\" value=\""+valueString+"\"/>");
                            //JAVASCRIPT
              client.println("<script>var slider = document.getElementById(\"servoSlider\");");
              client.println("var servoP = document.getElementById(\"servoPos\"); servoP.innerHTML = slider.value;");
              client.println("slider.oninput = function() { slider.value = this.value; servoP.innerHTML = this.value; }");
              client.println("$.ajaxSetup({timeout:1000}); function servo(pos) { ");
              client.println("$.get(\"/?value=\" + pos + \"&\"); {Connection: close};}</script>");
              //Max speed slider
              client.println("<p>Max Speed: <span id=\"maPos\"></span></p>");          
              client.println("<input type=\"range\" min=\"100\" max=\"255\" class=\"slider\" id=\"maSlider\" onchange=\"ma(this.value)\" value=\""+ maxString +"\"/>");
                        //JAVASCRIPT
              client.println("<script>var slider1 = document.getElementById(\"maSlider\");");
              client.println("var maP = document.getElementById(\"maPos\"); maP.innerHTML = slider1.value;");
              client.println("slider1.oninput = function() { slider1.value = this.value; maP.innerHTML = this.value; }");
              client.println("$.ajaxSetup({timeout:1000}); function ma(pos0) { ");
              client.println("$.get(\"/?value1=\" + pos0 + \"&\"); {Connection: close};}</script>"); 
              
              client.println("<p><a href=\"/menu/2\"><button class=\"button button4\">MENU</button></a></p>");
            }
            if(menu == 2){
              // IR heading
              client.println("<h1>IR Controls</h1>");
              currentIr = analogRead(AIN);
              IRstring = String(currentIr);
              client.println("<p>Current IR Value - " + IRstring + "</p>");
              //Lower threshold slider
              client.println("<p>Lower Threshold: <span id=\"lowerPos\"></span></p>");          
              client.println("<input type=\"range\" min=\"0\" max=\"4095\" class=\"slider\" id=\"lowerSlider\" onchange=\"lower(this.value)\" value=\""+ lowerThreshold+"\"/>");
                      //JAVASCRIPT
              client.println("<script>var slider2 = document.getElementById(\"lowerSlider\");");
              client.println("var lowerP = document.getElementById(\"lowerPos\"); lowerP.innerHTML = slider2.value;");
              client.println("slider2.oninput = function() { slider2.value = this.value; lowerP.innerHTML = this.value; }");
              client.println("$.ajaxSetup({timeout:1000}); function lower(pos1) { ");
              client.println("$.get(\"/?value2=\" + pos1 + \"&\"); {Connection: close};}</script>");
              //Upper Threshold Slider 
              client.println("<p>Upper Threshold: <span id=\"upperPos\"></span></p>");          
              client.println("<input type=\"range\" min=\"0\" max=\"4095\" class=\"slider\" id=\"upperSlider\" onchange=\"upper(this.value)\" value=\""+ upperThreshold+"\"/>");
                  //JAVASCRIPT
              client.println("<script>var slider3 = document.getElementById(\"upperSlider\");");
              client.println("var upperP = document.getElementById(\"upperPos\"); upperP.innerHTML = slider3.value;");
              client.println("slider3.oninput = function() { slider3.value = this.value; upperP.innerHTML = this.value; }");
              client.println("$.ajaxSetup({timeout:1000}); function upper(pos2) { ");
              client.println("$.get(\"/?value3=\" + pos2 + \"&\"); {Connection: close};}</script>");
              //INPUT TEXT FIELD
              client.println("<p>Current Stall Min: " + String(checkStallCount) + "</p>");
              client.println("<form id=\"inputTex\" action=\"#\">");
              client.println("<input type=\"text\" name=\"input1\"> <input type=\"submit\" value=\"Submit\"> </form>");
              //client.println("<script> document.getElementById(\"inputTex\").onsubmit = function(){var x = document.getElementById(\"input1\"); ");
              client.println("<p>Recovery Delay: " + String(recoveryDelay) + "</p>");
              client.println("<form id=\"inputDelay\" action=\"#\">");
              client.println("<input type=\"text\" name=\"input2\"> <input type=\"submit\" value=\"Submit\"> </form>");
              client.println("<p><a href=\"/menu/3\"><button class=\"button button4\">MENU</button></a></p>");
            }
            if(menu == 3){
              //PAGE OF INFO AND STATS           
              client.println("<h1>NERD STATS</h1>");
              client.println("<p><a href=\"/menu/1\"><button class=\"button button4\">MENU</button></a></p>");
              client.println("<p>Total Zips - " + String(runs) + "</p>");
              client.println("<p>Total Zip Time - " + String(totalZip) + "</p>");
              client.println("<p>Total Rotations - " + String(pulses) + "</p>");
              client.println("<p>Stalled Stop - " + String(irStop) + "</p>");
              client.println("<p>Last Zip Time - " + String(previousZip) + "</p>");
              client.println("<p>Last Zip Rotations - " + String(previousRot) + "</p>");
              client.println("<p>Last Max Zip Speed - " + String(maxRot) + "</p>");
              client.println("<p>Max Recovery Speed - " + String(maxRotR) + "</p>");
              client.println("<p><a href=\"/reset/1\"><button class=\"button button2\">RESET</button></a></p>");
            }
            //THE REST OF THIS JUST RUNS THE PAGE
            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void setup() {
  Serial.begin(115200);
  state = READY;
  
  //STORED VALUES ON THE ESP32 Memory. PUll from storage
  preferences.begin("my-app", false);
  maxSpeed = preferences.getUInt("max", 195);     //max speed
  upper_threshold = preferences.getUInt("upper", 2500); //Upper IR Threshold
  lower_threshold = preferences.getUInt("lower", 1000); //Lower IR threshold
  checkStallCount = preferences.getUInt("stall", 2);    //Number of motor stalls for the day.
  recoveryDelay = preferences.getUInt("rDelay", 1000);  //Delay after the zipping state
  maxString = String(maxSpeed);
  upperThreshold = String(upper_threshold);
  lowerThreshold = String(lower_threshold);

//  PIN SET UPS
  analogReadResolution(12);     //Analog Read values up to 4095
  pinMode(handlebarbutton, INPUT_PULLUP); //Will read a 1 if button is not pressed and a 0 if button is pressed
  pinMode(readylight, OUTPUT);
  pinMode(ziplight, OUTPUT);    //set the leds to output
  pinMode(recoverylight, OUTPUT);
  pinMode(speaker,OUTPUT);
  pinMode(direction, OUTPUT);
  digitalWrite(direction, LOW);
  digitalWrite(33, LOW); //I MAY HAVE SHORTED THIS WIRE SO I JUST CONNECT IT TO GROUND SO IT DOENSNT DO ANYTHING
  ledcSetup(ledChannel, freq, resolution);
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(motor, ledChannel);
  ledcWrite(ledChannel, 0);  
  pinMode(LED_OUT,OUTPUT);
  //  Odometer setup
  next_toggle = micros() + led_period;
  next_collect = micros() + collectrate;
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP to be open
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();
  checkClient();
}
void loop(){
  //------------------------------------------------------------------READY------------------------------------------------------------------------------------------------------------
  Serial.print("state = ");
  Serial.println(state);
  checkClient();  
  if (state == READY){
    digitalWrite(recoverylight, LOW); //Lights and speaker turned off
    digitalWrite(speaker,LOW);
    ready.start();                    //start the timer for the ready state
    ledcWrite(ledChannel, 0);         //Motor turned off
    while (handlebarbuttonvalue == 1 || ready.getTime() <= readyDelay){  //While handlebar is being pulled 
      checkbuttons();
      checkClient();
      simpleCount();
      sBuzz();
      if(zipStop && ready.getTime() >= 2000){  
        break;
      }
    }
    zipStop = false;
    checkbuttons();
    digitalWrite(readylight, HIGH);
    while (handlebarbuttonvalue == 0){    // when the button value is at 0 then the handle is not being pulled and is stationary. 
      checkbuttons();   //check to see when the handlebar is pulled so it can be put into the next state
      checkClient();    
      sBuzz();
      if(state != READY){     //this can happen due to a wifi input change of state. 
        break;
      }
    }
    if(state == READY){
      zipping.start();                        //initiate zipping state and timers 
      state = ZIPPING;
      stateS = "ZIPPING";
      digitalWrite(ziplight, HIGH);           //turn on the zipping light
    }
    digitalWrite(readylight, LOW);            //Turn of Ready indicator light
  }
//-------------------------------------------------------------------ZIPPING-----------------------------------------------------------------------------------------------------------------------
  else if (state == ZIPPING){
    startRot();
    maxRot = 0;
    while (zipping.getTime() <= 2000){
      checkClient();
      simpleCount();
      checkStall();
      sBuzz();
    }
    while (handlebarbuttonvalue == 1){        //changed HBBV from 0 to 1
      checkbuttons();
      checkClient();
      simpleCount();
      checkStall();
      sBuzz();
    }
    state = RECOVERY;
    stateS = "RECOVERY";
    digitalWrite(ziplight,LOW);      //turn off the ziplight when let go
    digitalWrite(recoverylight, HIGH);     // turn on the recoveryled before the delay for motor on. will come on at the top if double pressed.
    digitalWrite(speaker, HIGH);
    Serial.print("time spent zipping: "), Serial.println(zipping.getTime());
    previousZip = (zipping.getTime()/1000);
    totalZip += (zipping.getTime()/1000);
    getRot();
    runs++;
    delay(2000);
    recovery.start();
    while(recovery.getTime() <= recoveryDelay){
      checkbuttons();
      checkClient();
      if(handlebarbuttonvalue == 1){
        state = READY;
        stateS = "READY";
        digitalWrite(recoverylight, LOW);
        digitalWrite(speaker, LOW);
        zipStop = true;
      }
    }
    trigger = false;
    setTime = false;
  }
//--------------------------------------------------------------------RECOVERY--------------------------------------------------------------------------------------------------------
  else if (state == RECOVERY){         //Theres 3 stages to recovery, speed up the motor, go at full speed for a time (depending on how long they zipped), and finally go to half power untill its stopped.
    digitalWrite(speaker,LOW);
    for (int i = 55; i <= maxSpeed; i++){     //this speeds up the motor slowly-------------- if want to start at a higher voltage then set i = larger num
      ledcWrite(ledChannel, i);
      checkbuttons();
      checkClient();
      simpleCount();
      if(i >= 130){
        checkStall();
        if (trigger){
          trigger = false;
          state = READY;
          stateS = "READY";
          irStop++;
        }        
      }
      if(state != RECOVERY){
        break;
      }
      if (handlebarbuttonvalue == 1){     //changed HBBV from 0 to 1
        break;
      }
      reDelay.start();
      while(reDelay.getTime() <= 15){
        checkbuttons();
        checkClient();
        simpleCount();
        if(i >= 130){
          checkStall();
          if (trigger){
          trigger = false;
          state = READY;
          stateS = "READY";
          irStop++;
          }
        }
        if (handlebarbuttonvalue == 1){     //changed HBBV from 0 to 1
          state = READY;
        }
      }
    } 
    while(handlebarbuttonvalue == 0){    //changed HBBV from 1 to 0
      checkbuttons();
      checkClient();
      simpleCount();
      checkStall();
      if (trigger){
        trigger = false;
        state = READY;
        stateS = "READY";
        irStop++;
      }
      if(state != RECOVERY){
        break;
      }
    }    
    ledcWrite(ledChannel, 0);             //stop motor
    Serial.println("motor is off");
    digitalWrite(recoverylight, LOW);      //turn the recoverylight off
    digitalWrite(speaker, LOW);
    state = READY;
    stateS = "READY";
  }
  else if (state == MANUAL){
    checkbuttons();
    while(handlebarbuttonvalue == 0){
      checkClient();
      checkbuttons();
      Serial.println("im here");
    }
    state = READY;
    stateS = "READY";
    ledcWrite(ledChannel, 0);
    Serial.println("stopped");
  }
}

void test(){
  simpleCount();
  checkStall();
}