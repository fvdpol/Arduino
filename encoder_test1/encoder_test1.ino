/*

 */
 
unsigned long currentTime;
unsigned long loopTime = 0;
unsigned long printTime = 0;
const int pin_A = 22; 
const int pin_B = 23; 


int prev_enc_val = -1;

long value = 0;

unsigned long samples = 0;


void setup(){
  //start serial connection
  Serial.begin(57600);

  pinMode(pin_A, INPUT_PULLUP);
  pinMode(pin_B, INPUT_PULLUP);

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
  
//  if(currentTime >= (loopTime + 2)){
    // 5ms since last check of encoder = 500Hz  
    
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
    
//    loopTime = currentTime;  // Updates loopTime
//  }
  // Other processing can be done here

  if(currentTime >= (printTime + 100)){

    //print out the value of the pushbutton
    Serial.print(samples);
    samples=0;
    Serial.print(" - ");
    Serial.println(value);
 
    printTime = currentTime;
  }                             
  
}



