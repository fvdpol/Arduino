
// https://github.com/FortySevenEffects/arduino_midi_library/
#include <MIDI.h>
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);



// https://www.pjrc.com/teensy/td_libs_Encoder.html
#define ENCODER_USE_INTERRUPTS
#include <Encoder.h>


// https://code.google.com/p/digitalwritefast/
// Arduino pin/port mapping: https://spreadsheets.google.com/spreadsheet/pub?key=0AtfNMvfWhA_ccnRId19SNmVWTDE0MEtTOV9HOEdQa0E&gid=0
#include <digitalWriteFast.h>



//int bit = 0;
int effect2 = 0;
int search2 = 0;
unsigned long meas_time = 0;
unsigned long meas_tick = 0;


// Serial LED Outputs
#define BU2090F_CLK    22    // PA0       -- to Denon 1-10/18 and 2-10/18
#define BU2090F_DATA1  23    // PA1       -- to Denon 1-15/18 
#define BU2090F_DATA2  24    // PA2       -- to Denon 2-15/18

// Single LED Outputs
#define LED_PLAY1      6     // PWM       -- to Denon 1-11/18
#define LED_CUE1       7     // PWM       -- to Denon 1-12/18
#define LED_PLAY2      8     // PWM       -- to Denon 2-11/18
#define LED_CUE2       9     // PWM       -- to Denon 2-12/18


#define JOG1_A         19    // irq pin   -- to Denon 1-13/18
#define JOG1_B         18    // irq pin   -- to Denon 1-14/18
#define ENC1_A         4     // irq pin   -- to Denon 1-4/12
#define ENC1_B         2     // no irq    -- to Denon 1-5/12
#define ENC1_P         28    // encoder push

#define JOG2_A         20    // irq pin   -- to Denon 2-13/18
#define JOG2_B         21    // irq pin   -- to Denon 2-14/18
#define ENC2_A         5     // irq pin   -- to Denon 2-4/12
#define ENC2_B         3     // no irq    -- to Denon 2-5/12
#define ENC2_P         29    // encoder push


#define PITCH1_FADER   A0
#define PITCH1_CENTER  A1
#define PITCH2_FADER   A2
#define PITCH2_CENTER  A3


#define KeyMatrixA0    25
#define KeyMatrixA1    26
#define KeyMatrixA2    27
#define KeyMatrixPort  PORTC



class PitchFader {
  public:
    // constructor
    // pin 1 is for value; pin 2 is for the center
    PitchFader(uint8_t pin1, uint8_t pin2) {
      _pin1 = pin1;
      _pin2 = pin2;
      value = 0.0;
    }
    
    float read(void) {
      value = (0.95 * value) + (0.05 * (analogRead(_pin1) - analogRead(_pin2)));      
      return value;
    }

  private:
    uint8_t _pin1;
    uint8_t _pin2;
    float value;

};




class Keypad {
  public:
    // constructor
    Keypad() {
      // http://arduino.cc/en/Hacking/PinMapping2560
      // switches connected to Mega port 30..37 --> PORTC
      DDRC  = 0b00000000;    //All pins in PORTC are inputs
      PORTC = 0b11111111;    //Pull-ups enabled in all the pins

      pinModeFast(KeyMatrixA0, OUTPUT);
      pinModeFast(KeyMatrixA1, OUTPUT);
      pinModeFast(KeyMatrixA2, OUTPUT); 
    }
    
  uint8_t readcolumn(uint8_t col) {
     
    digitalWriteFast(KeyMatrixA0, (col & 0x1) != 0);
    digitalWriteFast(KeyMatrixA1, (col & 0x2) != 0);
    digitalWriteFast(KeyMatrixA2, (col & 0x4) != 0);  
    delayMicroseconds(2);  // delay to allow the 74HC138 to settle...

    // port 30..37 can be accessed via PORTC hardware
    
    return ~PINC;        //Read the PORTD and put the values in the variable

  }
    
  private:
};



