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
#include <TagExchange.h>
#include <MyTags.h>

// smartmeter
//#include <SoftwareSerial.h>

#define NODE_ID 1
#define GROUP_ID 235 


#define MQTTCONNECT_INTERVAL  20
#define TRANSMIT_INTERVAL 150  /* in 0.1 s */
#define LIFESIGN_INTERVAL 100  /* in 0.1 s */

#define SERIAL_BAUD 57600

enum { 
  TASK1, TASK2, 
  TASK_MQTTCONNECT,
  TASK_LIFESIGN,
  TASK_LIMIT };
  

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);

bool debug_txt = false;
int heartbeat_state = 0;



TagExchangeStream tagExchangeSerial(&Serial1);



//SoftwareSerial mySerial (10, 17, true); // inverted logic


// Update these with values suitable for your network.
byte mac[]    = {  0xDE, 0xED, 0xBE, 0xEF, 0xFE, 0xED };
byte server[] = { 192, 168, 1, 51 };



// keep a history of the last number of messages relayed from RF12 to MQTT
// as the MQTT broker will echo these back --> this would result in re-publishing
// these over RF12. To prevent this loop a queue if the last messages send is 
// maintained and if a message received from the MQTT matches these the publication
// to RF12 will be supressed.

class TagHistory {
  public:
  
    // constructor
    TagHistory(int size=8, long autoexpire=10000);
    
    void Write(TagExchange::TagType tagtype, int tagid);
    
    bool Exists(TagExchange::TagType tagtype, int tagid);

    void Debug(void);
    
    bool isEmpty(void) {
      return (end == start);
    }
    
  private:
   int size;
   int start;
   int end;
   long autoexpire;
   
   typedef struct {
     unsigned int tagcode;
     unsigned long timestamp;
   } tagelementType;
   
   tagelementType *history; 
   
   // perform the expiration logic; remove expired entries from the queue
   bool Expire(void);

};

TagHistory::TagHistory(int _hsize, long _autoexpire) {
  size = _hsize;
  autoexpire = _autoexpire;

  start = 0;
  end = 0;
  
  history = (tagelementType *) malloc(sizeof(tagelementType) * size);
  
}


void TagHistory::Write(TagExchange::TagType tagtype, int tagid) {
  unsigned int tagcode = tagExchangeSerial.getTagCode(tagtype, tagid);
  
  // expire any old entries to free up space 
  while (Expire())
    ; // keep call the Expiration function untill there is nothing to expire (returns false)
  
  history[start].tagcode = tagcode;
  history[start].timestamp = millis() + autoexpire; // timestamp at which the message should expire
  start = (start + 1) % size;
  
  // in case buffer is full advance the end pointer
  if (start == end)
    end = (end + 1) % size;
     
  //Debug();
  
}


bool TagHistory::Exists(TagExchange::TagType tagtype, int tagid) {
  
  // expire any old entries to free up space 
  while (Expire())
    ; // keep call the Expiration function untill there is nothing to expire (returns false)
    
   if (!isEmpty()) {
     unsigned int tagcode = tagExchangeSerial.getTagCode(tagtype, tagid);

     int p = start;
     while (p != end) {
       p--;
       if (p<0) {
         p+=size;
       }
 
       if (history[p].tagcode == tagcode) 
         return true;
     }
   }
   return false;
 }



bool TagHistory::Expire(void) {
  unsigned long ts = millis();
  
  if (end != start) {
    // check last element in queue
    if (history[end].timestamp <= ts) {
        //Serial.println("Expiring last entry");
        end = (end + 1) % size;
        return true;
    }
  } 
  return false;
}

void TagHistory::Debug(void) {
   Serial.println(F("TagHistory Debug"));
   Serial.print(F("start = "));
   Serial.println(start);
   Serial.print(F("end   = "));
   Serial.println(end);
   
   if (!isEmpty()) {
     int p = start;
     while (p != end) {
       p--;
       if (p<0) {
         p+=size;
       }

       Serial.print(p);
       Serial.print(": ");
       
       Serial.print(history[p].tagcode);
       Serial.print("\t");
       Serial.print(history[p].timestamp);
       Serial.println();
     }
   }
}


