
#include <SPI.h>
#include <Ethernet.h>
#include <JeeLib.h>
#include <TagExchange.h>


int led = 13;



// assign a MAC address for the ethernet controller.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
// fill in your address here:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

IPAddress server(10,2,2,151); // Automation

// fill in an available IP address on your network here,
// for manual configuration:
//IPAddress ip(10,2,2,82);

byte ip[] = { 10, 2, 2, 83 };
byte gateway[] = { 10, 2, 2, 1 };
byte subnet[] = { 255, 255, 255, 0 };

bool gotdata = false;

// initialize the library instance:
EthernetClient client;

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(10,2,2,251);      // numeric IP for api.cosm.com
//char server[] = "api.cosm.com";   // name address for cosm API

unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long postingInterval = 10*1000; //delay between updates to Cosm.com


TagExchangeRF12 tagExchangeRF12;
TagExchangeStream tagExchangeEth(&client);  // use stream to publish tags to ethernet end-point
TagExchangeHardwareSerial tagExchangeSerial(&Serial);


#define TRANSMIT_INTERVAL 150  /* in 0.1 s */



enum { 
  TASK_TRANSMIT,
  TASK_LIMIT };


static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


void setup() {
  // start serial port:
  Serial.begin(57600);
  Serial.println(F("TagPub 2 Ethernet Sketch"));
  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());
    
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);     


  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  
  
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println(F("connecting..."));

  // if you get a connection, report back via serial:
  if (client.connect(server, 5000)) {
    Serial.println(F("connected"));
    // Make a HTTP request:
    client.println(F("Dit is een test vanaf de Arduino!"));
    //client.println();
  } 
  else {
    // kf you didn't get a connection to the server:
    Serial.println(F("connection failed"));
  }
  
   
  
  // test transmission
  scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
  
  
  // register callback functions for the tag updates
  tagExchangeRF12.registerFloatHandler(&TagUpdateFloatHandler);
  tagExchangeRF12.registerLongHandler(&TagUpdateLongHandler);
  
}



// callback function: executed when a new tag update of type float is received
void TagUpdateFloatHandler(int tagid, unsigned long timestamp, float value)
{         
  Serial.print(F("Float Update Tagid="));
  Serial.print(tagid);
              
  Serial.print(F(" Value="));
  Serial.print(value);
               
  Serial.println();

  tagExchangeEth.publishFloat(tagid, value);
}


// callback function: executed when a new tag update of type long is received
void TagUpdateLongHandler(int tagid, unsigned long timestamp, long value)
{
  Serial.print(F("Long Update Tagid="));
  Serial.print(tagid);
              
  Serial.print(F(" Value="));
  Serial.print(value);
               
  Serial.println();
  
  tagExchangeEth.publishLong(tagid, value);
}




void loop() {
  
    tagExchangeRF12.poll(); 
    tagExchangeEth.poll();
    tagExchangeSerial.poll();

  
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
    gotdata = true;
  } else {
    if (gotdata) {
       gotdata = false;
       Serial.println("-----");
    }
  }

  switch (scheduler.poll()) {

  case TASK_TRANSMIT:
     // reschedule every 15s
      
//      tagExchangeRF12.publishFloat(10, 1.0 * millis());
//      tagExchangeRF12.publishLong(11,  millis());
      
      // transmission is handled by queueing mechanism. if we don't want to wait we can trigger the sending ourselves.
      //tagExchangeRF12.publishNow();
      
      
      tagExchangeEth.publishFloat(10, 1.0 * millis());
      tagExchangeEth.publishLong(11,  millis());


      tagExchangeSerial.publishFloat(10, 1.0 * millis());
      tagExchangeSerial.publishLong(11,  millis());
      tagExchangeSerial.publishLong(12,  freeRam());

      
      scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
    break;
    
  }
 
  

  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
//  if (client.available()) {
//    char c = client.read();
//    Serial.print(c);
//  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
//  if (!client.connected() && lastConnected) {
//    Serial.println();
//    Serial.println("disconnecting.");
//    client.stop();
//  }

  // if you're not connected, and ten seconds have passed since
  // your last connection, then connect again and send data:
//  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
   
//    digitalWrite(led, HIGH);
//    sendData();
//    digitalWrite(led, LOW);  
//  }
  // store the state of the connection for next time through
  // the loop:
//  lastConnected = client.connected();
}


#if 0
// this method makes a HTTP connection to the server:
void sendData(void) {
  // if there's a successful connection:
  Serial.println("connecting..."); 
  if (client.connect(server, 5000)) {
    Serial.println("connected."); 
    client.println("New Arduino connection...");
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }
   // note the time that the connection was made or attempted:
  lastConnectionTime = millis();
}
#endif 


    
