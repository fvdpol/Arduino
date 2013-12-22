/*

 */
 

#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>  // F Malpartida's NewLiquidCrystal library


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


uint8_t bell[8]  = {0x4,0xe,0xe,0xe,0x1f,0x0,0x4};
uint8_t note[8]  = {0x2,0x3,0x2,0xe,0x1e,0xc,0x0};
uint8_t clock[8] = {0x0,0xe,0x15,0x17,0x11,0xe,0x0};
uint8_t heart[8] = {0x0,0xa,0x1f,0x1f,0xe,0x4,0x0};
uint8_t duck[8]  = {0x0,0xc,0x1d,0xf,0xf,0x6,0x0};
uint8_t check[8] = {0x0,0x1,0x3,0x16,0x1c,0x8,0x0};
uint8_t cross[8] = {0x0,0x1b,0xe,0x4,0xe,0x1b,0x0};
uint8_t retarrow[8] = {	0x1,0x1,0x5,0x9,0x1f,0x8,0x4};

LiquidCrystal_I2C  lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

 
unsigned long currentTime;
unsigned long loopTime = 0;
unsigned long printTime = 0;
const int pin_A = 22; 
const int pin_B = 23; 


int prev_enc_val = -1;

long value = 0;
long prev_value=-9999;

unsigned long samples = 0;


void setup(){
  //start serial connection
  Serial.begin(57600);

  pinMode(pin_A, INPUT_PULLUP);
  pinMode(pin_B, INPUT_PULLUP);
  
  
  lcd.begin (20,4);  // initialize the lcd 
  // Switch on the backlight
  lcd.setBacklightPin(BACKLIGHT_PIN,NEGATIVE);
  lcd.setBacklight(LED_ON);
  
  lcd.createChar(0, bell);
  lcd.createChar(1, note);
  lcd.createChar(2, clock);
  lcd.createChar(3, heart);
  lcd.createChar(4, duck);
  lcd.createChar(5, check);
  lcd.createChar(6, cross);
  lcd.createChar(7, retarrow);
  lcd.home();

  lcd.clear();
  
  lcd.setCursor(0, 0);
  for(int i = 0;i < 20; i++)  lcd.printByte(6);
  lcd.setCursor(0, 1);
  lcd.printByte(6);
  lcd.print("   Hello world    ");
  lcd.printByte(6);
  
  lcd.setCursor(0, 2);
  lcd.printByte(6);
  lcd.setCursor(19, 2);
  lcd.printByte(6);
  
  
  lcd.setCursor(0, 3);
  for(int i = 0;i < 20; i++)  lcd.printByte(6);
}

//          push switch
//     |   | 
//    _|___|_
//   |   _   |  
//   |  / \  |  top view
//   |  \_/  |
//   |_______|
//     | | |
//     | | |
//     A B G


int GrayToInt(int a, int b) {
  if (a==0 && b==0) 
    return 0;
  else if (a==0 && b>0)
    return 1;
  else if (a>0 && b>0)
    return 2;
  else 
    return 3;
}


void loop() {

  // get the current elapsed time
  currentTime = millis();
  
  if(currentTime >= (loopTime + 1)){
  // 5ms since last check of encoder = 1kHz  
    
    samples++;
    
    // Read encoder pins
    int enc_val = GrayToInt(digitalRead(pin_A), digitalRead(pin_B));
    
    
    // pev    next
    //  2
    //  3
    //  0  -> 3::-1 / 0::0 / 1::+1
    //  1  -> 0::-1 / 1::0 / 2::+1
    //  2  -> 1::-1 / 2::0 / 3::+1
    //  3  -> 2::-1 / 3::0 / 0::+1
    //  0
    //  1
    
    if (prev_enc_val == 0) {
      if (enc_val == 3) value--;
      if (enc_val == 1) value++;
    } else 
    if (prev_enc_val == 1) {
      if (enc_val == 0) value--;
      if (enc_val == 2) value++;
    } else 
    if (prev_enc_val == 2) {
      if (enc_val == 1) value--;
      if (enc_val == 3) value++;
      if (enc_val == 2) {
        // we are at a dented position; value *should* be a multiple of 4; if not round
        if (value % 4 != 0) {
          value = ((value + 2) >> 2) << 2;
  	}
      }      
    } else  
    if (prev_enc_val == 3) {
      if (enc_val == 2) value--;
      if (enc_val == 0) value++;
    }
    prev_enc_val = enc_val;
    
    loopTime = currentTime;  // Updates loopTime
  }



  // Other processing can be done here



  if(currentTime >= (printTime + 50)){

  
    lcd.setCursor(2, 2);
    lcd.print(samples*20); 
    lcd.print("  ");
    samples=0;
   
    if (value != prev_value) {
        lcd.setCursor(10, 2);
        lcd.print(value/4);
        lcd.print("   ");
        prev_value=value;
    }
     

    
 
    printTime = currentTime;
  }                             
  
}



