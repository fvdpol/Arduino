


class P1SmartMeter {
  
public:

  // constructor
  P1SmartMeter(void);
  void poll(void);


private:
  char *buf;
  int end;

  void processLine(void);
};


P1SmartMeter::P1SmartMeter(void) {
  Serial2.begin(9600,SERIAL_7E1);
    
  buf = (char *)malloc(100);
  end=0;
  buf[end]='\0';
}

void P1SmartMeter::processLine() {
        if (strncmp(buf,"1-0:1.8.1",9)==0) {
          Serial.print("Meas 1.8.1 = ");
          //Serial.println(buf+10);
          char *r = (char *) malloc(25);
          strcpy(r, buf+10);
          r[9]='\0';
          //Serial.println(r);
          float value = atof(r);
          Serial.println(value,3);
          free(r); 
        }  else 
        if (strncmp(buf,"1-0:1.8.2",9)==0) {
          Serial.print("Meas 1.8.2 = ");
          //Serial.println(buf+10);
          char *r = (char *) malloc(25);
          strcpy(r, buf+10);
          r[9]='\0';
          //Serial.println(r);
          float value = atof(r);
          Serial.println(value,3);
          free(r); 
        }  else {
           if (strlen(buf) > 0) {
            Serial.print("Received: ");
            Serial.println(buf);
          }
        }
        end=0;
        buf[end]='\0';        
      
}


void P1SmartMeter::poll(void) {
  
  while (Serial2.available()) {
    int c = Serial2.read();
    if (c > 0) {      
      if ((c >= 32) && (end<90)) {
        buf[end]=c;
        end++;
        buf[end]='\0';
      } else {
        processLine();   
      }
    }
  }
}



P1SmartMeter smartmeter;

void setup() {
  // put your setup code here, to run once:

  // console
  Serial.begin(57600);
  Serial.println("p1 scan");


  // p1 port to smart meter
}




void loop() {
  // put your main code here, to run repeatedly: 
  
  smartmeter.poll();
  
}
