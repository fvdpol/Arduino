#include <Ports.h>
#include <RF12.h>

#define PRINT_COUNT
#define CALC_MINMAX

#define LANG_ELECTRICITY 'E'
#define MESG_ELEC_CURRENT 'c'

typedef struct {
    char lang;
    char mesg;
    long data;
} Packet_t;

static unsigned long last;
static unsigned long rep_last;

// port 4 has ds18x20 connected
Port leds (1);
Port inputPort(3);



void setup() {
  Serial.begin(57600);
  Serial.print("\n[meter_monitor_v1 sketch]\n");

  sendLed(1);
  delay(250);
  sendLed(0);
  delay(250);
  sendLed(1);
  delay(250);
  sendLed(0);
    
    
  inputPort.mode2(INPUT); // Set AIO mode as input
  inputPort.digiWrite2(1); // Activate pull-up resistor for AIO
  rf12_config();
  //rf12_config(); // Yes, twice - see http://talk.jeelabs.net/topic/169
  rf12_easyInit(3); // Send value at most every 3 seconds

  last = millis();
}

void loop() {
    static boolean ledOn = false;
    static long int cnt = 0;
    static int min_val, max_val = 0;    
    int data = inputPort.anaRead();
    unsigned long rep_time = millis();
    unsigned long rep_interval = rep_time - rep_last;


// for electricity meter: <300 & >400
// for gas 200.. 250 ???
    
    rf12_easyPoll();
    if (!ledOn && data < 200) {
        ledOn = true;
        sendLed(1);
    } else if (ledOn && data > 250) {
        ledOn = false;
        ledBlink();
        sendLed(0);
        cnt++;
    }
    
#ifdef CALC_MINMAX    
    if (data > max_val) {
      max_val = data;
    }    
    if (data < min_val) {
      min_val = data;
    }    
#endif

    
    if (rep_interval < 0) { // millis() overflow
        rep_last = rep_time;
    } else if (rep_interval > 2000) { // 2+ sec passed
#ifdef PRINT_COUNT
      Serial.println("===");
      Serial.print("count = ");
      Serial.println(cnt);
#endif
#ifdef CALC_MINMAX      
      Serial.print("min = ");
      Serial.println(min_val);
      Serial.print("max = ");
      Serial.println(max_val);
      
      min_val=1024;
      max_val=0;  
#endif      
      
      rep_last = rep_time;
    }
}

void ledBlink() {
    static int nBlinks = 0;
    unsigned long time = millis();
    unsigned long interval = time - last;

    nBlinks++;
    if (interval < 0) { // millis() overflow
        last = time;
        nBlinks = 0;
    } else if (interval > 5000) { // 1+ sec passed
        // Blinks are 10000 per kWh, or 0.1 Wh each
        // One hour has 3.6M milliseconds
        long watts = nBlinks * 0.1 * 3.6E6 / interval;

    Serial.print(watts);
    Serial.println("W");


        wattSend(watts);
        last = time;
        nBlinks = 0;
    }
}

static void wattSend(long watts) {
    Packet_t packet;

    packet.lang = LANG_ELECTRICITY;
    packet.mesg = MESG_ELEC_CURRENT;
    packet.data = watts;
    rf12_easySend(&packet, sizeof packet);
}


static void sendLed (byte on) {
    leds.mode(OUTPUT);
    leds.digiWrite(on);
}
