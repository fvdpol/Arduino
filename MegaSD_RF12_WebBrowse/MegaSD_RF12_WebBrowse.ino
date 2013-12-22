/*
 * USERS OF ARDUINO 0023 AND EARLIER: use the 'SDWebBrowse.pde' sketch...
 * 'SDWebBrowse.ino' can be ignored.
 * USERS OF ARDUINO 1.0 AND LATER: **DELETE** the 'SDWebBrowse.pde' sketch
 * and use ONLY the 'SDWebBrowse.ino' file.  By default, BOTH files will
 * load when using the Sketchbook menu, and the .pde version will cause
 * compiler errors in 1.0.  Delete the .pde, then load the sketch.
 *
 * I can't explain WHY this is necessary, but something among the various
 * libraries here appears to be wreaking inexplicable havoc with the
 * 'ARDUINO' definition, making the usual version test unusable (BOTH
 * cases evaluate as true).  FML.
 */

/*
 * This sketch uses the microSD card slot on the Arduino Ethernet shield
 * to serve up files over a very minimal browsing interface
 *
 * Some code is from Bill Greiman's SdFatLib examples, some is from the
 * Arduino Ethernet WebServer example and the rest is from Limor Fried
 * (Adafruit) so its probably under GPL
 *
 * Tutorial is at http://www.ladyada.net/learn/arduino/ethfiles.html
 * Pull requests should go to http://github.com/adafruit/SDWebBrowse
 */

#include <SdFat.h>
#include <SdFatUtil.h>
#include <Ethernet.h>
#include <EthernetUdp.h>  
#include <Time.h>  
#include <SPI.h>
#include <SD.h>
#include <JeeLib.h>
#include <TagExchange.h>


// defintion for the chip-select pins. all this equipment is sharing the SPI bus
const int SS_Ethernet  = 10;  
const int SS_SD        = 4;  
const int SS_RF12      = 53;  




#define NODE_ID 9
/*#define GROUP_ID 212 */
#define GROUP_ID 235 

#define TRANSMIT_INTERVAL 150  /* in 0.1 s */



enum { 
  TASK_TRANSMIT,
  TASK_LIMIT };

static word schedBuf[TASK_LIMIT];
Scheduler scheduler (schedBuf, TASK_LIMIT);

TagExchangeRF12 tagExchangeRF12;



/************ ETHERNET STUFF ************/
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 1, 80 };
byte gateway[] = { 192, 168, 1, 1 };  
//byte timeServer[] = { 192, 168, 1, 1 };  
//IPAddress timeServer(192, 43, 244, 18); // time.nist.gov NTP server
IPAddress timeServer(192, 168, 1, 1); // local NTP


EthernetServer server(80);

                                          // seconds to add to UTCtime to consider correct timezone  
// const unsigned long UTCcorrection = 7200; // CET = UTC + 1 (+2 in summer):  (HOW to properly calculate DST?)  
const unsigned long UTCcorrection = 0;    // keep UTC  
const int NTP_PACKET_SIZE= 48;            // NTP time stamp is in the first 48 bytes of the message  
byte packetBuffer[NTP_PACKET_SIZE];       // buffer to hold incoming and outgoing packets  
unsigned int localPort = 8989;            // local port to listen for UDP packets  

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

const unsigned long seventyYears = 2208988800UL;       
unsigned long epoch = seventyYears;  

time_t syncInterval = 60UL;         // ntp sync every syncinterval seconds  
time_t lastSyncTime = 0UL;          

/************ SDCARD STUFF ************/
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

// store error strings in flash to save RAM
#define error(s) error_P(PSTR(s))

void error_P(const char* str) {
  PgmPrint("error: ");
  SerialPrintln_P(str);
  if (card.errorCode()) {
    PgmPrint("SD error: ");
    Serial.print(card.errorCode(), HEX);
    Serial.print(',');
    Serial.println(card.errorData(), HEX);
  }
  while(1);
}

void setup() {
  Serial.begin(57600);
 
  PgmPrint("Free RAM: ");
  Serial.println(FreeRam());  
  
  // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
  // breadboards.  use SPI_FULL_SPEED for better performance.
  pinMode(53, OUTPUT);                       // set the SS pin as an output (necessary!)
  
  digitalWrite(53, HIGH);                    // but turn off the W5100 chip!

  pinMode(4, OUTPUT);      // SD SS
  pinMode(10, OUTPUT);     // ETH SS
  pinMode(53, OUTPUT);     // RF12M SS
 
  digitalWrite(4, HIGH); 
  digitalWrite(10, HIGH); 
  digitalWrite(53, HIGH); 
  
  
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH); // davekw7x: If it's low, the Wiznet chip corrupts the SPI bus
  
  delay(100);


//  if (!card.init(SPI_HALF_SPEED, 4)) error("card.init failed!");
    
    // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  while (!card.init(SPI_HALF_SPEED, 4)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card is inserted?");
    Serial.println("* Is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    delay(5000); 
  } 
  
  // print the type of card
  Serial.print("\nCard type: ");
  switch(card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }
  
  
  
  // initialize a FAT volume
