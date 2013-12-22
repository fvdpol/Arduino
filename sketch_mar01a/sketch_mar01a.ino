#include <Ports.h>
#include <RF12.h>


#define NODE_ID 3
#define GROUP_ID 212



// fist attempt at clock function

Port one (1);

enum { 
  TASK1, TASK2, TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);



// timestamp is time in unix format, seconds since 1 jan 1970.



MilliTimer RepTimer;
static unsigned long timestamp;
static int ticksPerSecond;  // number of ticks for one 'real' second
                            // if the AVR clock is exactly 16MHz this would be 1000
                            // but can be higher or lower to compensate for clock
                            // deviations.


static boolean dir;

void setup() {               
    Serial.begin(57600);
    Serial.print("\n[test sketch]\n");
  
  one.mode(OUTPUT);
  
  scheduler.timer(TASK1, 1);
  scheduler.timer(TASK2, 1);
    
  ticksPerSecond = 1000;
  
  dir = 0;
  
  
    ticksPerSecond = 300;
}



void loop() {
    rf12_easyPoll();
    
    if (RepTimer.poll(ticksPerSecond)) {
         timestamp++;    
         
         scheduler.timer(TASK1, 1);
    }
    
    
  switch (scheduler.poll()) {
    // LED 1 blinks regularly, once a second
  case TASK1:
    //scheduler.timer(TASK1, 10);
  
    //    flash led;  
    one.digiWrite(1);
    delay(5);     
    one.digiWrite(0);
    
    
    if (dir) {
      ticksPerSecond++;
    } else {
      ticksPerSecond--;
    }
    
    if (ticksPerSecond > 500) {
      dir = 0;
    }
    
    if (ticksPerSecond < 5) {
      dir = 1;
    }
    
    break;
    

  case TASK2:
    scheduler.timer(TASK2, 10);

    Serial.print(timestamp);
    Serial.print(" ");
    Serial.print(ticksPerSecond);
    Serial.print("\n");
    break;

  }
    
}
