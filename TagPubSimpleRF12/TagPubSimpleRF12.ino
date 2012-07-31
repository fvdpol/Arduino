#include <JeeLib.h>
#include <TagExchange.h>


#define NODE_ID 6
/*#define GROUP_ID 212 */
#define GROUP_ID 235 

#define TRANSMIT_INTERVAL 150  /* in 0.1 s */




enum { 
  TASK1, TASK2, 
  TASK_MEASURE,
  TASK_PROCESS,
  TASK_TRANSMIT,
  TASK_REPORT, 
  TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);



TagExchangeRF12 tagExchangeRF12;




void setup(void) {
  Serial.begin(57600);
  Serial.print("\nReception Test Node\n");
  
  
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
    

  case TASK_TRANSMIT:
     // reschedule every 15s
      
      tagExchangeRF12.publishFloat(10, 1.0 * millis());
      tagExchangeRF12.publishLong(11,  millis());
      
      // queueing mechanism not yet implemented; so we trigger the sending ourselves.
      tagExchangeRF12.publishNow();
      

      scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
    break;
    
  }

}    






  
