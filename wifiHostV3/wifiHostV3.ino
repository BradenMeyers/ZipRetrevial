// Load Wi-Fi library
#include <WiFi.h>
#include <Preferences.h> 
//States
#define READY 1
#define ZIPPING 2
#define RECOVERY 3
#define MANUAL 4

class Timer{
  
};

int readyDelay = 750;
int menu = 1;
int currentIr = 7;
//int secret = 0;
int handlebarbuttonvalue = 0; //changed from 1 to 0
int state = READY;
int maxSpeed = 195;
int irStop = 0;     //counter for how many times the ir lights stop the pulley
bool zipStop = false;
unsigned long totalZip = 0;

unsigned long rotStart = 0;
int previousRot = 0;
double previousZip = 0;
int maxRot = 0;
int runs = 0;
int maxRotR = 0;

//Pin assignments
int motor = 21;
int handlebarbutton = 14;
int readylight = 32;
int ziplight = 25;        
int recoverylight = 26;    
const int AIN = 34;       // For photodiode
const int LED_OUT = 19;
const int direction = 22;
const int speaker = 33;

//timers throughout states
unsigned long currenttime;
unsigned long aftertime;
unsigned long clocktime;

//PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

//Odometer values
int checkStallCount = 3;
unsigned long pastRot = 0;
bool setTime = true;
unsigned long pastTime = millis();
int freqSpeed = 500;
long pulseDifference = 0;
bool trigger = false;

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
unsigned long pulses = 0;

// 2 crossing trigger.
double upper_threshold = 2500; // TODO get real values
double lower_threshold = 1000; // TODO get real values
bool crossed_upper = false;
double lock_in_data = 0;

// WIFI variables
// Replace with your network credentials
const char* ssid     = "ESP32-Access-Point";
const char* password = "123456789";
// Set web server port number to 80
WiFiServer server(80);
// Variable to store the HTTP request
String header;

// html variables
String directionState = "off";
String output28State = "auto";
String directionString = "Forward";
String stateS = "READY";
String IRstring = "0";
//slider variables
int lower1 = 0;   //lower threshold
int lower2 = 0;
String lowerThreshold = String(lower_threshold);
int upper1 = 0;    //upper threshold
int upper2 = 0;
String upperThreshold = String(upper_threshold);
int inputS1 = 0;    //checkStallCount
int inputF1 = 0;
String inputStringUno;
int pos1 = 0;       //motor Speed
int pos2 = 0;
String valueString = String(0);
int max1 = 0;       //max motor speed
int max2 = 0;
String maxString = String(maxSpeed);
//html timers
unsigned long Time = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

