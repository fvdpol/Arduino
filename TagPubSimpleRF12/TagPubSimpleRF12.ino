#include <JeeLib.h>
#include <TagExchange.h>


#define NODE_ID 6
/*#define GROUP_ID 212 */
#define GROUP_ID 235 

#define TRANSMIT_INTERVAL 150  /* in 0.1 s */



enum { 
  TASK_TRANSMIT,
  TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);

TagExchangeRF12 tagExchangeRF12;


int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


void setup(void) {
  Serial.begin(57600);
  Serial.print(F("\nReception Test Node [TabPubSimpleRF12.ino]\n"));
  
  
  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());
  Serial.print(F("Network:  "));
  Serial.println(GROUP_ID);
  Serial.print(F("Node ID:  "));
  Serial.println(NODE_ID);
  
  
  
  rf12_initialize (NODE_ID, RF12_868MHZ, GROUP_ID);
  

  // test transmission
  scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
  
  // register callback functions for the tag updates
  tagExchangeRF12.registerFloatHandler(&TagUpdateFloatHandler);
  tagExchangeRF12.registerLongHandler(&TagUpdateLongHandler);
}  


// callback function: executed when a new tag update of type float is received
void TagUpdateFloatHandler(int tagid, unsigned long timestamp, float value)
{         
  Serial.print("Float Update Tagid=");
  Serial.print(tagid);
              
  Serial.print(" Value=");
  Serial.print(value);
               
  Serial.println();
}


// callback function: executed when a new tag update of type long is received
void TagUpdateLongHandler(int tagid, unsigned long timestamp, long value)
{
  Serial.print("Long Update Tagid=");
  Serial.print(tagid);
              
  Serial.print(" Value=");
  Serial.print(value);
               
  Serial.println();
}





void loop(void) {
    // handle rf12 communications
    tagExchangeRF12.poll();
 
  
  switch (scheduler.poll()) {

  case TASK_TRANSMIT:
     // reschedule every 15s
      
      //tagExchangeRF12.publishFloat(10, 1.0 * millis());
      //tagExchangeRF12.publishLong(11,  millis());
      
      // transmission is handled by queueing mechanism. if we don't want to wait we can trigger the sending ourselves.
      //tagExchangeRF12.publishNow();
      
      scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
    break;
    
  }

}    