// control the bank of serialised leds
class Leds {
  public:
    // constructor
    Leds() {
      
      // Rohm BU2090 for driving LEDS -- 12 bit serial in, parallel out driver
      pinModeFast(BU2090F_CLK, OUTPUT);  // clock
      pinModeFast(BU2090F_DATA1, OUTPUT);  // data A
      pinModeFast(BU2090F_DATA2, OUTPUT);  // data B
      
      _leds[1] = 0;
      _leds[2] = 0;
      update_leds();
    }
    
    
    // update to send both data packed for left and right deck at same time
    //
    // implementation using standard Arduino digitalWrite takes around 500 microseconds, version using digitalWriteFast takes around 180 microseconds
    //
    // datasheet: http://rohmfs.rohm.com/en/products/databook/datasheet/ic/logic_switch/serial_parallel/bu2090xx-e.pdf
    //
    void SendBU2090F(unsigned int ch1, unsigned int ch2) {
      digitalWriteFast(BU2090F_CLK, LOW);  
      
      for (int b=0; b<12; b++) {
        // set data
        digitalWriteFast(BU2090F_DATA1, (ch1 & (1<<b)) != 0);
        digitalWriteFast(BU2090F_DATA2, (ch2 & (1<<b)) != 0);
          
        // BU2090F clocks data on rising edge
        //delayMicroseconds(1);  
        digitalWriteFast(BU2090F_CLK, HIGH);
        delayMicroseconds(1);
        if (b<11) {
          digitalWriteFast(BU2090F_DATA1, LOW);
          digitalWriteFast(BU2090F_DATA2, LOW);
        } else {
          digitalWriteFast(BU2090F_DATA1, HIGH);  // data high on last falling edge to force latching
          digitalWriteFast(BU2090F_DATA2, HIGH);  // data high on last falling edge to force latching
        }
        //delayMicroseconds(1);    
        digitalWriteFast(BU2090F_CLK, LOW);  
        digitalWriteFast(BU2090F_DATA1, LOW);
        digitalWriteFast(BU2090F_DATA2, LOW);
        //delayMicroseconds(1);    
      } 
    }


  void set(int channel, int led, boolean state) {
    //if (value(channel, led) != state) { // do nothing unless we change state
      if (state) {
        _leds[channel] |= (1<<led);
        if (led==5) {_leds[channel] |= (1<<6); }
      } else {
        _leds[channel] &= ~(1<<led);
        if (led==5) {_leds[channel] &= ~(1<<6); }
      }
      Serial.print("LED ");
      Serial.print(channel);
      Serial.print(",");
      Serial.print(led);
      Serial.print(" -> ");
      Serial.println(state);  
      update_leds(); 
//    } 
  }

  inline boolean value(int channel, int led) {
//    Serial.print(channel);
//    Serial.print(",");
//    Serial.print(led);
//    Serial.print(" val=");
//    Serial.println(_leds[channel] & (1<<led));  
    return ((_leds[channel] & (1<<led)) != 0);
  }
    

  void on(int channel, int led) {
    set(channel, led, true);
  }
  
  void off(int channel, int led) {
    set(channel, led, false);
  }
  
  void toggle(int channel, int led) {
    if (value(channel, led) == false) {
      on(channel, led);
    } else {
      off(channel, led);
    }
  }
  
  private:
    unsigned int _leds[3];
    
    void update_leds(void) {
      SendBU2090F(_leds[1] ^ 1023, _leds[2] ^ 1023);
    }
};


// class to detect change in value
// the holdoff value specifies the minimum number of milliseconds between reported values
class ChangeLong {
  public:
    // constructor
    ChangeLong(int holdoff) {
      _value = -1E9L;
      _holdoff = holdoff;
      ms = millis()+_holdoff;
    }
    
    // returns true if value has changed and minimum holdoff time reached
    boolean changed(long value) {
      if (millis() < ms) 
        return false;
        
      if (value == _value) {
        return false;
      } else {
        _value = value;
        ms = millis()+_holdoff;
        return true;
      }
    }
    
    // returns the changed value itself
    long value(void) {
      return _value;
    }
    
    private:
      long _value;
      int _holdoff;
      unsigned long ms;
      
};

// class to detect change in value
class ChangeInt {
  public:
    // constructor
    ChangeInt(int holdoff = 0) {
      _value = -1E4;
      _holdoff = holdoff;
      ms = millis()+_holdoff;
    }
    
    // returns true if value has changed
    boolean changed(int value) {
      if (millis() < ms) 
        return false;

      if (value == _value) {
        return false;
      } else {
        _value = value;
        ms = millis()+_holdoff;
        return true;
      }
    }
    
    // returns the changed value itself
    int value(void) {
      return _value;
    }
    
    private:
      int _value;
      int _holdoff;
      unsigned long ms;
};




PitchFader Pitch1(PITCH1_FADER,PITCH1_CENTER);
PitchFader Pitch2(PITCH2_FADER,PITCH2_CENTER);