//FUNCTIONS
Preferences preferences;
void starttime(){    //start time and get time are functions for monitoring the time in the states starttime will start the timer and get time will check the time.
  currenttime = millis();
}
void gettime(){
  aftertime = millis();
  clocktime = aftertime - currenttime;
}
void checkbuttons(){
  handlebarbuttonvalue = digitalRead(handlebarbutton);
}
void startRot(){
  rotStart = pulses;
}
void getRot(){
  previousRot = pulses - rotStart;
}
void checkStall(){           //check stall is counting the difference in the pulses from one time to another. changes bool-trigger to true when not enough rotations happen
  if (setTime == false){        //only will happen on the initialization of the function
      pastTime = millis();
      pastRot = pulses;
      setTime = true;
      //Serial.println("set time");
  }
  if ((millis() - pastTime) >= freqSpeed){
      pulseDifference = (pulses - pastRot);
      Serial.println(pulseDifference);
      if(state == ZIPPING){
        if(pulseDifference > maxRot){
          maxRot = pulseDifference;
        }
      }
      if(state == RECOVERY){
        if(pulseDifference > maxRotR){
          maxRotR = pulseDifference;
        }
      }
      if (pulseDifference < checkStallCount){
          trigger = true;
          readyDelay = 2500;
          Serial.println("trigger on");
      }
      setTime = false;
      //Serial.print("trueee");
  }
}
void simpleCount(){           //simple version of cuncans code. checking to see if value is going over upper threshold and below lower threshold
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
void spinCount()
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
void checkClient(){                         //THIS FUNCTION HAS ALL THE WIFI STUFF
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    Time = millis();
    previousTime = Time;
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()  && Time - previousTime <= timeoutTime) {            // loop while the client's connected
      Time = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
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
            else if (header.indexOf("GET /72/off") >= 0) {
              Serial.println("Spencer");
              digitalWrite(speaker, HIGH);
            }
            else if (header.indexOf("GET /menu/1") >= 0) {
              menu = 1;
            } 
            else if (header.indexOf("GET /menu/2") >= 0) {
              menu = 2;
            }
            else if (header.indexOf("GET /menu/3") >= 0) {
              menu = 3;
            }
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
            //lower threshold slider
            if(header.indexOf("GET /?value2=")>=0) {
              lower1 = header.indexOf('=');
              lower2 = header.indexOf('&');
              lowerThreshold = header.substring(lower1+1, lower2);
              lower_threshold = lowerThreshold.toInt();
              Serial.print("Lower Threshold: ");
              Serial.println(lowerThreshold);
              preferences.putUInt("lower", lower_threshold);
            }
            //upper threshold slider
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

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
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
              client.println("<p><a href=\"/menu/3\"><button class=\"button button4\">MENU</button></a></p>");
              //client.println("<script> document.getElementById(\"inputTex\").onsubmit = function(){var x = document.getElementById(\"input1\"); ");
            }
            if(menu == 3){
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
            //THE REST OF THIS JUST RUNS THE PAGE DONT KNOW WHAT IT MEANS
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
  preferences.begin("my-app", false);
  maxSpeed = preferences.getUInt("max", 195);
  upper_threshold = preferences.getUInt("upper", 2500);
  lower_threshold = preferences.getUInt("lower", 1000);
  checkStallCount = preferences.getUInt("stall", 2);
  maxString = String(maxSpeed);
  upperThreshold = String(upper_threshold);
  lowerThreshold = String(lower_threshold);
  Serial.begin(115200);
  state = READY;
  analogReadResolution(12);     //Analog Read values up to 4095
  pinMode(handlebarbutton, INPUT_PULLUP);
  pinMode(readylight, OUTPUT);
  pinMode(ziplight, OUTPUT);    //set the leds to output
  pinMode(recoverylight, OUTPUT);
  pinMode(speaker,OUTPUT);
  ledcSetup(ledChannel, freq, resolution);
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(motor, ledChannel);
  ledcWrite(ledChannel, 0);
  // Initialize the output variables as outputs
  pinMode(direction, OUTPUT);
  digitalWrite(direction, LOW);
  pinMode(LED_OUT,OUTPUT);
  next_toggle = micros() + led_period;
  next_collect = micros() + collectrate;
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();
  checkClient();
}
void loop(){
  //checkClient();
  //------------------------------------------------------------------READY------------------------------------------------------------------------------------------------------------
  Serial.print("state = ");
  Serial.println(state);
  checkClient();
  if (state == READY){
    digitalWrite(recoverylight, LOW);
    digitalWrite(speaker,LOW);
    starttime();
    gettime();
    ledcWrite(ledChannel, 0);
    while (handlebarbuttonvalue == 1 || clocktime <= readyDelay){
      checkbuttons();
      gettime();
      checkClient();
      simpleCount();
      if(zipStop && clocktime >= 2000){
        
        break;
      }
    }
    zipStop = false;
    checkbuttons();
    digitalWrite(readylight, HIGH);
    while (handlebarbuttonvalue == 0){    // changed HBBV from 1 to 0
      checkbuttons();
      checkClient();
      if(state != READY){
        break;
      }
    }
    if(state == READY){
      starttime();
      gettime();
      state = ZIPPING;
      stateS = "ZIPPING";
      digitalWrite(ziplight, HIGH);           //turn on the zipping light
    }
    digitalWrite(readylight, LOW);
  }
//-------------------------------------------------------------------ZIPPING-----------------------------------------------------------------------------------------------------------------------
  else if (state == ZIPPING){
    startRot();
    maxRot = 0;
    while (clocktime <= 2000){
      checkClient();
      gettime();
      simpleCount();
      checkStall();
    }
    while (handlebarbuttonvalue == 1){        //changed HBBV from 0 to 1
      checkbuttons();
      checkClient();
      simpleCount();
      checkStall();
    }
    gettime();
    state = RECOVERY;
    stateS = "RECOVERY";
    digitalWrite(ziplight,LOW);      //turn off the ziplight when let go
    digitalWrite(recoverylight, HIGH);     // turn on the recoveryled before the delay for motor on. will come on at the top if double pressed.
    digitalWrite(speaker, HIGH);
    Serial.print("time spent zipping: "), Serial.println(clocktime);
    previousZip = (clocktime/1000);
    totalZip += (clocktime/1000);
    getRot();
    runs++;
    starttime();
    gettime();
    delay(2000);
    while(clocktime <= 3000){
      gettime();
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
      starttime();
      gettime();
      while(clocktime <= 15){
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
        gettime();
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