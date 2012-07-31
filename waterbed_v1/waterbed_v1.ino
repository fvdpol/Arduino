#include <JeeLib.h>
#include <OneWire.h>



#define NODE_ID 4
/*#define GROUP_ID 212 */
#define GROUP_ID 235 


#define TRANSMIT_INTERVAL 50  /* in 0.1 s */

typedef struct {
  // header
  unsigned long timestamp;
  byte msg_src;
  byte msg_dest;
  byte msg_id;
  byte msg_len;

  // payload  
  unsigned long watts;    // actual electricity consumption
  unsigned long e_count;  // electricity counter
  unsigned long g_count;  // gas counter 
} 
electro_Packet_t;



typedef struct {
  // header
  byte msg_id;      // identifier 
  
  byte tagcount;     // number of tags in packet
  
  // payload
  unsigned int tagid_1;
  float        value_1;
  
  unsigned int tagid_2;
  float        value_2;
  
  unsigned int tagid_3;
  float        value_3;
  
  unsigned int tagid_4;
  float        value_4;
  
  unsigned int tagid_5;
  float        value_5;
  
  unsigned int tagid_6;
  float        value_6;

  unsigned int tagid_7;
  float        value_7;

  unsigned int tagid_8;
  float        value_8;   
} 
Packet_t_8;



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


byte addr_bed[8];
byte addr_room[8];


// these variables will also be mapped as tags & logged for trending
float bed_temperature;    // degrees celsius
float bed_setpoint;       // degrees celsius
float bed_output;         // 0...100 procent
float room_temperature;   // degrees celsius


// pwm cycle defined in centi-seconds. ie. PWM_CYCLE 3000 means 300 seconds (5 minute) cycle
//#define PWM_CYCLE 5000.0
#define PWM_CYCLE 10.0


float out_inc = 10.0;

enum { 
  TASK1, TASK2, 
  TASK_MEASURE,
  TASK_PROCESS,
  TASK_TRANSMIT,
  TASK_REPORT, 
  TASK_PWM_CYCLE, TASK_PWM_OUT,
  TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);



void setup(void) {
  Serial.begin(57600);
  Serial.print("\nWaterbed node\n");

  // outside temperature sensor
  addr_room[0] = 0x28;
  addr_room[1] = 0x2C;
  addr_room[2] = 0x79;
  addr_room[3] = 0x82;
  addr_room[4] = 0x03;
  addr_room[5] = 0x00;
  addr_room[6] = 0x00;
  addr_room[7] = 0x71;
  

  // waterbed sensor
  addr_bed[0] = 0x28;
  addr_bed[1] = 0x18;
  addr_bed[2] = 0x68;
  addr_bed[3] = 0x6C;
  addr_bed[4] = 0x01;
  addr_bed[5] = 0x00;
  addr_bed[6] = 0x00;
  addr_bed[7] = 0xE2;


  // defined port A as output
  relay.digiWrite2(0);
  relay.mode2(OUTPUT);

  rf12_initialize (NODE_ID, RF12_868MHZ, GROUP_ID);
  //rf12_config();
  //rf12_easyInit(3); // Send value at most every 3 seconds


  // mark analog values as unintialised
  room_temperature = -99999.0;
  bed_temperature  = -99999.0;
  bed_setpoint     = -99999.0;
  bed_output       = -50.0;
    
 
  // start measuring values
  scheduler.timer(TASK_MEASURE, 1);   
  
  // start pwm output
  scheduler.timer(TASK_PWM_CYCLE, 1);
  
  // tast transmission
  scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
}





void loop(void) {
  float celsius;

  //rf12_easyPoll();
  
  
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
     { 
      Packet_t_8 packet;

      packet.msg_id = 1;
      packet.tagcount = 4;
      
      packet.tagid_1 = 1;
      packet.value_1 = room_temperature;

      packet.tagid_2 = 2;
      packet.value_2 = bed_temperature;

      packet.tagid_3 = 3;
      packet.value_3 = bed_setpoint;

      packet.tagid_4 = 4;
      packet.value_4 = bed_output;

      packet.tagid_5 = 0;
      packet.value_5 = 0.0;

      packet.tagid_6 = 0;
      packet.value_6 = 0.0;

      packet.tagid_7 = 0;
      packet.value_7 = 0.0;

      packet.tagid_8 = 0;
      packet.value_8 = 0.0;
      

      //rf12_easySend(&packet, sizeof packet);
      
      while (!rf12_canSend())
        rf12_recvDone();
      rf12_sendStart(0, &packet, sizeof packet);
      rf12_sendWait(0);

    }
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
    
    Serial.print("  Temperature Room = ");
    Serial.print(celsius);
    Serial.println(" 'C raw");
    Serial.print("  Temperature Room = ");
    Serial.print(room_temperature);
    Serial.println(" 'C");
  
   
    celsius = ds18b20_read(addr_bed, 0); 
    if (bed_temperature <= -9999.0) {
      // initialise value
      bed_temperature = celsius;
    } else {
      bed_temperature = (bed_temperature * 0.9) + (celsius * 0.1);
    }
    
    Serial.print("  Temperature Bed  = ");
    Serial.print(celsius);
    Serial.println(" 'C raw");
    Serial.print("  Temperature Bed  = ");
    Serial.print(bed_temperature);
    Serial.println(" 'C");
    
    
    
    bed_output = bed_output + out_inc;

    if (bed_output < -50) {
      out_inc = 10.0;
    }
    if (bed_output > 150) {
      out_inc = -10.0;
    }
    
    Serial.print("  Output           = ");
    Serial.print(bed_output);
    Serial.println(" %");

  
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


