const int AIN = 34; // For phototransistor
const int LED_OUT = 19;
const int visible_led = 13;

// General variables
unsigned long nowTime = 0;

// LED flashing
int f_led = 2000; // LED oscilation frequency in Hz //3000
unsigned long led_period = (unsigned long)(1e6/(2*f_led)); // microseconds that it spends in on/off state.
bool LED_STATUS = LOW; // Have it turned off at the start
unsigned long next_toggle = 0; // Toggling the LED

// Lock-in detection / collecting the data
//unsigned long next_avg_time = 0;
unsigned long collectrate = led_period * 32; // Arbitrayily decided 22 times the led period. It just needs to be larger than the led period
unsigned long next_collect = 0;
int num_on_measures = 0;    //long
int num_off_measures = 0;   
unsigned long on_values = 0;
unsigned long off_values = 0;
unsigned long pulses = 0;
const int resolution = 12;

// 2 crossing trigger.
double upper_threshold = 300; // TODO get real values
double lower_threshold = 20; // TODO get real values
bool crossed_upper = false;
int lock_in_data = 0;

void setup()
{
  // make analog read faster
/*  _SFR_BYTE(ADCSRA) |= _BV(ADPS2);
  _SFR_BYTE(ADCSRA) &= ~_BV(ADPS1);
  _SFR_BYTE(ADCSRA) &= ~_BV(ADPS0);
*/
  analogReadResolution(resolution);
  Serial.begin(115200);
  pinMode(LED_OUT,OUTPUT);
  pinMode(visible_led,OUTPUT);

  //nowTime = micros();
  next_toggle = micros() + led_period;
  next_collect = micros() + collectrate;
}

void loop()
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
    delayMicroseconds(5);
    on_values += analogRead(AIN);
    num_on_measures++;
    //Serial.print(on_values);
    //Serial.print("  Measures:");
    //Serial.println(num_on_measures);
  }
  else
  {
    delayMicroseconds(5);
    off_values += analogRead(AIN);
    num_off_measures++;
    //Serial.print(off_values);
    //Serial.print("  Measures:");
    //Serial.println(num_off_measures);
  }

  // Collection and lock-in detection
  if (nowTime > next_collect)
  {
    // Get the lock-in data
    lock_in_data = on_values/num_on_measures - off_values/num_off_measures;
    next_collect += collectrate;
    on_values = 0;
    num_on_measures = 0;
    off_values = 0;
    num_off_measures = 0;

    //Uncomment to display the raw data *****
     Serial.print("Lock in data: ");
     Serial.println(lock_in_data);
    
    // Check if the data was a rotation
    if(crossed_upper)
    {
      if(lock_in_data < lower_threshold)
      {
        digitalWrite(visible_led,LOW);
        //Uncomment to count pulses *****
        //Serial.println(++pulses);
        crossed_upper = false;
      }
    }else if (lock_in_data > upper_threshold)
    {
      digitalWrite(visible_led,HIGH);
      crossed_upper = true;
    }
  }
}
