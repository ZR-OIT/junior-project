//Define pins
int dataPin = 3;   // Pin 0 of DigiSpark connected to Pin 3 of CD4021
int clockPin = 2;  // Pin 1 of DigiSpark connected to Pin 10 of CD4021
int latchPin = 4;  // Pin 2 of DigiSpark connected to Pin 9 of CD4021

//Define variable
byte RegisterValue = 0;  // Used to hold data from DC4021

void setup() {

//define pins used to connect to the CD4021 Shift Register
  pinMode(dataPin, INPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT); 
  Serial.begin(115200);
}

void loop() {

  //Set latch pin to 1 to get recent data into the CD4021
  digitalWrite(latchPin,1);
  
  delayMicroseconds(20);
  
  //Set latch pin to 0 to get data from the CD4021
  digitalWrite(latchPin,0);

  //Get CD4021 register data in byte variable
  RegisterValue = shiftIn(dataPin, clockPin);
  Serial.println("\nReading motion sensor data...");
  //Serial.println(RegisterValue, BIN);
  for (int ii = 7; ii >= 0; ii--)
  {
      
      Serial.print(bitRead(RegisterValue, ii));
  }

  //Serial.println(RegisterValue, BYTE);

delay(1000);

}

byte shiftIn(int myDataPin, int myClockPin) {
  int i;
  int temp = 0;
  int pinState;
  byte myDataIn = 0;

  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, INPUT);

//we will be holding the clock pin high 8 times (0,..,7) at the
//end of each time through the for loop

//at the begining of each loop when we set the clock low, it will
//be doing the necessary low to high drop to cause the shift
//register's DataPin to change state based on the value
//of the next bit in its serial information flow.
//The register transmits the information about the pins from pin 7 to pin 0
//so that is why our function counts down
  for (i=7; i>=0; i--)
  {
    digitalWrite(myClockPin, 0);
    delayMicroseconds(2);
    temp = digitalRead(myDataPin);
    if (temp) {
      pinState = 1;
      //set the bit to 0 no matter what
      myDataIn = myDataIn | (1 << i);
    }
    else {
      //turn it off -- only necessary for debuging
     //print statement since myDataIn starts as 0
      pinState = 0;
    }

    //Debuging print statements
    //Serial.print(pinState);
    //Serial.print("     ");
    //Serial.println (dataIn, BIN);

    digitalWrite(myClockPin, 1);

  }
  //debuging print statements whitespace
  //Serial.println();
  //Serial.println(myDataIn, BIN);
  return myDataIn;
}
