#include <Ports.h>
#include <RF12.h>

#define UTILITY_FEEDBACKLED

class UtilityCounter : 
public Port {

public:
  enum { 
    ALL_OFF, ON1, OFF1, ON2, OFF2, SOME_ON, ALL_ON             }; // for buttonCheck

  // constructor
  UtilityCounter (uint8_t port);

  void ledOn(byte mask);
  void ledOff(byte mask);
  byte state();
  byte pushed(); // deprecated, don't use in combination with buttonCheck
  byte buttonCheck();

  // set parameters for accurate pulse detection
  // for a pulse to be detected the measured value should be for at least 'ms' milliseconds
  // below the 'min_val' value. A measured value above 'max_val' will reset the pulse condition.
  void setTreshold(int min_val, int max_val, int ms);

  // call this continuously or at least right after a pin change
  // returns true if a pulse has been detected
  boolean poll(void);    

  // start measuring
  void start(void);

  // retrieve statistics from data acquisition
  int getSensorMin(void) const
  { 
    return sensor_min; 
  }

  int getSensorMax(void) const
  { 
    return sensor_max; 
  }

  // total number of pulses counted since startup
  unsigned long getCount(void) const
  { 
    return count; 
  }



private:
  // config parameters
  int treshold_min;
  int treshold_max;
  int treshold_ms;

  // runtime data
  int sensor_min;
  int sensor_max;
  boolean pulse;
  unsigned long count;

  MilliTimer debounce;    // ensure minimum pulse duration is met
#ifdef UTILITY_FEEDBACKLED  
  MilliTimer afterglow;   // keep feedback lit for xx ms after end of pulse 
#endif
};


// constructor
UtilityCounter::UtilityCounter (uint8_t port): 
Port (port)
{ 
  // initialise variables
  treshold_min = 512;
  treshold_max = 512;
  treshold_ms  = 5;  // default 5 milliseconds to debounce

  pulse = false;
  count = 0;

  // setup i/o pins
#ifdef UTILITY_FEEDBACKLED  
  mode(OUTPUT);  // digital pin = output for feedback LED
  digiWrite(0);  // switch off LED
#endif

  mode2(INPUT);  // Set AIO mode as input
  digiWrite2(1); // Activate pull-up resistor for AIO

  // initialise acquisition statisitics
  start();
}


boolean UtilityCounter::poll(void)
{
  // read sensor value; because of pull-up this would normally read 1024 for OC
  // if pulse detected the value will be pulled towards 0 by switch/phototransistor
  int sensor = anaRead();  
  int pulse_detected = false;

  // keep track of highest/lowest value read from the ADC
  if (sensor < sensor_min) { 
    sensor_min = sensor;
  }
  if (sensor > sensor_max) { 
    sensor_max = sensor;
  }

  // detect pulses
  if ((pulse == false) && (sensor < treshold_min)) {
    if (debounce.idle()) {
      // potential pulse detected; start timer to ensure pulse is long enough
      debounce.set(treshold_ms);
    }
    if (debounce.poll()) {            
      // we detected a pulse!
      pulse = true;
      pulse_detected = true;    
      count++;

      //
      //          -----------     -----     -----     -----     -----     -----
      //                     |   |     |   |     |   |     |   |     |   |
      //                     v---      v---      v---      v---      v ---
      //    
      //         start()     |         |         |         |         |
      //            |        |         |         |         |         |
      //            V        V         V         V         V         V
      //          
      //start_ms  millis     -         -         -         -         - 
      //pulse_ms    0      millis-ms_start  
      //pulse_cnt   0      pulse_cnt++   


#ifdef UTILITY_FEEDBACKLED  
      digiWrite(true);  // switch on LED
#endif

    }
  } 
  if (sensor > treshold_min) {
    if (!debounce.idle()) {
      debounce.set(0);
    }
  }

  if ((sensor > treshold_max) && (pulse == true)) {
    pulse = false;    
#ifdef UTILITY_FEEDBACKLED  
    afterglow.set(20);  // switch off LED after 20ms
#endif
  }
  
#ifdef UTILITY_FEEDBACKLED  
  if (afterglow.poll()) {
    digiWrite(false);  // switch off LED
  }
#endif

  return pulse_detected;
}


void UtilityCounter::setTreshold(int min_val, int max_val, int ms) 
{  
  treshold_min = min_val;
  treshold_max = max_val;
  treshold_ms  = ms;
}



// start measuring
void UtilityCounter::start(void) 
{
  sensor_min = 1024;  
  sensor_max = 0;
}





enum { 
  TASK1, TASK2, TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);

UtilityCounter CounterE (1);  // electricity consumption
UtilityCounter CounterG (2);  // gas consumption

MilliTimer RepTimer;


void setup() {
  Serial.begin(57600);
  Serial.print("\n[meter_monitor_v1 sketch]\n");

  CounterE.setTreshold(400,600,1);
  CounterG.setTreshold(150,200,100);

  rf12_config();
  rf12_easyInit(3); // Send value at most every 3 seconds

  // start both tasks 1.5 seconds from now
  scheduler.timer(TASK1, 15);
  scheduler.timer(TASK2, 15);
}

void loop() {
  if (CounterE.poll()) {
    //Serial.println ("Port E Pulse Detected");
  }

  if (CounterG.poll()) {
    Serial.println ("Port G Pulse Detected");
  }

  //CounterB.poll();


  static int loops_per_sec;

  loops_per_sec++;

  rf12_easyPoll();

  if (RepTimer.poll(1000)) {
    //Serial.print("loop...\n");



    Serial.print("SENSOR E: \t");
    Serial.print(CounterE.getCount());
    Serial.print("\t");
    Serial.print(CounterE.getSensorMin());
    Serial.print("\t");
    Serial.print(CounterE.getSensorMax());
    Serial.print("\n");

    Serial.print("SENSOR G: \t");
    Serial.print(CounterG.getCount());
    Serial.print("\t");
    Serial.print(CounterG.getSensorMin());
    Serial.print("\t");
    Serial.print(CounterG.getSensorMax());
    Serial.print("\n");

    Serial.println(loops_per_sec);
    loops_per_sec = 0;

    CounterE.start();
    CounterG.start();
  }



  switch (scheduler.poll()) {
    // LED 1 blinks regularly, once a second
  case TASK1:
    //    toggle1();
    scheduler.timer(TASK1, 5);
    break;
    // LED 2 blinks with short on pulses and slightly slower
  case TASK2:
    //    toggle2();
    scheduler.timer(TASK2, 6);
    break;
  }
}










