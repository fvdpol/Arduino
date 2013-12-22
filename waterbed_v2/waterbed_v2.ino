#include <JeeLib.h>
#include <OneWire.h>
#include <TagExchange.h>
#include <MyTags.h>

#define NODE_ID 4
/*#define GROUP_ID 212 */
#define GROUP_ID 235 


#define TRANSMIT_INTERVAL 150  /* in 0.1 s */
#define LIFESIGN_INTERVAL 600  /* in 0.1 s */



// Arduino PID Control:
//
// http://brettbeauregard.com/blog/2011/04/improving-the-beginner%E2%80%99s-pid-sample-time/
// http://playground.arduino.cc/Code/PIDLibrary
//
// http://fermentationriot.com/arduinopid.php




// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// http://milesburton.com/Dallas_Temperature_Control_Library

OneWire  ds(4);        // Jeenode Port 1 Digital
Port     relay(1);     // Jeenode Port 1 -- the solid state relay is connected the Analog port

//
// sensors:
// remote (long lead) -- water temperature
// ROM = 28 18 68 6C 1 0 0 E2
// Chip = DS18B20
//
// local (short lead) -- room temperature
// ROM = 28 2C 79 82 3 0 0 71
// Chip = DS18B20


//byte addr_bed[8];
//byte addr_room[8];

byte addr_bed[]  = {0x28, 0x18, 0x68, 0x6C, 0x01, 0x00, 0x00, 0xE2};  // waterbed sensor
byte addr_room[] = {0x28, 0x2C, 0x79, 0x82, 0x03, 0x00, 0x00, 0x71};  // room sensor

  

// these variables will also be mapped as tags & logged for trending
float bed_temperature;    // degrees celsius
float bed_setpoint;       // degrees celsius
float bed_output;         // 0...100 procent
float room_temperature;   // degrees celsius


// pwm cycle defined in centi-seconds. ie. PWM_CYCLE 3000 means 300 seconds (5 minute) cycle
//#define PWM_CYCLE 5000.0
#define PWM_CYCLE 600.0


float out_inc = 10.0;

enum { 
  TASK1, TASK2, 
  TASK_MEASURE,
  TASK_PROCESS,
  TASK_TRANSMIT,
  TASK_REPORT, 
  TASK_PWM_CYCLE, TASK_PWM_OUT,
  TASK_LIFESIGN,
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
  Serial.print(F("\nWaterbed node [waterbed_v2.ino]\n"));

  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());
  Serial.print(F("Network:  "));
  Serial.println(GROUP_ID);
  Serial.print(F("Node ID:  "));
  Serial.println(NODE_ID);
  

  // defined port A as output
  relay.digiWrite2(0);
  relay.mode2(OUTPUT);


  // mark analog values as unintialised
  room_temperature = -99999.0;
  bed_temperature  = -99999.0;
  bed_setpoint     = -99999.0;
  bed_output       = -50.0;
    
 
   rf12_initialize (NODE_ID, RF12_868MHZ, GROUP_ID);


  // register callback functions for the tag updates
  tagExchangeRF12.registerFloatHandler(&TagUpdateFloatHandler);
  
  
  
  // start measuring values
  scheduler.timer(TASK_MEASURE, 1);   
  
  // start pwm output
  scheduler.timer(TASK_PWM_CYCLE, 10);
  
  // startt transmission
  scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
  
  // start transmission of lifesign message
  scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);

}



// callback function: executed when a new tag update of type float is received
void TagUpdateFloatHandler(int tagid, unsigned long timestamp, float value)
{            
//  Serial.print("Float Update Tagid=");
//  Serial.print(tagid);
//  Serial.print(" Value=");
//  Serial.print(value);               
//  Serial.println();
  
  switch(tagid) {
    case TAG_F_WATERBED_UPDATE_SETPOINT:  // new setpoint (from remote controller)
    
      Serial.print(F("Update of Setpoint "));
      Serial.print(bed_setpoint);
      Serial.print(F(" --> "));
      Serial.print(value);
      Serial.println();
      
      bed_setpoint = value;
      break;
  }      
}


