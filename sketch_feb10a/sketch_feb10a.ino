#include <Ports.h>
#include <RF12.h>

Port CounterA (1);
Port CounterB (2);

MilliTimer RepTimer;
MilliTimer Led1Timer;
MilliTimer Led2Timer;

byte led1;
byte led2;

void toggle1() {
  if (led1 == 0) {
    led1 =1;
  } else {
    led1 = 0;
  }
   CounterA.digiWrite(led1);   
}

void toggle2() {
  if (led2 == 0) {
    led2 =1;
  } else {
    led2 = 0;
  }
   CounterB.digiWrite(led2);   
}


void setup() {
  Serial.begin(57600);
  Serial.print("\n[meter_monitor_v1 sketch]\n");

  CounterA.mode(OUTPUT);
  CounterB.mode(OUTPUT);

  CounterA.digiWrite(0);
  CounterB.digiWrite(0);

  CounterA.mode2(INPUT); // Set AIO mode as input
  CounterA.digiWrite2(1); // Activate pull-up resistor for AIO

  CounterB.mode2(INPUT); // Set AIO mode as input
  CounterB.digiWrite2(1); // Activate pull-up resistor for AIO

  rf12_config();
  //rf12_config(); // Yes, twice - see http://talk.jeelabs.net/topic/169
  rf12_easyInit(3); // Send value at most every 3 seconds
}

void loop() {
  int dataA = CounterA.anaRead();
  int dataB = CounterB.anaRead();

  rf12_easyPoll();

  if (RepTimer.poll(2000)) {
    Serial.print("loop...\n");
    Serial.println(dataA);
    Serial.println(dataB);
  }

  if (Led1Timer.poll(100)) {
    toggle1();
  }

  if (Led2Timer.poll(101)) {
    toggle2();
  }
  
}




