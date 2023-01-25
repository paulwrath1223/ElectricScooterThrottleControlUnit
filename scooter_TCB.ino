#include <Servo.h>
#include <EEPROM.h>


#define calSwitchPin 0
#define commitButtonPin 1
#define throttleInPin 2
#define brakeInPin 3
#define throttleOutputPin 4

#define throttleLowerEEPROMAddress 0 // addresses are in chunks of 2 bytes
#define throttleUpperEEPROMAddress 2
#define brakeLowerEEPROMAddress 4
#define brakeUpperEEPROMAddress 6

bool calibrationMode = false;
bool newConfigQueued = false;

int potentialNewThrottleUpper;
int potentialNewThrottleLower;
int potentialNewBrakeUpper;
int potentialNewBrakeLower;

int tempThrottleIn;
int tempBrakeIn;

int currentThrottleUpper;
int currentThrottleLower;
int currentBrakeUpper;
int currentBrakeLower;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Servo output;
  output.attach(throttleOutputPin);
  // initialize EEPROM with predefined size
  EEPROM.begin(8);
  pinMode(calSwitchPin, INPUT_PULLUP);
  pinMode(commitButtonPin, INPUT_PULLUP);

  attachInterrupt(commitButtonPin, isr, FALLING);

  currentThrottleUpper = read2bytes(throttleUpperEEPROMAddress);
  currentThrottleLower = read2bytes(throttleLowerEEPROMAddress);
  currentBrakeUpper = read2bytes(brakeUpperEEPROMAddress);
  currentBrakeLower = read2bytes(brakeLowerEEPROMAddress);
}

void loop() {
  if(!digitalRead(calSwitchPin)){    //TODO: do I need a 'not'?
    calibrationMode = true;
    newConfigQueued = false;
    output_to_esc(0);
    potentialNewThrottleUpper = INT_MIN;
    potentialNewThrottleLower = INT_MAX;
    potentialNewBrakeUpper = INT_MIN;
    potentialNewBrakeLower = INT_MAX;
    while(!digitalRead(calSwitchPin)){     //TODO: do I need a 'not'?

      tempThrottleIn = analogRead(throttleInPin);

      if(tempThrottleIn > potentialNewThrottleUpper){
        potentialNewThrottleUpper = tempThrottleIn;
      }
      if(tempThrottleIn < potentialNewThrottleLower){
        potentialNewThrottleLower = tempThrottleIn;
      }

      tempBrakeIn = analogRead(brakePinIn);

      if(tempBrakeIn > potentialNewBrakeUpper){
        potentialNewBrakeUpper = tempBrakeIn;
      }
      if(tempBrakeIn < potentialNewBrakeLower){
        potentialNewBrakeLower = tempBrakeIn;
      }
    }

    potentialNewThrottleUpper -= 100; // add deadzones
    potentialNewThrottleLower += 100;
    potentialNewBrakeUpper -= 100;
    potentialNewBrakeLower += 100;

    if(((potentialNewThrottleUpper - potentialNewThrottleLower) > 200) && ((potentialNewBrakeUpper - potentialNewBrakeLower) > 200)){
      newConfigQueued = true;
    }
    calibrationMode = false;
  } else{
    output_to_esc(calculate_output(analogRead(throttleInPin), analogRead(brakeInPin)));
  }
}

void IRAM_ATTR isr(){  // TODO: attach to commit pin interrupt
  if(digitalRead(commitButtonPin)){
    if(!calibrationMode){
      if((analogRead(throttleInPin) < potentialNewThrottleLower) && (analogRead(brakeInPin) < potentialNewBrakeLower)){ //   TODO: make sure both inputs read 0
        currentThrottleUpper = potentialNewThrottleUpper;
        currentThrottleLower = potentialNewThrottleLower;
        currentBrakeUpper = potentialNewBrakeUpper;
        currentBrakeLower = potentialNewBrakeLower;

        write2bytes(throttleLowerEEPROMAddress, currentThrottleLower);
        write2bytes(throttleUpperEEPROMAddress, currentThrottleUpper);
        write2bytes(brakeLowerEEPROMAddress, currentBrakeLower);
        write2bytes(brakeUpperEEPROMAddress, currentBrakeLower);
      }
    }
  }
}

int read2bytes(int addressToRead){
  byte byte1 = EEPROM.read(address);
  byte byte2 = EEPROM.read(address + 1);
  return ((byte1 << 8) + byte2);
}

void write2bytes(int addressToWrite, int value){
  byte byte1 = value >> 8;
  byte byte2 = value & 0xFF;
  EEPROM.write(addressToWrite, byte1);
  EEPROM.write(addressToWrite, byte2);
}

void output_to_esc(int value){
  output.write(map(constrain(value, -127, 127), -127, 127, 0, 180));
}

int calculate_output(int throttleIn, int BrakeIn){
  return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 0, 127) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 0, 127));
}














