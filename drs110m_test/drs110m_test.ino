// test program rs485
// DRS110M

// rs485 bus connected to serial3
// output #D2 toggles TX


#include <JeeLib.h>


#define SERIAL_BAUD 57600

enum { 
  TASK1, TASK2, 
  TASK_LIMIT };
  
static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);


enum stateType {
  disconnected = 10,
  wait_identification = 11,
  wait_password_prompt = 12,
  wait_password_verification=13,
  wait_data_read = 14,
  data_available = 15
};


stateType  state = disconnected;



bool debug_txt = false;
int  tx_sel = 2;  // pin to enable MAX485 output

char* SerialID = "000000000000";

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


void showHelp(void) 
{
  Serial.println(F("\nkWh test"));  
  
  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());
  //Serial.print(F("Network:  "));
  //Serial.println(GROUP_ID);
  //Serial.print(F("Node ID:  "));
  //Serial.println(NODE_ID);
  
  Serial.println(F("\n"
      "Available commands:\n"
      " d  - toggle debug messages\n"
      " l  - send lifesign message\n"
      " ?  - show this help\n"
      "\n"));  
}


void setup() {
  // put your setup code here, to run once:
  
  // console
  Serial.begin(SERIAL_BAUD);
  showHelp();
  
  
  
  pinMode(tx_sel, OUTPUT);
  digitalWrite(tx_sel, LOW);
  Serial3.begin(9600,SERIAL_7E1);
  
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

//      case 'l':
//        // send lifesign message
//        scheduler.timer(TASK_LIFESIGN, 0);
//        break;

  case 'r':
    Serial.println(F("Start Read Sequence"));

      digitalWrite(tx_sel, HIGH);
      delay(1);
      Serial3.print("/?");
      Serial3.print(SerialID);
      Serial3.print("!\r\n");
      Serial3.flush();
      //delay(1); 
      digitalWrite(tx_sel, LOW);
      
      state = wait_identification;
      break;
    

    case 't':
      Serial.println(F("Test Msg - get Serial ID"));
      digitalWrite(tx_sel, HIGH);
      delay(1);
      Serial3.print("/?!\r\n");
      Serial3.flush();
      delay(1); 
      digitalWrite(tx_sel, LOW);
      break;
      
     case '1':
      Serial.println(F("Test Msg 1 - check Serial ID"));
      digitalWrite(tx_sel, HIGH);
      delay(1);
      Serial3.print("/?");
      Serial3.print(SerialID);
      Serial3.print("!\r\n");
      Serial3.flush();
      //delay(1); 
      digitalWrite(tx_sel, LOW);
      break;
  
     case '2':
      Serial.println(F("Test Msg 2 - ACK/Option Select Message"));
      digitalWrite(tx_sel, HIGH);
      delay(1);
      Serial3.print(char(0x06));
      Serial3.print(char(0x30));
      Serial3.print(":");
      Serial3.print(char(0x31));
      Serial3.print("\r\n");
      Serial3.flush();
      //delay(1); 
      digitalWrite(tx_sel, LOW);
      
      delay(50);
      digitalWrite(tx_sel, HIGH);
      delay(1);
      Serial3.print(char(0x01));
      Serial3.print("P1");
      Serial3.print(char(0x02));
      Serial3.print("00000000");
      Serial3.print(char(0x03));
      Serial3.print(char(0x61));
      Serial3.flush();
      //delay(1); 
      digitalWrite(tx_sel, LOW);
            
      break;
      
      
     case '3':
      Serial.println(F("Test Msg 3 - Read Register"));
      digitalWrite(tx_sel, HIGH);
      delay(1);
      Serial3.print(char(0x01));
      Serial3.print("R1");
      Serial3.print(char(0x02));
      Serial3.print("00000000()");
      Serial3.print(char(0x03));
      Serial3.print(char(0x63));
      Serial3.flush();
      digitalWrite(tx_sel, LOW);
      
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



void loop() {
  // put your main code here, to run repeatedly: 
  
  
  // D2 = high: transmit
  // D2 = low:  receive
  
  
  // handle serial port
  while (Serial.available())
    handleInput(Serial.read());
   
    
  while (Serial3.available()) {
    char c = Serial3.read();
    Serial.print("RX: 0x");
    Serial.print(c, HEX);
    if (c>=32) {
      Serial.print("\t'");
      Serial.print(char(c));
      Serial.print("'");
    }
      Serial.println();
    
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
  }
  
}


