#include <Ports.h>
#include <RF12.h>

#define NODE_ID 2
#define GROUP_ID 212


#define UTILITY_FEEDBACKLED

class UtilityCounter : 
public Port {

public:
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



typedef struct {
  // header
  unsigned long timestamp;
  byte msg_src;
  byte msg_dest;
  byte msg_id;
  byte msg_len;

  // payload  
  unsigned long watts;    // actual electricity consumption
  unsigned long e_count;  // electricity counter
  unsigned long g_count;  // gas counter 
} 
Packet_t;


enum { 
  TASK1, TASK2, TASK_REPORT, TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);
static unsigned long timestamp;

UtilityCounter CounterE (1);  // electricity consumption
UtilityCounter CounterG (2);  // gas consumption

MilliTimer RepTimer;


void setup() {
  Serial.begin(57600);
  Serial.print("\n[meter_monitor_v1 sketch]\n");

  CounterE.setTreshold(400,600,1);    // measured pulse width with scope: flash duration approx 2ms !!!
  CounterG.setTreshold(150,200,100);

  rf12_initialize (NODE_ID, RF12_868MHZ, GROUP_ID);
  //rf12_config();
  rf12_easyInit(3); // Send value at most every 3 seconds

  // start both tasks 1.5 seconds from now
  scheduler.timer(TASK1, 15);
  scheduler.timer(TASK2, 15);

  scheduler.timer(TASK_REPORT, 1);    
}



void loop() {

  // Watt Meter
  static unsigned long start_ms;
  static unsigned long pulse_ms;
  static int pulse_count;
  static long watts; 

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
  //start_ms    0      millis     -         -         -         -         
  //pulse_ms    n/a      0      millis-ms_start  
  //pulse_cnt   n/a      0      pulse_cnt++   

  // according to meter specification the LED blinks 10.000 times per kWh
  // each blink is 0.1 Wh;  One hour has 3600x1000 = 3.6E6 milliseconds


    if (CounterE.poll()) {
    //Serial.println ("Port E Pulse Detected");


    if (start_ms == 0) {
      // initialise
      start_ms = millis();
      pulse_ms = 0;
      pulse_count = 0;
      watts = 0;
    } 
    else {
      pulse_ms = millis() - start_ms;
      pulse_count ++;
      watts = pulse_count * 0.1 * 3.6E6 / pulse_ms;
    }
  }


  if (CounterG.poll()) {
    Serial.println ("Port G Pulse Detected");
    scheduler.timer(TASK_REPORT, 0); 
  }


  static int loops_per_sec;
  loops_per_sec++;
  // approx

  rf12_easyPoll();


#if 0
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

    CounterE.resetStats();
    CounterG.resetStats();
  }
#endif


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

  case TASK_REPORT:

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


    Serial.print("SENSOR W: \t");
    Serial.print(watts);
    Serial.print("\t");
    Serial.print(pulse_count);
    Serial.print("\t");
    Serial.print(pulse_ms);
    Serial.print("\n");

    // send data
    { 
      Packet_t packet;

      packet.timestamp = millis();
      packet.msg_src = 1;  // this node
      packet.msg_dest = 1;
      packet.msg_id = 1;
      packet.msg_len = 1;

      packet.watts = watts;                  // actual electricity consumption
      packet.e_count = CounterE.getCount();  // electricity counter
      packet.g_count = CounterG.getCount();  // gas counter 

      rf12_easySend(&packet, sizeof packet);


// sample data received from jeenode      
// OK 34 179 21 7 0 1 1 1 1 171 2 0 0 72 3 0 0 2 0 0 0
//  -> ack
//
// OK 34 195 60 7 0 1 1 1 1 173 2 0 0 91 3 0 0 2 0 0 0
//  -> ack
// OK 34 211 99 7 0 1 1 1 1 175 2 0 0 110 3 0 0 2 0 0 0
//  -> ack
//
// OK 34 67 117 8 0 1 1 1 1 173 2 0 0 243 3 0 0 2 0 0 0
//       TTTTTTTTTT S D I L WWWWWWWWW EEEEEEEEE GGGGGGG
// |  |
// |  \- rf12_hdr ???
// |
// \---- rf12_crc = 0 = OK
//
//
//#define RF12_HDR_CTL    0x80
//#define RF12_HDR_DST    0x40
//#define RF12_HDR_ACK    0x20
//#define RF12_HDR_MASK   0x1F  --> nodeid 0..31


    }

    // cause re-initialisation of Watt meter
    start_ms = 0;

    CounterE.resetStats();
    CounterG.resetStats();

    // reschedule
    scheduler.timer(TASK_REPORT, 100);    
  }
}