//  if (!volume.init(&card)) error("vol.init failed!");
  
  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    return;
  }


  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("\nVolume type is FAT");
  Serial.println(volume.fatType(), DEC);
  Serial.println();
  
  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize *= 512;                            // SD card blocks are always 512 bytes
  Serial.print("Volume size (bytes): ");
  Serial.println(volumesize);
  Serial.print("Volume size (Kbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Mbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);




  PgmPrint("Volume is FAT");
  Serial.println(volume.fatType(),DEC);
  Serial.println();
  
  if (!root.openRoot(&volume)) error("openRoot failed");

  // list file in root with date and size
  PgmPrintln("Files found in root:");
  root.ls(LS_DATE | LS_SIZE);
  Serial.println();
  
  // Recursive list of all directories
  PgmPrintln("Files found in all dirs:");
  root.ls(LS_R);
  
  Serial.println();
  PgmPrintln("Done");
  
  
    
//  rf12_initialize (NODE_ID, RF12_868MHZ, GROUP_ID);
  

  // test transmission
  //scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
  scheduler.timer(TASK_TRANSMIT, 10);
  
  // register callback functions for the tag updates
  tagExchangeRF12.registerFloatHandler(&TagUpdateFloatHandler);
  tagExchangeRF12.registerLongHandler(&TagUpdateLongHandler);
  
  
  
  // Debugging complete, we start the server!
  Ethernet.begin(mac, ip);
  server.begin();
}

void ListFiles(EthernetClient client, uint8_t flags) {
  // This code is just copied from SdFile.cpp in the SDFat library
  // and tweaked to print to the client output in html!
  dir_t p;
  
  root.rewind();
  client.println("<ul>");
  while (root.readDir(p) > 0) {
    // done if past last used entry
    if (p.name[0] == DIR_NAME_FREE) break;

    // skip deleted entry and entries for . and  ..
    if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.') continue;

    // only list subdirectories and files
    if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;

    // print any indent spaces
    client.print("<li><a href=\"");
    for (uint8_t i = 0; i < 11; i++) {
      if (p.name[i] == ' ') continue;
      if (i == 8) {
        client.print('.');
      }
      client.print((char)p.name[i]);
    }
    client.print("\">");
    
    // print file name with possible blank fill
    for (uint8_t i = 0; i < 11; i++) {
      if (p.name[i] == ' ') continue;
      if (i == 8) {
        client.print('.');
      }
      client.print((char)p.name[i]);
    }
    
    client.print("</a>");
    
    if (DIR_IS_SUBDIR(&p)) {
      client.print('/');
    }

    // print modify date/time if requested
    if (flags & LS_DATE) {
       root.printFatDate(p.lastWriteDate);
       client.print(' ');
       root.printFatTime(p.lastWriteTime);
    }
    // print size if requested
    if (!DIR_IS_SUBDIR(&p) && (flags & LS_SIZE)) {
      client.print(' ');
      client.print(p.fileSize);
    }
    client.println("</li>");
  }
  client.println("</ul>");
}




// callback function: executed when a new tag update of type float is received
void TagUpdateFloatHandler(int tagid, unsigned long timestamp, float value)
{         
  Serial.print("Float Update Tagid=");
  Serial.print(tagid);
              
  Serial.print(" Value=");
  Serial.print(value);
               
  Serial.println();
}


// callback function: executed when a new tag update of type long is received
void TagUpdateLongHandler(int tagid, unsigned long timestamp, long value)
{
  Serial.print("Long Update Tagid=");
  Serial.print(tagid);
              
  Serial.print(" Value=");
  Serial.print(value);
               
  Serial.println();
}


  
//unsigned long sendNTPpacket(byte *address) {  
//  memset(packetBuffer, 0, NTP_PACKET_SIZE); // set all bytes in the buffer to 0  
//  // Initialize values needed to form NTP request  
//  packetBuffer[0] = 0b11100011;   // LI, Version, Mode  
//  packetBuffer[1] = 0;     // Stratum, or type of clock  
//  packetBuffer[2] = 6;     // Polling Interval  
//  packetBuffer[3] = 0xEC;  // Peer Clock Precision  
//  // 8 bytes of zero for Root Delay & Root Dispersion  
//  packetBuffer[12]  = 49;  
//  packetBuffer[13]  = 0x4E;  
//  packetBuffer[14]  = 49;  
//  packetBuffer[15]  = 52;  
//  // Now send packet requesting a timestamp to server udp port NTP 123  
//  Udp.sendPacket( packetBuffer,NTP_PACKET_SIZE, address, 123);  
//}  


// send an NTP request to the time server at the given address 
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:         
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
}
  

