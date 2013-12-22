/*
 Basic MQTT example 
 
  - connects to an MQTT server
  - publishes "hello world" to the topic "outTopic"
  - subscribes to the topic "inTopic"
*/

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <JeeLib.h>


#define TRANSMIT_INTERVAL 150  /* in 0.1 s */
#define LIFESIGN_INTERVAL  5  /* in 0.1 s */

#define SERIAL_BAUD 57600

enum { 
  TASK1, TASK2, 
  TASK_LIFESIGN,
  TASK_LIMIT };
  
static unsigned long seqnr = 0;  

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);


// Update these with values suitable for your network.
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
byte server[] = { 192, 168, 1, 209 };
byte ip[]     = { 192, 168, 1, 45 };

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.println("message received");
  
  Serial.print("topic: ");
  Serial.print(topic);
  Serial.print(" = ");
  
  char *s = (char *)malloc(length+1);
  memcpy(s,payload,length);
  s[length]='\0';
  Serial.println(s);
  free(s);
  
  
}

EthernetClient ethClient;
PubSubClient client(server, 1883, callback, ethClient);


int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


void setup()
{
  Serial.begin(SERIAL_BAUD);
  Serial.print(F("\nmeterkastnode [xxxxx.ino]\n"));

  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());

  Serial1.begin(SERIAL_BAUD);
  
  
  Ethernet.begin(mac, ip);
  if (client.connect("arduinoClient")) {
    Serial.println("MMQT Connected");
    client.publish("outTopic","hello world");
    client.subscribe("inTopic");
    client.subscribe("arduino/#");
  }
  
    // start transmission of lifesign message
  scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);

  float v = 3.14159267;
  
  while (v<1.0E10) {
    static char charMsg[20];
    memset(charMsg,'\0', 20);
    dtostrf(v, 4, 2, charMsg);
    //sprintf(charMsg,"v = %d",v);
    Serial.println(charMsg);
    v=v*2.0;
    delay(50);
  }
  

}

void loop()
{
  client.loop();
  
  
  
  // read from port 1, send to port 0:
  if (Serial1.available()) {
    int inByte = Serial1.read();
    Serial.write(inByte); 
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
      
  case TASK_LIFESIGN:
      //tagExchangeRF12.publishLong(100 + NODE_ID ,  millis());

        // ### move re-connecting to separate task
        if (!client.connected()) {
          if (client.connect("arduinoClient")) {     
            Serial.println("MMQT (re-)connected");
          } 
        }

      
      if (client.connected()) {
        static char charMsg[10];
        memset(charMsg,'\0', 10);
        //dtostrf(celsius, 4, 2, charMsg);
        sprintf(charMsg,"%ld",seqnr);
          
        //Serial.print("Lifesign = ");
        //Serial.println(charMsg);

        client.publish("arduino/lifesign",charMsg);
      }
      seqnr++;
      
      
      
      if (client.connected()) {
        static char charMsg[10];
        memset(charMsg,'\0', 10);
        //dtostrf(celsius, 4, 2, charMsg);
        sprintf(charMsg,"%d",freeRam());
          
        client.publish("arduino/freeram",charMsg);
      }
      
      
      scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);
      break;

      
  }
      
  
  
}