// keep last 10 messages, with expiration of 10 seconds
TagHistory th(10,10000);




//////////////////////////////////////


extern "C" {
  // callback function types
    typedef void (*SmartMeterUpdateCallbackFunction)(char *tag, char *value);
}



class P1SmartMeter {
  
public:

  // constructor
  P1SmartMeter(void);
  void poll(void);

  void registerUpdateHandler(SmartMeterUpdateCallbackFunction);



private:
  char *buf;
  int end;
  bool debug;
  bool pendingGasValue;  // quirck for gas meter readout split over multiple lines
  
  // callback handlers
  SmartMeterUpdateCallbackFunction	_updateHandler_callback;


  void processLine(void);
};


P1SmartMeter::P1SmartMeter(void) {
  debug=false;
  pendingGasValue = false;

  Serial2.begin(9600,SERIAL_7E1);
    
  buf = (char *)malloc(150);
  end=0;
  buf[end]='\0';
}



void P1SmartMeter::registerUpdateHandler(SmartMeterUpdateCallbackFunction callbackfunction) {
  _updateHandler_callback = callbackfunction;
}



void P1SmartMeter::processLine() {
              
        if (debug && strlen(buf) > 0) {
            Serial.print(F("P1: "));
            Serial.println(buf);
         }
         
        if (strncmp(buf,"/",1)==0) {
            // start of data stream
        } else
        if (strncmp(buf,"!",1)==0) {
            // end of data stream
        } else
        if (buf[0] != '\0') {
            char *tag = buf;
            char *value = buf;
            
            // find separator between tag and value --> start of '(' surrounding value
            char *sep = buf;
            while (*sep) {
                if ((*sep) == '(') {
                  *sep='\0';
                  value=sep+1;
                } else {
                  sep++;
                }
            }
            
            char *term=value;
            while (*term) {
              if ((*term) == ')')
                *term='\0';
              term++;
            }
            
            
            if (pendingGasValue) {
              
              //Serial.print("GAS Tag = ");
              //Serial.println(tag);
              //Serial.print("GAS Val = ");
              //Serial.println(value);
              tag = "0-1:24-3-0";  // set the tag to the *processed* variant of gas m3; this was the quirck 
                                   // detection logic will leave it alone as this is looking for the unprocessed              
              
              pendingGasValue = false;
            }
            
            if ((strcmp(tag,"1-0:1.8.1") == 0)      // meter reading import tariff 1 (high) 
              ||(strcmp(tag,"1-0:1.8.2") == 0)      // meter reading import tariff 2 (low) 
              ||(strcmp(tag,"1-0:2.8.1") == 0)      // meter reading export tariff 1 (high) 
              ||(strcmp(tag,"1-0:2.8.2") == 0)      // meter reading export tariff 2 (low)
              ||(strcmp(tag,"0-0:96.14.0") == 0)    // tariff indicator electricity
              ||(strcmp(tag,"1-0:1.7.0") == 0)      // actual power import
              ||(strcmp(tag,"1-0:2.7.0") == 0)      // actual power export
              ||(strcmp(tag,"0-0:96.3.10") == 0)    // switch electricity
              
              ||(strcmp(tag,"0-1:24.3.0") == 0)    // gas usage ++ format contains timestamp + value
              ||(strcmp(tag,"0-1:24-3-0") == 0)    // gas usage ++ format contains timestamp + value
              ||(strcmp(tag,"0-1:24.4.0") == 0))    // gas valve
            {
              
               // quirck for the gas usage value
               if (strcmp(tag,"0-1:24.3.0") == 0) {
                 pendingGasValue = true;
                 tag = "0-1:24-3-0/timestamp";
               }
              
              
              // substitue the dots by dashses as this is interpreted as path separator by mqtt
              for(char *sub=tag; *sub; sub++)
                if (*sub =='.')
                  *sub='-';
              //if (debug)
              //Serial.print("Tag = ");
              //Serial.println(tag);
              //Serial.print("Val = ");
              //Serial.println(value);
                                  
	      if (_updateHandler_callback != NULL) {
		(*_updateHandler_callback)(tag,value);
              }
            }      
        }  
          

        // clear string
        end=0;
        buf[end]='\0';        
      
}


