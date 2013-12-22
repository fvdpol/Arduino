/*
 Basic MQTT example 
 
  - connects to an MQTT server
  - publishes "hello world" to the topic "outTopic"
  - subscribes to the topic "inTopic"
*/

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>  // F Malpartida's NewLiquidCrystal library
//#include <JeeLib.h>
//#include <Time.h>

#define I2C_ADDR    0x20  // Define I2C Address where the PCF8574A is

#define BACKLIGHT_PIN     7
#define En_pin  4
#define Rw_pin  5
#define Rs_pin  6
#define D4_pin  0
#define D5_pin  1
#define D6_pin  2
#define D7_pin  3

#define  LED_OFF  0
#define  LED_ON  1

#define printByte(args)  write(args);

LiquidCrystal_I2C  lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);


// Update these with values suitable for your network.
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 40 };
byte server[] = { 192, 168, 1, 1 };
byte ip[]     = { 192, 168, 1, 40 };



long msgcount = 0;

unsigned long currentTime;
unsigned long printTime = 0;


int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}



void callback(char* topic, byte* payload, unsigned int length) {
  
  
//  Serial.print("Free RAM: ");
//  Serial.println(freeRam());
  
  
  // handle message arrived --> clear screen
  if (msgcount == 0) {  
    lcd.clear();
  }


 String Topic = String(topic);
 
 char* s = (char*)malloc(length+1);
 memcpy(s,payload,length);
 s[length]=0;
 String Value = String(s);
 free(s);
  
 if (Topic.equals("/raw/jeenode/tag/21")) {
   Serial.println("");
   Serial.println("We have an electricity value");
   
   int ival = Value.toInt();
   Serial.println(ival);


   //lcd.home();   
   lcd.setCursor(0, 0);
   lcd.print("E = ");
   
   if (ival <1000) { 
     lcd.print(" ");
   }
   
   if (ival <100) { 
     lcd.print(" ");
   }
   
   if (ival <10) { 
     lcd.print(" ");
   } 
   lcd.print(ival);
   lcd.print(" Watt");
   
 }
 else
 if (Topic.equals("/raw/jeenode/tag/23")) {
   Serial.println("");
   Serial.println("We have an gas value");
   
   int ival = Value.toInt();
   Serial.println(ival);

   //lcd.home();
   lcd.setCursor(0, 1);
   lcd.print("G = ");
   
   if (ival <1000) { 
     lcd.print(" ");
   }
   
   if (ival <100) { 
     lcd.print(" ");
   }
   
   if (ival <10) { 
     lcd.print(" ");
   } 
   lcd.print(ival);
   lcd.print(" l/h ");

 }
 else {
  Serial.println("");
  Serial.println("Received message");
  Serial.print("topic = ");
  Serial.println(Topic);
  Serial.print("Message = ");
  Serial.println(Value);
 }


 msgcount++;
    
    
 lcd.setCursor(10, 2);
 lcd.print("C = ");
 lcd.print(msgcount);
 
  lcd.setCursor(0, 3);
 lcd.print("M = ");
 lcd.print(freeRam());

}

EthernetClient ethClient;
PubSubClient client(server, 1883, callback, ethClient);

boolean connect_mqtt() {
  Serial.print("MQTT...");

  lcd.setCursor(0, 2);
  lcd.print("Connecting");
  //         1234567890

  if (client.connect("arduinoClient2")) {
    Serial.println("connected");
        
    lcd.setCursor(0, 2);
    lcd.print("Connected ");
    
//    char topic[35];
//    snprintf(topic, 35, "house/node/%s/state", "arduinoClient");
//    client.publish(topic, "start");

    client.subscribe("/raw/jeenode/tag/1");  /* temperature ??? */
    client.subscribe("/raw/jeenode/tag/21");  /* Electricity watts */
    client.subscribe("/raw/jeenode/tag/23");  /* Gas l/h */


    return true;
  }
  Serial.println("Failed.");

  lcd.setCursor(0, 2);
  lcd.print("No Connect");
  //         1234567890


  return false;
}

void setup()
{
  
  //start serial connection
  Serial.begin(57600);
  Serial.print("\n\nmqtt test");
  
  
  Serial.print("Free RAM: ");
  Serial.println(freeRam());
  
  
  lcd.begin (20,4);  // initialize the lcd 
  // Switch on the backlight
  lcd.setBacklightPin(BACKLIGHT_PIN,NEGATIVE);
  lcd.setBacklight(LED_ON);
  
  lcd.home();
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("MQTT Test...");
  
  Serial.print("DHCP ");
  Serial.print("...");
  
  if (false) {
    // DHCP
    if (Ethernet.begin(mac) == 0) {    
      Serial.println("Failed.");
      while(true);
    }
  } else {
    // static IP
    Ethernet.begin(mac,ip);
  }
  
  Serial.println(Ethernet.localIP());
  lcd.setCursor(0,1);
  lcd.print("DHCP: ");
  lcd.print(Ethernet.localIP());
  
  
//  Ethernet.begin(mac, ip);
  if (client.connect("arduinoClient2")) {

  lcd.setCursor(0, 2);
  lcd.print("Host: 192.168.1.1");
  /*         12345678901234567890 */

    client.publish("outTopic","hello world");

    client.subscribe("/raw/jeenode/tag/1");  /* temperature ??? */
    client.subscribe("/raw/jeenode/tag/21");  /* Electricity watts */
    client.subscribe("/raw/jeenode/tag/23");  /* Gas l/h */
  }

  
}

void loop()
{
  
  
  if (!client.connected() && !connect_mqtt()) {
    return;
  }


  
  // get the current elapsed time
  currentTime = millis();
 
  if(currentTime >= (printTime + 100)){

    lcd.setCursor(0, 3);
    lcd.print("M = ");
    lcd.print(freeRam());

     
     lcd.setCursor(10, 3);
    long csec = (currentTime/100);
    lcd.print(csec/10.0); 
    printTime = currentTime;
    
    lcd.setCursor(15, 0);
    lcd.print("CL");
    client.loop();
    lcd.setCursor(15, 0);
    lcd.print("  ");

  }                             
  
#if 0  
  int dhcp_status = Ethernet.maintain();
  /*
    returns:
    0: nothing happened
    1: renew failed
    2: renew success
    3: rebind fail
    4: rebind success
  */
  if (dhcp_status) {
    long now = millis();
    Serial.println("DHCP Lease");
  }
#endif
}

