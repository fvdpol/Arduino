#include <Ports.h>
#include <RF12.h>

class UtilityCounter : public Port {
    
public:
    enum { ALL_OFF, ON1, OFF1, ON2, OFF2, SOME_ON, ALL_ON }; // for buttonCheck
    
    // constructor
    UtilityCounter (uint8_t port);
    
    void ledOn(byte mask);
    void ledOff(byte mask);
    byte state();
    byte pushed(); // deprecated, don't use in combination with buttonCheck
    byte buttonCheck();
    
private:
    MilliTimer debounce;
    byte leds, lastState, checkFlags;

};


// constructor
UtilityCounter::UtilityCounter (uint8_t port): Port (port)
{
}



enum { TASK1, TASK2, TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);

Port CounterA (1);
Port CounterB (2);

MilliTimer RepTimer;
MilliTimer Led1Timer;
MilliTimer Led2Timer;

byte led1;
byte led2;


void toggle1() {
  if (led1 == 0) {
    led1 =1;
  } else {
    led1 = 0;
  }
   CounterA.digiWrite(led1);   
}

void toggle2() {
  if (led2 == 0) {
    led2 =1;
  } else {
    led2 = 0;
  }
   CounterB.digiWrite(led2);   
}


void setup() {
  Serial.begin(57600);
  Serial.print("\n[meter_monitor_v1 sketch]\n");

  CounterA.mode(OUTPUT);
  CounterB.mode(OUTPUT);

  CounterA.digiWrite(0);
  CounterB.digiWrite(0);

  CounterA.mode2(INPUT); // Set AIO mode as input
  CounterA.digiWrite2(1); // Activate pull-up resistor for AIO

  CounterB.mode2(INPUT); // Set AIO mode as input
  CounterB.digiWrite2(1); // Activate pull-up resistor for AIO

  rf12_config();
  //rf12_config(); // Yes, twice - see http://talk.jeelabs.net/topic/169
  rf12_easyInit(3); // Send value at most every 3 seconds

   // start both tasks 1.5 seconds from now
    scheduler.timer(TASK1, 15);
    scheduler.timer(TASK2, 15);
}

void loop() {
  int dataA = CounterA.anaRead();
  int dataB = CounterB.anaRead();
  
  static int dataAmin;
  static int dataAmax;
  static int dataBmin;
  static int dataBmax;
  
  if (dataA < dataAmin) { dataAmin = dataA;};
  if (dataA > dataAmax) { dataAmax = dataA;};
  if (dataB < dataBmin) { dataBmin = dataB;};
  if (dataB > dataBmax) { dataBmax = dataB;};
  
  static int loops_per_sec;

  loops_per_sec++;

  rf12_easyPoll();

  if (RepTimer.poll(1000)) {
    Serial.print("loop...\n");
    /*Serial.print(dataA);*/
    
    Serial.print(dataAmin);
    Serial.print(" .. ");
    Serial.print(dataAmax);
    Serial.print("\n");
    
    /*Serial.print(dataB);*/
    Serial.print(dataBmin);
    Serial.print(" .. ");
    Serial.print(dataBmax);
    Serial.print("\n");
    
    Serial.println(loops_per_sec);
    loops_per_sec = 0;
    dataAmin = 1024;
    dataAmax = 0;
    dataBmin = 1024;
    dataBmax = 0;
  }

  if (Led1Timer.poll(100)) {
/*    toggle1(); */
  }

  if (Led2Timer.poll(101)) {
/*    toggle2(); */
  }
  
  
      switch (scheduler.poll()) {
        // LED 1 blinks regularly, once a second
        case TASK1:
            toggle1();
            scheduler.timer(TASK1, 5);
            break;
        // LED 2 blinks with short on pulses and slightly slower
        case TASK2:
            toggle2();
            scheduler.timer(TASK2, 6);
            break;
    }
}