ChangeInt Pitch1Change(50);
ChangeInt Pitch2Change(50);


Encoder knobJog1(JOG1_A, JOG1_B);
Encoder knobJog2(JOG2_A, JOG2_B);
Encoder knobEnc1(ENC1_A, ENC1_B);
Encoder knobEnc2(ENC2_A, ENC2_B);


ChangeLong KnobJog1Change(20);
ChangeLong KnobEnc1Change(20);
ChangeInt  KnobEnc1Push(20);
ChangeLong KnobJog2Change(20);
ChangeLong KnobEnc2Change(20);
ChangeInt  KnobEnc2Push(20);

Keypad ButtonRow;
ChangeInt Buttons[8];
ChangeInt Button[8][8];


Leds LedDisplay;


// code to display the sketch name/compile date
// http://forum.arduino.cc/index.php?PHPSESSID=82046rhab9h76mt6q7p8fle0u4&topic=118605.msg894017#msg894017

int pgm_lastIndexOf(uint8_t c, const char * p)
{
  int last_index = -1; // -1 indicates no match
  uint8_t b;
  for(int i = 0; true; i++) {
    b = pgm_read_byte(p++);
    if (b == c)
      last_index = i;
    else if (b == 0) break;
  }
  return last_index;
}

// displays at startup the Sketch running in the Arduino

void display_srcfile_details(void){
  const char *the_path = PSTR(__FILE__);           // save RAM, use flash to hold __FILE__ instead

  int slash_loc = pgm_lastIndexOf('/',the_path); // index of last '/' 
  if (slash_loc < 0) slash_loc = pgm_lastIndexOf('\\',the_path); // or last '\' (windows, ugh)

  int dot_loc = pgm_lastIndexOf('.',the_path);   // index of last '.'
  if (dot_loc < 0) dot_loc = pgm_lastIndexOf(0,the_path); // if no dot, return end of string

  Serial.print("\nSketch: ");

  for (int i = slash_loc+1; i < dot_loc; i++) {
    uint8_t b = pgm_read_byte(&the_path[i]);
    if (b != 0) Serial.print((char) b);
    else break;
  }

  Serial.print(", Compiled on: ");
  Serial.print(__DATE__);
  Serial.print(" at ");
  Serial.print(__TIME__);
  Serial.print("\n");
}



int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}







void setup() {
  Serial.begin(57600);
  
  display_srcfile_details();  
  
  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());


  // encoder push switch
  pinMode(ENC1_P, INPUT_PULLUP);
  pinMode(ENC2_P, INPUT_PULLUP);
  
  
  MIDI.begin(1);          // Launch MIDI and listen to channel 1
  MIDI.turnThruOff();
}



