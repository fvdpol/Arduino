// Configure some values in EEPROM for easy config of the RF12 later on.
// 2009-05-06 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

// this version adds flash memory support, 2009-11-19

#include <JeeLib.h>
#include <util/crc16.h>
#include <util/parity.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>


#define LED_PIN     13   // activity LED, comment out to disable

#define COLLECT 0x20 // collect mode, i.e. pass incoming without sending acks

static unsigned long now () {
    // FIXME 49-day overflow
    return millis() / 1000;
}

static void activityLed (byte on) {
#ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
//    digitalWrite(LED_PIN, !on);
    digitalWrite(LED_PIN, on);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// RF12 configuration setup code

typedef struct {
    byte nodeId;
    byte group;
    char msg[RF12_EEPROM_SIZE-4];
    word crc;
} RF12Config;

static RF12Config config;

static char cmd;
static byte value, stack[RF12_MAXDATA], top, sendLen, dest, quiet;
static byte testbuf[RF12_MAXDATA], testCounter;

static void addCh (char* msg, char c) {
    byte n = strlen(msg);
    msg[n] = c;
}

static void addInt (char* msg, word v) {
    if (v >= 10)
        addInt(msg, v / 10);
    addCh(msg, '0' + v % 10);
}

static void saveConfig () {
    // set up a nice config string to be shown on startup
    memset(config.msg, 0, sizeof config.msg);
    strcpy(config.msg, " ");
    
    byte id = config.nodeId & 0x1F;
    addCh(config.msg, '@' + id);
    strcat(config.msg, " i");
    addInt(config.msg, id);
    if (config.nodeId & COLLECT)
        addCh(config.msg, '*');
    
    strcat(config.msg, " g");
    addInt(config.msg, config.group);
    
    strcat(config.msg, " @ ");
    static word bands[4] = { 315, 433, 868, 915 };
    word band = config.nodeId >> 6;
    addInt(config.msg, bands[band]);
    strcat(config.msg, " MHz ");
    
    config.crc = ~0;
    for (byte i = 0; i < sizeof config - 2; ++i)
        config.crc = _crc16_update(config.crc, ((byte*) &config)[i]);

    // save to EEPROM
    for (byte i = 0; i < sizeof config; ++i) {
        byte b = ((byte*) &config)[i];
        eeprom_write_byte(RF12_EEPROM_ADDR + i, b);
    }
    
    if (!rf12_config())
        Serial.println("config save failed");
}



static void discardInput () {
    while (Serial.read() >= 0)
        ;
}


char helpText1[] PROGMEM = 
    "\n"
    "Available commands:" "\n"
    "  <nn> i     - set node ID (standard node ids are 1..26)" "\n"
    "               (or enter an uppercase 'A'..'Z' to set id)" "\n"
    "  <n> b      - set MHz band (4 = 433, 8 = 868, 9 = 915)" "\n"
    "  <nnn> g    - set network group (RFM12 only allows 212, 0 = any)" "\n"
    "  <n> c      - set collect mode (advanced, normally 0)" "\n"
    "  t          - broadcast max-size test packet, with ack" "\n"
    "  ...,<nn> a - send data packet to node <nn>, with ack" "\n"
    "  ...,<nn> s - send data packet to node <nn>, no ack" "\n"
    "  <n> l      - turn activity LED on PB1 on or off" "\n"
    "  <n> q      - set quiet mode (1 = don't report bad packets)" "\n"
;

static void showString (PGM_P s) {
    for (;;) {
        char c = pgm_read_byte(s++);
        if (c == 0)
            break;
        if (c == '\n')
            Serial.print('\r');
        Serial.print(c);
    }
}

static void showHelp () {
    showString(helpText1);
    Serial.println("Current configuration:");
    rf12_config();
}

static void handleInput (char c) {
    if ('0' <= c && c <= '9')
        value = 10 * value + c - '0';
    else if (c == ',') {
        if (top < sizeof stack)
            stack[top++] = value;
        value = 0;
    } else if ('a' <= c && c <='z') {
        Serial.print("> ");
        Serial.print((int) value);
        Serial.println(c);
        switch (c) {
            default:
                showHelp();
                break;
            case 'i': // set node id
                config.nodeId = (config.nodeId & 0xE0) + (value & 0x1F);
                saveConfig();
                break;
            case 'b': // set band: 4 = 433, 8 = 868, 9 = 915
                value = value == 8 ? RF12_868MHZ :
                        value == 9 ? RF12_915MHZ : RF12_433MHZ;
                config.nodeId = (value << 6) + (config.nodeId & 0x3F);
                saveConfig();
                break;
            case 'g': // set network group
                config.group = value;
                saveConfig();
                break;
            case 'c': // set collect mode (off = 0, on = 1)
                if (value)
                    config.nodeId |= COLLECT;
                else
                    config.nodeId &= ~COLLECT;
                saveConfig();
                break;
            case 't': // broadcast a maximum size test packet, request an ack
                cmd = 'a';
                sendLen = RF12_MAXDATA;
                dest = 0;
                for (byte i = 0; i < RF12_MAXDATA; ++i)
                    testbuf[i] = i + testCounter;
                Serial.print("test ");
                Serial.println((int) testCounter); // first byte in test buffer
                ++testCounter;
                break;
            case 'a': // send packet to node ID N, request an ack
            case 's': // send packet to node ID N, no ack
                cmd = c;
                sendLen = top;
                dest = value;
                memcpy(testbuf, stack, top);
                break;
            case 'l': // turn activity LED on or off
                activityLed(value);
                break;
            case 'q': // turn quiet mode on or off (don't report bad packets)
                quiet = value;
                break;
        }
        value = top = 0;
        memset(stack, 0, sizeof stack);
    } else if ('A' <= c && c <= 'Z') {
        config.nodeId = (config.nodeId & 0xE0) + (c & 0x1F);
        saveConfig();
    } else if (c > ' ')
        showHelp();
}

void setup() {
    Serial.begin(57600);
    Serial.print("\n[RF12demo.8 FOR 2560]");

    if (rf12_config()) {
        config.nodeId = eeprom_read_byte(RF12_EEPROM_ADDR);
        config.group = eeprom_read_byte(RF12_EEPROM_ADDR + 1);
    } else {
        config.nodeId = 0x41; // node A1 @ 433 MHz
        config.group = 0xD4;
        saveConfig();
    }
    Serial.print("\n>>> rf12_config()\n"); 
    
    showHelp();
}

void loop() {
    if (Serial.available())
        handleInput(Serial.read());

    if (rf12_recvDone()) {
        byte n = rf12_len;
        if (rf12_crc == 0) {
            activityLed(1);
        
            Serial.print("OK");
        } else {
            if (quiet)
                return;
            Serial.print(" ?");
            if (n > 20) // print at most 20 bytes if crc is wrong
                n = 20;
        }
        if (config.group == 0) {
            Serial.print("G ");
            Serial.print((int) rf12_grp);
        }
        Serial.print(' ');
        Serial.print((int) rf12_hdr);
        for (byte i = 0; i < n; ++i) {
            Serial.print(' ');
            Serial.print((int) rf12_data[i]);
        }
        Serial.println();
        
        if (rf12_crc == 0) {
            
            if (RF12_WANTS_ACK && (config.nodeId & COLLECT) == 0) {
                Serial.println(" -> ack");
                rf12_sendStart(RF12_ACK_REPLY, 0, 0);
            }            
        }
        activityLed(0);
    }

    if (cmd && rf12_canSend()) {
        activityLed(1);

        Serial.print(" -> ");
        Serial.print((int) sendLen);
        Serial.println(" b");
        byte header = cmd == 'a' ? RF12_HDR_ACK : 0;
        if (dest)
            header |= RF12_HDR_DST | dest;
        rf12_sendStart(header, testbuf, sendLen);
        cmd = 0;

        activityLed(0);
    }
}
