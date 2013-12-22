#include <Ports.h>
#include <RF12.h>
#include <TagExchange.h>
#include <MyTags.h>

// version 3 of the energy monitor changes to the TagExchange Format


/* Schetch to measure energy consumption/utility meters and send results out as
** tags over the rf12 network.
**
** Frank van de Pol
** 16 November 2012
**
*/

#define NODE_ID 1
#define GROUP_ID 235 


#define SERIAL_BAUD 57600


#define DEBUG_INTERVAL 20  /* in 0.1 s */
#define LIFESIGN_INTERVAL  (300 *10)  /* in 0.1 s */

#define UTILITY_FEEDBACKLED


#define REPORT_INTERVAL_MIN ( 10 *1000UL)   /* minimum time between reporting values */
#define REPORT_INTERVAL_MAX (180 *1000UL)   /* maximum time between reporting values */


enum { 
  TASK_DEBUG, 
  TASK_LIFESIGN,
  TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);

TagExchangeRF12 tagExchangeRF12;




  // Watt Meter
static unsigned long e_start_ms = 0;
static unsigned long e_end_ms = 0;
static unsigned long e_start_count = 0;
static unsigned long e_end_count = 0;  


  // Gas Meter
static unsigned long g_start_ms = 0;
static unsigned long g_end_ms = 0;
static unsigned long g_start_count = 0;
static unsigned long g_end_count = 0;  





#define UTILITY_FEEDBACKLED

class UtilityCounter : 
public Port {

public:
  // constructor
  UtilityCounter (uint8_t port);

  // set parameters for accurate pulse detection
  // for a pulse to be detected the measured value should be for at least 'ms' milliseconds
  // below the 'min_val' value. A measured value above 'max_val' will reset the pulse condition.
  void setTreshold(int min_val, int max_val, int ms);

  // call this continuously or at least right after a pin change
  // returns true if a pulse has been detected
  boolean poll(void);    

  // reset acquisition statistics
  void resetStats(void);

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


  // set total number of pulses
  void setCount(unsigned long newcount) 
  { 
    count = newcount; 
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
  resetStats();
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
    afterglow.set(10);  // switch off LED after 10ms
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




// initialise acquisition statisitics
void UtilityCounter::resetStats(void) 
{
  sensor_min = 1024;  
  sensor_max = 0;
}




static unsigned long timestamp;

UtilityCounter CounterE(1);  // electricity consumption
UtilityCounter CounterG(2);  // gas consumption



int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


void showHelp(void) 
{
  Serial.println(F("\nUtility Meter Node [node_energy_monitor_v3.ino]"));  
  
  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());
  Serial.print(F("Network:  "));
  Serial.println(GROUP_ID);
  Serial.print(F("Node ID:  "));
  Serial.println(NODE_ID);
  
//  Serial.println(F("\n"
//      "Available commands:\n"
//      " d  - toggle debug messages\n"
//      " l  - send lifesign message\n"
//      " ?  - show this help\n"
//      "\n"));  
}




void setup() {

  Serial.begin(SERIAL_BAUD);
  showHelp();

  CounterE.setTreshold(400,600,1);    // measured pulse width with scope: flash duration approx 2ms !!!
  CounterG.setTreshold(150,200,100);

  
  rf12_initialize (NODE_ID, RF12_868MHZ, GROUP_ID);
  
  // register callback functions for the tag updates
  tagExchangeRF12.registerFloatHandler(&TagUpdateFloatHandler);
    
  // start transmission of lifesign message
  scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);
    
  // start transmission
  scheduler.timer(TASK_DEBUG, DEBUG_INTERVAL);
  
  
  // set out zero counter values.. this will trigger a restore of last counter  
  tagExchangeRF12.poll(); 
  delay(500);
  tagExchangeRF12.publishFloat(TAG_F_UTILITY_ELECTRICITY_TOTAL, 0.0); 
  tagExchangeRF12.publishFloat(TAG_F_UTILITY_GAS_TOTAL, 0.0);
  
}




