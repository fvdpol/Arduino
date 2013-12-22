
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

void setup()
{
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
  
  lcd.setCursor(0, 0);
  for(int i = 0;i < 20; i++)  lcd.printByte(6);
  lcd.setCursor(0, 1);
  lcd.printByte(6);
  lcd.print("   Hello world    ");
  lcd.printByte(6);
  lcd.setCursor(0, 2);
  lcd.printByte(6);
  lcd.print("  i ");
  lcd.printByte(3);
  lcd.print(" arduinos!   ");
  lcd.printByte(6);
  lcd.setCursor(0, 3);
  for(int i = 0;i < 20; i++)  lcd.printByte(6);
//  lcd.clear();

}

void loop()
{
  lcd.setBacklight(LED_ON);
  delay(2000);
  lcd.setBacklight(LED_OFF);
  delay(200);
}