void P1SmartMeter::poll(void) {
  
  while (Serial2.available()) {
    int c = Serial2.read();
    if (c > 0) {      
      if ((c >= 32) && (end<90)) {
        buf[end]=c;
        end++;
        buf[end]='\0';
      } else {
        processLine();   
      }
    }
  }
}



P1SmartMeter smartmeter;

//////////////////////////////////////



void MQTT_Callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
//  Serial.println("message received");
  
//  Serial.print("topic: ");
//  Serial.print(topic);
//  Serial.print(" = ");
  
//  char *s = (char *)malloc(length+1);
//  memcpy(s,payload,length);
//  s[length]='\0';
//  Serial.println(s);
 // free(s);
 
 
 /* parse the TagId from the MQTT Topic */
 char *t = topic;
 if (strncmp(t,"raw/jeenode/tag/",17) == 0) {
   t+=17;
   if (strncmp(t,"float/",6) == 0) {  /* Float Tag Update */
     t+=6;
     int tagid = atoi(t);
     
     if (!th.Exists(TagExchange::tagtype_float, tagid)) {
         char *s = (char *)malloc(length+1);
         memcpy(s,payload,length);
         s[length]='\0';
         float value = atof(s);
         free(s);
       
       tagExchangeSerial.publishFloat(tagid, value);

       if (debug_txt) {  
         Serial.print(F("MQTT Float Update Tagid="));
         Serial.print(tagid);
                
         Serial.print(F(" Value="));
         Serial.print(value);
                 
         Serial.println();
        }

     }

   } else
   if (strncmp(t,"long/",5) == 0) {    /* Long Tag Update */
     t+=5;
     int tagid = atoi(t);
     
     if (!th.Exists(TagExchange::tagtype_long, tagid)) {
         char *s = (char *)malloc(length+1);
         memcpy(s,payload,length);
         s[length]='\0';
         long value = atol(s);
         free(s);

       tagExchangeSerial.publishLong(tagid, value);

       if (debug_txt) {  
         Serial.print(F("MQTT Long Update Tagid="));
         Serial.print(tagid);
                
         Serial.print(F(" Value="));
         Serial.print(value);
                 
         Serial.println();
        }
     }
   }  
 } 

}

EthernetClient ethClient;
PubSubClient MQTTClient(server, 1883, MQTT_Callback, ethClient);


int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


void showHelp(void) 
{
  Serial.println(F("\nmeterkast_v1"));  
  
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


void setup()
{
  // setup pinmodes
  
  // console
  Serial.begin(SERIAL_BAUD);
  showHelp();
  
  /* DHCP */
  switch(Ethernet.begin(mac)) {
    case 1:    
            Serial.println(F("Ethernet DHCP bind success."));
            Serial.print("IP = ");
            Serial.println(Ethernet.localIP());
            break;

    default: 
            Serial.println(F("Ethernet DHCP bind failure."));    
  }


  
  scheduler.timer(TASK_MQTTCONNECT, 10);

//  if (MQTTClient.connect("arduinoClient")) {
//    Serial.println("MMQT Connected");
//    MQTTClient.publish("outTopic","hello world");
//    MQTTClient.subscribe("inTopic");
//    MQTTClient.subscribe("arduino/#");
//  }
  
  // connection to RF12 reception node
  Serial1.begin(SERIAL_BAUD);

  // register callback functions for the tag updates
  tagExchangeSerial.registerFloatHandler(&Serial_TagUpdateFloatHandler);
  tagExchangeSerial.registerLongHandler(&Serial_TagUpdateLongHandler);
  
  
  smartmeter.registerUpdateHandler(&SmartMeter_TagUpdateHandler);
  
  
  // start transmission of lifesign message
  scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);



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
  
  
  if (MQTTClient.connected()) {
        char topic[32];
        sprintf(topic,"raw/jeenode/tag/float/%d", tagid);
     
        static char charMsg[20];
        memset(charMsg,'\0', 20);
        dtostrf(value, 10, 3, charMsg);
        MQTTClient.publish(topic,charMsg);
        
      
        th.Write(TagExchange::tagtype_float, tagid);
  }

  //BlinkSerialLed();
  //tagExchangeRF12.publishFloat(tagid, value);
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
  
  
  if (MQTTClient.connected()) {
        char topic[32];
        sprintf(topic,"raw/jeenode/tag/long/%d", tagid);
     
        static char charMsg[20];
        memset(charMsg,'\0', 20);
        //dtostrf(value, 10, 3, charMsg);
        sprintf(charMsg,"%ld",value);
          
        //Serial.print("Lifesign = ");
        //Serial.println(charMsg);

        MQTTClient.publish(topic,charMsg);

        th.Write(TagExchange::tagtype_long, tagid);

  }

  
  //BlinkSerialLed();
  //tagExchangeRF12.publishLong(tagid, value);
}