// callback function: executed when a new tag update of type float is received
void TagUpdateFloatHandler(int tagid, unsigned long timestamp, float value)
{            
//  Serial.print("Float Update Tagid=");
//  Serial.print(tagid);
//  Serial.print(" Value=");
//  Serial.print(value);             
//  Serial.println();
  
  switch(tagid) {
    case TAG_F_UTILITY_ELECTRICITY_RESET:  // reset E kWh counter to given value 
      Serial.print(F("TAG_F_UTILITY_ELECTRICITY_RESET Received: "));
      Serial.print(value);
      Serial.println();      
      CounterE.setCount(value * 10000.0);
      e_start_ms = millis();
      e_start_count = CounterE.getCount();
      break;

    case TAG_F_UTILITY_GAS_RESET:          // reset G counter to given value
      Serial.print(F("TAG_F_UTILITY_GAS_RESET Received: "));
      Serial.print(value);
      Serial.println();
      CounterG.setCount(value * 100.0);
      g_start_ms = millis();
      g_start_count = CounterG.getCount();
      break;

  }      
}



void loop() {
  
  // handle rf12 communications
  tagExchangeRF12.poll(); 


  // 
  // Use the pulses from the E counter to determine the current energy consumption in Watt
  //
  //          -----------     -----     -----     -----     -----     -----
  //                     |   |     |   |     |   |     |   |     |   |
  //                     v---      v---      v---      v---      v ---
  //    
  //         start()     |         |         |         |         |
  //            |        |         |         |         |         |
  //            V        V         V         V         V         V
  //          
  //start_ms    0      millis     -         -         -         -         OLD XXX OLD
  //pulse_ms    n/a      0      millis-ms_start                           OLD XXX OLD
  //pulse_cnt   n/a      0      pulse_cnt++                               OLD XXX OLD

  // according to meter specification the LED blinks 10.000 times per kWh
  // each blink is 0.1 Wh;  One hour has 3600x1000 = 3.6E6 milliseconds
  


  //start_ms    0      millis     -         -         -         -         
  //start_cnt   0       cnt  
  //pulse_ms    0        0    millis-ms_start  
  //pulse_cnt   0        0    cnt-start_cnt
  
  // next cycle
  //
  //start_ms    old_ms            -         -         -         -         
  //start_cnt   old_cnt  cnt  
  //pulse_ms    0        0    millis-ms_start  
  //pulse_cnt   0        0    cnt-start_cnt
  
  
  if (CounterE.poll()) {
    // Serial.println ("Port E Pulse Detected");
    if ((e_start_count == 0) || (e_start_count > e_end_count) || (e_start_ms > e_end_ms)) {
      // initialise
      //Serial.println ("INIT E");

      e_start_ms = millis();
      e_start_count = CounterE.getCount();
      e_end_ms = e_start_ms;
      e_end_count = e_start_count;
    } else {
      e_end_ms = millis();
      e_end_count = CounterE.getCount();
    }   
  }


  if (CounterG.poll()) {  
    //Serial.println ("Port G Pulse Detected");        
    if ((g_start_count == 0) || (g_start_count > g_end_count) || (g_start_ms > g_end_ms)) {
      // initialise
      //Serial.println ("INIT G");

      g_start_ms = millis();
      g_start_count = CounterG.getCount();
      g_end_ms = g_start_ms;
      g_end_count = g_start_count;
    } else {
      g_end_ms = millis();
      g_end_count = CounterG.getCount();
    }
  }
  


  // if no or only one pulse have been detected during MAX interval report the available data anyway
  unsigned long cur_ms = millis();
  if (cur_ms > e_start_ms + REPORT_INTERVAL_MAX) {
    e_end_ms = cur_ms;
    e_end_count = CounterE.getCount();
  }
  if (cur_ms > g_start_ms + REPORT_INTERVAL_MAX) {
    g_end_ms = cur_ms;
    g_end_count = CounterG.getCount();
  }
 
  
  // show electricity data if reporting interval is reached
  if (e_end_ms - e_start_ms > REPORT_INTERVAL_MIN) {
    
    // e-power (watts)
    if ((e_end_count >= e_start_count) && (e_end_ms > e_start_ms)) {

      Serial.print("SENSOR E: \t");
      Serial.print(CounterE.getCount() / 10000.0);
      Serial.print("\n");
    
      // publish output tags 
      tagExchangeRF12.publishFloat(
        TAG_F_UTILITY_ELECTRICITY_TOTAL, 
        CounterE.getCount() / 10000.0);  // total electricty consumption in kWh

      float watts = (e_end_count - e_start_count) * 0.1 * 3.6E6 / (e_end_ms - e_start_ms);
    
      tagExchangeRF12.publishFloat(
          TAG_F_UTILITY_ELECTRICITY_ACTUAL, 
          watts);    // actual electricity consumpty in W

      Serial.print("ACTUAL E: \t");
      Serial.print(watts);
      Serial.print(" W\n");
      
      // init for next cycle      
      e_start_ms = e_end_ms;
      e_start_count = e_end_count;
    }
  }

  // show gas data if reporting interval is reached
  if (g_end_ms - g_start_ms > REPORT_INTERVAL_MIN) {
    
    // g-power (l/h)
    if ((g_end_count >= g_start_count) && (g_end_ms > g_start_ms)) {

      Serial.print("SENSOR G: \t");
      Serial.print(CounterG.getCount() / 100.0);
      Serial.print("\n");
    
      // publish output tags 
      tagExchangeRF12.publishFloat(
          TAG_F_UTILITY_GAS_TOTAL, 
          CounterG.getCount() / 100.0);    // total gas consumption in M3
      
      float flow = (g_end_count - g_start_count) * 10 * 3.6E6 / (g_end_ms - g_start_ms);

      tagExchangeRF12.publishFloat(
          TAG_F_UTILITY_GAS_ACTUAL, 
          flow);    // actual gas flow in l/h
     
      Serial.print("ACTUAL G: \t");
      Serial.print(flow);
      Serial.print(" l/h\n");      
      
      // init for next cycle      
      g_start_ms = g_end_ms;
      g_start_count = g_end_count;
    }
  }

 

  switch (scheduler.poll()) {


      // running at low frequency (once per 5 minutes) to send out counter values even in case no gas consumption has taken place
    case TASK_DEBUG:
  
      // reschedule
    scheduler.timer(TASK_DEBUG, DEBUG_INTERVAL);    

/*    
    Serial.print("Debug E \t");
    Serial.print(e_end_ms);
    Serial.print("\t");
    Serial.print(e_start_ms);
    Serial.print("\t");
    Serial.print(e_start_count);
    Serial.print("\t");
    Serial.print(e_end_count);
    Serial.print("\t");
    Serial.print(e_end_count - e_start_count);
    Serial.println("");
*/


/*
    Serial.print("Debug G \t");
    Serial.print(cur_ms);
    Serial.print("\t");    
    Serial.print(g_start_ms + REPORT_INTERVAL_MAX);    
    Serial.print("\t");    
    Serial.print(g_start_ms);
    Serial.print("\t");
    Serial.print(g_end_ms);
    Serial.print("\t");
    Serial.print(g_end_ms - g_start_ms);
    Serial.print("\t\t");
    Serial.print(g_start_count);
    Serial.print("\t");
    Serial.print(g_end_count);
    Serial.print("\t");
    Serial.print(g_end_count - g_start_count);
    Serial.println("");
*/
    
    break;
      

  
  
      case TASK_LIFESIGN:
//      if (debug_txt) {  
        Serial.println(F("Send Lifesign message"));
//      }

//      BlinkSerialLed();
//      BlinkRF12Led();
    
      tagExchangeRF12.publishLong(100 + NODE_ID ,  millis());
      
      scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);
      break;
  }
  
  
  
  
}