//void ntpTimeSync() {  
//  sendNTPpacket(timeServer); // send an NTP packet to a time server & wait if a reply is available  
//  delay(1000);    
//  if ( Udp.available() ) {    
//    Udp.readPacket(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer  
//    // the timestamp starts at byte 40 of the received packet and is four bytes,  
//    // or two words, long. First, esxtract the two words:  
//    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);  
//    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);    
//    // combine the four bytes (two words) into a longint: NTP time (seconds since Jan 1 1900)  
//    unsigned long secsSince1900 = highWord << 16 | lowWord;    
//    // NTP gives secs from 1 1 1900. Unix time starts on Jan 1 1970. In seconds, that's 2208988800:  
//    // subtract seventy years:  
//    epoch = secsSince1900 - seventyYears;    
//    Serial.print("T=");  
//  } else {  
//    Serial.print("Te!");   
//  }  
//  Serial.println(epoch);      
//  time_t t = epoch + UTCcorrection;  
//  setTime(t);    // sets arduino internal clock  
//}  
  
String getTimeString() {  
  // gives back hh:mm:ss  
  time_t t = now();  
  String s = "";  
  if (hour(t) <10) s = s + "0";  
  s = s + hour(t) + ":";  
  if (minute(t) <10) s = s + "0";  
  s = s + minute(t) + ":";  
  if (second(t) <10) s = s + "0";  
  s = s + second(t);  
  return(s);  
}  
  
  
String getDateString() {  
  // gives back dd/mm/yyyy  
  time_t t = now();  
  String s = "";  
  if (day(t) <10) s = s + "0";  
  s = s + day(t) + "/";  
  if (month(t) <10) s = s + "0";  
  s = s + month(t) + "/";  
  s = s + year(t);  
  return(s);    
}  
  



// How big our line buffer should be. 100 is plenty!
#define BUFSIZ 100

void loop()
{  
  char clientline[BUFSIZ];
  int index = 0;

  time_t t = now();  
  
//  if ((t - lastSyncTime) > syncInterval) {  
//    lastSyncTime = t;  
//    Serial.println("NTP");    
//    ntpTimeSync();  
//  }   
  

  // handle rf12 communications
//  tagExchangeRF12.poll();
 
  
  switch (scheduler.poll()) {

  case TASK_TRANSMIT:
     // reschedule every 15s
      
      Serial.println("send something over RF12");
      
      Serial.println("Timestamp");
      Serial.println(getDateString());
      Serial.println(getTimeString());
      
//      tagExchangeRF12.publishFloat(10, 1.0 * millis());
//      tagExchangeRF12.publishLong(11,  millis());
      
      
      // transmission is handled by queueing mechanism. if we don't want to wait we can trigger the sending ourselves.
      //tagExchangeRF12.publishNow();
      
      
      //sendNTPpacket(timeServer); // send an NTP packet to a time server

      scheduler.timer(TASK_TRANSMIT, TRANSMIT_INTERVAL);
    break;
    
  }  
  

#if 1
  // handle NTP reply
  if ( Udp.parsePacket() ) {  
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;  
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);               

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;  
    // print Unix time:
    Serial.println(epoch);                               


    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');  
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':'); 
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch %60); // print the second
  }
#endif  
  
  

  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean current_line_is_blank = true;
    
    // reset the input buffer
    index = 0;
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        // If it isn't a new line, add the character to the buffer
        if (c != '\n' && c != '\r') {
          clientline[index] = c;
          index++;
          // are we too big for the buffer? start tossing out data
          if (index >= BUFSIZ) 
            index = BUFSIZ -1;
          
          // continue to read more data!
          continue;
        }
        
        // got a \n or \r new line, which means the string is done
        clientline[index] = 0;
        
        // Print it out for debugging
        Serial.println(clientline);
        
        // Look for substring such as a request to get the root file
        if (strstr(clientline, "GET / ") != 0) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          
          // print all the files, use a helper to keep it clean
          client.println("<h2>Files:</h2>");
          ListFiles(client, LS_SIZE);
        } else if (strstr(clientline, "GET /") != 0) {
          // this time no space after the /, so a sub-file!
          char *filename;
          
          filename = clientline + 5; // look after the "GET /" (5 chars)
          // a little trick, look for the " HTTP/1.1" string and 
          // turn the first character of the substring into a 0 to clear it out.
          (strstr(clientline, " HTTP"))[0] = 0;
          
          // print the file we want
          Serial.println(filename);

          if (! file.open(&root, filename, O_READ)) {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/html");
            client.println();
            client.println("<h2>File Not Found!</h2>");
            break;
          }
          
          Serial.println("Opened!");
                    
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/plain");
          client.println();
          
          int16_t c;
          while ((c = file.read()) > 0) {
              // uncomment the serial to debug (slow!)
              //Serial.print((char)c);
              client.print((char)c);
          }
          file.close();
        } else {
          // everything else is a 404
          client.println("HTTP/1.1 404 Not Found");
          client.println("Content-Type: text/html");
          client.println();
          client.println("<h2>File Not Found!</h2>");
        }
        break;
      }
    }
    // give the web browser time to receive the data
    delay(1);
    client.stop();
  }
  


}