void loop(void) {
  float celsius;

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
     
    // send data
    tagExchangeRF12.publishFloat(TAG_F_WATERBED_ROOM_TEMPERATURE, room_temperature);
    tagExchangeRF12.publishFloat(TAG_F_WATERBED_BED_TEMPERATURE, bed_temperature);
    tagExchangeRF12.publishFloat(TAG_F_WATERBED_BED_SETPOINT, bed_setpoint);
    tagExchangeRF12.publishFloat(TAG_F_WATERBED_BED_OUTPUT, bed_output);
      
    // queueing mechanism not yet implemented; so we trigger the sending ourselves.
    //tagExchangeRF12.publishNow();

    scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
    break;


    
    
    // cyclic task to measure temperature values
  case TASK_MEASURE:
    scheduler.timer(TASK_MEASURE, 50);  // repeat every 5 seconds
      
    ds18b20_start(addr_room);
    ds18b20_start(addr_bed);
    scheduler.timer(TASK_PROCESS, 10);  // one second after starting measurement the DS18B20 sensors should have the value ready, and we can start processing it    
    break;
    
    
    // triggered from the measurement task
  case TASK_PROCESS:
    celsius = ds18b20_read(addr_room, 0); 
    if (room_temperature <= -9999.0) {
      // initialise value
      room_temperature = celsius;
    } else {
      room_temperature = (room_temperature * 0.9) + (celsius * 0.1);
    }
    
    Serial.print(F("  Temperature Room = "));
    Serial.print(celsius);
    Serial.println(F(" 'C raw"));
    Serial.print(F("  Temperature Room = "));
    Serial.print(room_temperature);
    Serial.println(F(" 'C"));
  
   
    celsius = ds18b20_read(addr_bed, 0); 
    if (bed_temperature <= -9999.0) {
      // initialise value
      bed_temperature = celsius;
    } else {
      bed_temperature = (bed_temperature * 0.9) + (celsius * 0.1);
    }
    
    Serial.print(F("  Temperature Bed  = "));
    Serial.print(celsius);
    Serial.println(F(" 'C raw"));
    Serial.print(F("  Temperature Bed  = "));
    Serial.print(bed_temperature);
    Serial.println(F(" 'C"));
    
    
    
    bed_output = bed_output + out_inc;

    if (bed_output < -50) {
      out_inc = 10.0;
    }
    if (bed_output > 150) {
      out_inc = -10.0;
    }
    
    Serial.print(F("  Output           = "));
    Serial.print(bed_output);
    Serial.println(F(" %"));

 
    break;
    

    
    case TASK_PWM_CYCLE:
      //
      //     +---------+
      //     |       <-|->               |
      //     +         +-----------------+
      //
      //     ^--------[PWM_CYCLE]--------^ 0.1 seconds
      //
      //     ^--[OUT]--^   OUT = output percentage/100 * PWM_CYCLE
      //
      
      {
        float  pwm_out = bed_output;
        if (pwm_out < 0.0) {
          pwm_out = 0.0;
        }
        if (pwm_out > 100.0) {
          pwm_out = 100.0;
        }
        
        scheduler.timer(TASK_PWM_CYCLE, int(PWM_CYCLE));
        scheduler.timer(TASK_PWM_OUT, int(PWM_CYCLE * (pwm_out / 100.0)));
        
        // only switch output if there is *any* output            
        if (pwm_out > 1.0) {
          relay.digiWrite2(1);
        }          
      }
      break;


    case TASK_PWM_OUT:
      if (bed_output < 99.0) {
        relay.digiWrite2(0);  
      }
      break;
     
      
    case TASK_LIFESIGN:
      tagExchangeRF12.publishLong(100 + NODE_ID ,  millis());
      
      scheduler.timer(TASK_LIFESIGN, LIFESIGN_INTERVAL);
      break;

      
  }
  
  delay(10);
}



 
//  relay.digiWrite2(0);
  






// start conversion of the dallas ds18b20 chip
void ds18b20_start(uint8_t *addr)
{
    ds.reset();
    ds.select(addr);
    ds.write(0x44,1);         // start conversion, with parasite power on at the end
}



// read temperature (celcius) from the dallas ds18b20 chip
//
// type_s = 0  --> DS18B20 or DS1822
// type_s = 1  --> DS18S20 or old DS1820
float ds18b20_read(uint8_t *addr, byte type_s)
{
  byte data[12];
  byte i;

  byte present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

//  Serial.print("  Data = ");
//  Serial.print(present,HEX);
//  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
//    Serial.print(data[i], HEX);
//    Serial.print(" ");
  }
//  Serial.print(" CRC=");
//  Serial.print(OneWire::crc8(data, 8), HEX);
//  Serial.println();

  // convert the data to actual temperature
  unsigned int raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // count remain gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw << 3;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw << 2; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw << 1; // 11 bit res, 375 ms
    // default is 12 bit resolution, 750 ms conversion time
  }
  
  // return celcius temperature value
  return (float)raw / 16.0;
}


