#include <JeeLib.h>
#include <TagExchange.h>

/* Schetch to relay "TagExchange" Messages between RF12 network and the serial port.
** intended use is the provide a gateway to the jeenodes from a (linux) computer
**
** Frank van de Pol
** 10 November 2012
**
*/



#define NODE_ID 6
#define GROUP_ID 235 

#define LIFESIGN_INTERVAL 600  /* in 0.1 s */

#define SERIAL_BAUD 57600

#define LED_HEARTBEAT     5
#define LED_DATA_SERIAL   6
#define LED_DATA_RF12     7

#define LED_GLOWTIME  1  /* in 0.1s */

enum { 
  TASK_LIFESIGN,
  TASK_LED_HEARTBEAT,
  TASK_LED_SERIAL,
  TASK_LED_RF12,
  TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);

bool debug_txt = false;
int heartbeat_state = 0;

TagExchangeRF12 tagExchangeRF12;
TagExchangeStream tagExchangeSerial(&Serial);


int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


void showHelp(void) 
{
  Serial.println(F("\nTagPubRF12toSerial"));  
  
  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());
  Serial.print(F("Network:  "));
  Serial.println(GROUP_ID);
  Serial.print(F("Node ID:  "));
  Serial.println(NODE_ID);
  
  Serial.println(F("\n"
      "Available commands:\n"
      " d  - toggle debug messages\n"
      " l  - send lifesign message\n"
      " ?  - show this help\n"
      "\n"));  
}


void BlinkSerialLed(void)
{
    digitalWrite(LED_DATA_SERIAL, HIGH);
    scheduler.timer(TASK_LED_SERIAL, LED_GLOWTIME);
}


void BlinkRF12Led(void)
{
    digitalWrite(LED_DATA_RF12,   HIGH);
    scheduler.timer(TASK_LED_RF12, LED_GLOWTIME);
}


void setup(void) 
{
    pinMode(LED_HEARTBEAT,   OUTPUT);
    pinMode(LED_DATA_SERIAL, OUTPUT);
    pinMode(LED_DATA_RF12,   OUTPUT);
    
    digitalWrite(LED_HEARTBEAT,   HIGH);
    digitalWrite(LED_DATA_SERIAL, HIGH);
    digitalWrite(LED_DATA_RF12,   HIGH);
    delay(500);
    digitalWrite(LED_HEARTBEAT,   LOW);
    delay(250);
    digitalWrite(LED_DATA_SERIAL, LOW);
    delay(250);
    digitalWrite(LED_DATA_RF12,   LOW);
    delay(250);
    BlinkSerialLed();
    BlinkRF12Led();
    scheduler.timer(TASK_LED_HEARTBEAT, 1);

  
  Serial.begin(SERIAL_BAUD);
  showHelp();
    
  rf12_initialize (NODE_ID, RF12_868MHZ, GROUP_ID);
  
  // start transmission of lifesign message
  scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);
  
  
  
  // register callback functions for the tag updates
  tagExchangeRF12.registerFloatHandler(&RF12_TagUpdateFloatHandler);
  tagExchangeRF12.registerLongHandler(&RF12_TagUpdateLongHandler);

  tagExchangeSerial.registerFloatHandler(&Serial_TagUpdateFloatHandler);
  tagExchangeSerial.registerLongHandler(&Serial_TagUpdateLongHandler);
}  


// callback function: executed when a new tag update of type float is received
void RF12_TagUpdateFloatHandler(int tagid, unsigned long timestamp, float value)
{         
  if (debug_txt) {  
    Serial.print(F("RF12 Float Update Tagid="));
    Serial.print(tagid);
                
    Serial.print(F(" Value="));
    Serial.print(value);
                 
    Serial.println();
  }

  BlinkRF12Led();
  tagExchangeSerial.publishFloat(tagid, value);
}


// callback function: executed when a new tag update of type long is received
void RF12_TagUpdateLongHandler(int tagid, unsigned long timestamp, long value)
{
  if (debug_txt) {  
    Serial.print(F("RF12 Long Update Tagid="));
    Serial.print(tagid);
                
    Serial.print(F(" Value="));
    Serial.print(value);
                 
    Serial.println();
  }
  
  BlinkRF12Led();
  tagExchangeSerial.publishLong(tagid, value);
}





// callback function: executed when a new tag update of type float is received
void Serial_TagUpdateFloatHandler(int tagid, unsigned long timestamp, float value)
{         
  if (debug_txt) {  
    Serial.print(F("Serial Float Update Tagid="));
    Serial.print(tagid);
                
    Serial.print(F(" Value="));
    Serial.print(value);
                 
    Serial.println();
  }

  BlinkSerialLed();
  tagExchangeRF12.publishFloat(tagid, value);
}


// callback function: executed when a new tag update of type long is received
void Serial_TagUpdateLongHandler(int tagid, unsigned long timestamp, long value)
{
  if (debug_txt) {  
    Serial.print(F("Serial Long Update Tagid="));
    Serial.print(tagid);
                
    Serial.print(F(" Value="));
    Serial.print(value);
                 
    Serial.println();
  }
  
  BlinkSerialLed();
  tagExchangeRF12.publishLong(tagid, value);
}




static void handleInput (char c) {
  
  if (!tagExchangeSerial.handleInput(c)) {   
    
    switch (c) {
      case 'd':
        // toggle debug
        if (debug_txt) {  
          Serial.println(F("Debug off"));
          debug_txt = false;
        } else {
          Serial.println(F("Debug on"));
          debug_txt = true;
        }
        break;

      case 'l':
        // send lifesign message
        scheduler.timer(TASK_LIFESIGN, 0);
        break;

      case '?':
        // show help
        showHelp();
        break;

      
      default:
        //Serial.print("Serial RX for US: ");
        //Serial.println(c,HEX);  
        break;
    }
  }
}



void loop(void) {
  
  while (Serial.available())
    handleInput(Serial.read());
  
  
  // handle rf12 communications
  tagExchangeRF12.poll(); 
  tagExchangeSerial.poll();
 
  
  switch (scheduler.poll()) {

    case TASK_LIFESIGN:
      if (debug_txt) {  
        Serial.println(F("Send Lifesign message"));
      }

      BlinkSerialLed();
      BlinkRF12Led();
    
      tagExchangeRF12.publishLong(100 + NODE_ID ,  millis());
      tagExchangeSerial.publishLong(100 + NODE_ID ,  millis());
      
      scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);
      break;
      
    case TASK_LED_HEARTBEAT:
      switch (heartbeat_state) {
        case 0:
          digitalWrite(LED_HEARTBEAT,   HIGH);
          scheduler.timer(TASK_LED_HEARTBEAT, 1);
          heartbeat_state = 1;
          break;

        case 1:
          digitalWrite(LED_HEARTBEAT,   LOW);
          scheduler.timer(TASK_LED_HEARTBEAT, 2);
          heartbeat_state = 2;
          break;

        case 2:
          digitalWrite(LED_HEARTBEAT,   HIGH);
          scheduler.timer(TASK_LED_HEARTBEAT, 1);
          heartbeat_state = 3;
          break;

        case 3:
          digitalWrite(LED_HEARTBEAT,   LOW);
          scheduler.timer(TASK_LED_HEARTBEAT, 10);
          heartbeat_state = 0;
          break;                
      }
      
      break;
    
    case TASK_LED_SERIAL:
      digitalWrite(LED_DATA_SERIAL, LOW);
      break;
      
    case TASK_LED_RF12:
      digitalWrite(LED_DATA_RF12,   LOW);
      break;
      
  }
}    


