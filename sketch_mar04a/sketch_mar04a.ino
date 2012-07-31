#include <Ports.h>
#include <RF12.h>


// test sketch for central node
//  - log packets from remote nodes
//  - ??? store & forward with e2prom? 
//    I2C EEPROM 24LC256 - 256kb (32k*8)
//          this chip has 512 pages of 64 bytes; cascadable up to 8 devices
//  - gateway pc->nodes (routing function)
//  - real-time clock (synchronise from time server = PC)

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




MilliTimer sendTimer;
typedef struct { 
  unsigned long a;
  unsigned long b;
} Payload;
Payload inData, outData;
byte pendingOutput;


void blinkLed(void) {
  
    //    flash led;  
    one.digiWrite(1);
    delay(5);     
    one.digiWrite(0);
}





static void consumeInData (void) {
    // use data from the RF12 library -- avoid copy
    // rf12_data, rf12_len
    
        byte n = rf12_len;
        if (rf12_crc == 0) {
            logPacketToSerial();

//            Serial.print("OK");
        } 
//        Serial.print(' ');
        
        // size of packet
//        Serial.print((int) rf12_hdr);  
        
        // payload
//        for (byte i = 0; i < n; ++i) {
//            Serial.print(',');  // should be optional
//            Serial.print((int) rf12_data[i]);
//        }
//        Serial.println();
        
        if (rf12_crc == 0) {            
            if (RF12_WANTS_ACK) { // && (config.nodeId & COLLECT) == 0) {
//                Serial.println(" -> ack");
                rf12_sendStart(RF12_ACK_REPLY, 0, 0);
            }
            
        }  
}


static void logPacketToSerial (void) {
        byte n = rf12_len;

        Serial.print("PACKET ");
        
        // size of packet
        Serial.print((int) rf12_hdr);  
        
        // payload
        for (byte i = 0; i < n; ++i) {
            Serial.print(',');  // should be optional
            Serial.print((int) rf12_data[i]);
        }
        Serial.println();        
}



static byte produceOutData () {
    
    return 1;
}




static void handleInput (char c) {
   Serial.print("IN ");
   Serial.print(c);
   Serial.print("\n");
   
}



void setup() {               
    Serial.begin(57600);
    Serial.print("\n[test sketch]\n");
  
  one.mode(OUTPUT);
  
  
  rf12_initialize (NODE_ID, RF12_868MHZ, GROUP_ID);
  
  
  scheduler.timer(TASK1, 1);
  scheduler.timer(TASK2, 1);
    
  ticksPerSecond = 1000;
  
}



void loop() {
  
    if (Serial.available())
        handleInput(Serial.read());
      

    //     
    if (rf12_recvDone() && rf12_crc == 0) {
        consumeInData();
        rf12_recvDone();
    }

    if (sendTimer.poll(100))
        pendingOutput = produceOutData();


    if (pendingOutput && rf12_canSend()) {
        rf12_sendStart(0, &outData, sizeof outData, 2);
        // optional: rf12_sendWait(2); // wait for send to finish
        pendingOutput = 0;
    }



        
    if (RepTimer.poll(ticksPerSecond)) {
         timestamp++;             
    }
    
    
    
  switch (scheduler.poll()) {
    
    // LED 1 blinks regularly, once a second
  case TASK1:
    scheduler.timer(TASK1, 10);     
    //blinkLed();
        
    break;
    
    // report timing info
  case TASK2:
    scheduler.timer(TASK2, 150);

    Serial.print("TS ");
    Serial.print(timestamp);
    Serial.print(" ");
    Serial.print(ticksPerSecond);
    Serial.print("\n");
    break;

  }
    
}