void loop() {
  
  unsigned long ms = millis();
  meas_tick++;
  if (ms > meas_time) {
    float perf = 1000.0 * meas_tick / (10000.0 + ms - meas_time);
    Serial.print("Performance count: ");
    Serial.print(perf);
    Serial.println(" loops/sec");
    
    meas_time=ms+10000L;
    meas_tick=0L;    
  }
  
  
  if (Pitch1Change.changed(int(Pitch1.read() + 0.5))) {
      Serial.print("Pitch 1\t");    
      Serial.println(Pitch1Change.value());
  }

  if (Pitch2Change.changed(int(Pitch2.read() + 0.5))) {
      Serial.print("Pitch 2\t");    
      Serial.println(Pitch2Change.value());
  }



  
  // encoder handling 
  if (KnobJog1Change.changed(knobJog1.read())) {
    Serial.print("Jog 1\t");
    Serial.println(KnobJog1Change.value());
  }

  if (KnobJog2Change.changed(knobJog2.read())) {
    Serial.print("Jog 2\t");
    Serial.println(KnobJog2Change.value());
  }


  // the rotary encoder is detented and does report in multiples of 4
  if (KnobEnc1Change.changed((knobEnc1.read()+2)/4)) {
    Serial.print("Enc 1\t");
    Serial.println(KnobEnc1Change.value());
  }
 
  if (KnobEnc1Push.changed(digitalReadFast(ENC1_P) == 0)) {
    Serial.print("Enc 1 Push \t");
    Serial.println(KnobEnc1Push.value());
  }

  // the rotary encoder is detented and does report in multiples of 4
  if (KnobEnc2Change.changed((knobEnc2.read()+2)/4)) {
    Serial.print("Enc 2\t");
    Serial.println(KnobEnc2Change.value());
  }
  
  if (KnobEnc2Push.changed(digitalReadFast(ENC2_P) == 0)) {
    Serial.print("Enc 2 Push \t");
    Serial.println(KnobEnc2Push.value());
  }
  
  //XX this reset needs to be done periodically? as values will be reported incrementally to Traktor
  //alternative, reset if value becomes too large (> +/- 1E9)
  
  // if a character is sent from the serial monitor,
  // reset both back to zero.
//  if (Serial.available()) {
//    Serial.read();
//    Serial.println("Reset both knobs to zero");
//    knobJog1.write(0);
//    knobJog2.write(0);
//    knobEnc1.write(0);
//    knobEnc2.write(0);
//  }

 int leds1 = 0; 
 int leds2 = 0; 

  // keyboard matrix scanning
  for (int x=0; x<=7; x++) {
    if (Buttons[x].changed(ButtonRow.readcolumn(x))) {
      for (int y=0; y<=7; y++) {
         if (Button[x][y].changed((Buttons[x].value() & (1<<y))!=0)) {
            Serial.print("Button \t[");
            Serial.print(x);
            Serial.print(",");
            Serial.print(y);
            Serial.print("]\t");
            Serial.println(Button[x][y].value());
            
            

            if (x==1 && y==5 && Button[x][y].value()) 
              LedDisplay.toggle(1,4);

            if (x==5 && y==5 && Button[x][y].value()) 
              LedDisplay.toggle(1,0);

            if (x==5 && y==3 && Button[x][y].value()) 
              LedDisplay.toggle(1,10);

            if (x==1 && y==3 && Button[x][y].value()) 
              LedDisplay.toggle(1,11);

            if (x==6 && y==3 && Button[x][y].value()) 
              LedDisplay.toggle(1,5);

            if (x==2 && y==7 && Button[x][y].value()) 
              LedDisplay.toggle(1,9);
            if (x==6 && y==7 && Button[x][y].value()) 
              LedDisplay.toggle(1,8);
            if (x==6 && y==5 && Button[x][y].value()) 
              LedDisplay.toggle(1,7);




            if (x==1 && y==4 && Button[x][y].value()) 
              LedDisplay.toggle(2,4);

            if (x==5 && y==4 && Button[x][y].value()) 
              LedDisplay.toggle(2,0);

            if (x==5 && y==2 && Button[x][y].value()) 
              LedDisplay.toggle(2,10);

            if (x==1 && y==2 && Button[x][y].value()) 
              LedDisplay.toggle(2,11);

            if (x==6 && y==2 && Button[x][y].value()) 
              LedDisplay.toggle(2,5);

            if (x==2 && y==6 && Button[x][y].value()) 
              LedDisplay.toggle(2,9);
            if (x==6 && y==6 && Button[x][y].value()) 
              LedDisplay.toggle(2,8);
            if (x==6 && y==4 && Button[x][y].value()) 
              LedDisplay.toggle(2,7);
 
             
             
            // simple hack at possible "Jog Mode toggle" 
              
            if (x==4 && y==0 && Button[x][y].value()) { // search mode
              search2 = true;
              LedDisplay.off(2,2);  // digi-s
              LedDisplay.off(2,1);  // key
              LedDisplay.off(2,3);  // bend
            }
            if (x==0 && y==0 && Button[x][y].value()) { // effect mode
              if (search2 == false) { 
                effect2 +=1;
                if (effect2 > 2) { 
                  effect2 = 0;
                }  
              }
              search2 = false;
              if (effect2 == 0) {
                LedDisplay.on(2,2);  // digi-s
                LedDisplay.off(2,1);  // key
                LedDisplay.off(2,3);  // bend
              } 
              if (effect2 == 1) {
                LedDisplay.off(2,2);  // digi-s
                LedDisplay.on(2,1);  // key
                LedDisplay.off(2,3);  // bend
              } 
              if (effect2 == 2) {
                LedDisplay.off(2,2);  // digi-s
                LedDisplay.off(2,1);  // key
                LedDisplay.on(2,3);  // bend
              }
            }
              
            
         }
      }      
    }
  }


// internal processing for jog mode?
// search:  search mode
// search|effect -> effect mode (last effect)
// effect|digis  -> effect key
// effect|key    -> effect bend
// effect|bend   --> effect digis
//               


  
}