// callback function: executed when a new tag update of type float is received
void SmartMeter_TagUpdateHandler(char *tag, char *value)
{         
  if (debug_txt) {  
    Serial.print(F("SmartMeter Tag Update Tag="));
    Serial.print(tag);
                
    Serial.print(F(" Value="));
    Serial.print(value);
                 
    Serial.println();
  }
  
  
  if (MQTTClient.connected()) {
        char topic[60];
        sprintf(topic,"raw/meterkast/smartmeter/%s", tag);
     
        MQTTClient.publish(topic, value);        
  }
}




static void handleInput (char c) {
  
  //if (!tagExchangeSerial.handleInput(c)) {   
    
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
  //}
}




void loop()
{
  
  
  tagExchangeSerial.poll();

  smartmeter.poll();
    
  
  // handle serial port
  while (Serial.available())
    handleInput(Serial.read());


  
  // handle MQTT
  MQTTClient.loop();
  
  
  while (Serial1.available()) {
    int inByte = Serial1.read();
    if (!tagExchangeSerial.handleInput(inByte)) {   
      Serial.write(inByte);
    } 
  }
  


  // handle serial port
//  while (Serial2.available()) {
//    int inByte = Serial2.read();
 //   if (inByte > 0) {
 //     Serial.write(inByte);
  //  }
 // }
  
  

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

      
  case TASK_MQTTCONNECT:
         
        switch(Ethernet.maintain()) {
          case 1:
                  Serial.println(F("Ethernet DHCP renew failed."));
                  break;
                  
          case 2:
                  Serial.println(F("Ethernet DHCP renew success."));
                  
                  //print out the IP address
                  Serial.print("IP = ");
                  Serial.println(Ethernet.localIP());
                  break;
      
            case 3:
                  Serial.println(F("Ethernet DHCP rebind fail."));
                  break;
      
          case 4:  
                  Serial.println(F("Ethernet DHCP rebind success."));
                  //print out the IP address
                  Serial.print("IP = ");
                  Serial.println(Ethernet.localIP());
                  break;
        }

  
  
        if (!MQTTClient.connected()) {
          if (MQTTClient.connect("Meterkast")) {     
            Serial.println(F("MMQT (re-)connected"));
            
            MQTTClient.subscribe("raw/jeenode/tag/#");
          } else {
            Serial.println(F("Failed to connect MMQT"));
        }
        }
      scheduler.timer(TASK_MQTTCONNECT, MQTTCONNECT_INTERVAL);
      break;


      
  case TASK_LIFESIGN:
      //tagExchangeRF12.publishLong(100 + NODE_ID ,  millis());

      
      if (MQTTClient.connected()) {
        static char charMsg[10];
        memset(charMsg,'\0', 10);
        //dtostrf(celsius, 4, 2, charMsg);
        sprintf(charMsg,"%ld",millis());
          
        //Serial.print("Lifesign = ");
        //Serial.println(charMsg);

        MQTTClient.publish("raw/meterkast/millis",charMsg);
      }
      
      
      
      if (MQTTClient.connected()) {
        static char charMsg[10];
        memset(charMsg,'\0', 10);
        //dtostrf(celsius, 4, 2, charMsg);
        sprintf(charMsg,"%d",freeRam());
          
        MQTTClient.publish("raw/meterkast/freeram",charMsg);
      }
      
      
      scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);
      break;

      
  }
      
  
  
}

