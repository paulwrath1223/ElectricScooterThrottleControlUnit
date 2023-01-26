#include <Servo.h>
#include <EEPROM.h>


#define calSwitchPin 0
#define commitButtonPin 1
#define throttleInPin 2
#define brakeInPin 3
#define throttleOutputPin 4

#define reverseThrottle false
#define reverseBrake false

#define serialPrintStatements true

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

int tempOutput;

void setup() {
  // put your setup code here, to run once:
  if serialPrintStatements{
    Serial.begin(115200);
  }
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
  if serialPrintStatements{
    Serial.print("currentThrottleUpper as read from EEPROM: ");
    Serial.println(currentThrottleUpper);
    Serial.print("currentThrottleLower as read from EEPROM: ");
    Serial.println(currentThrottleLower);
    Serial.print("currentBrakeUpper as read from EEPROM: ");
    Serial.println(currentBrakeUpper);
    Serial.print("currentBrakeLower as read from EEPROM: ");
    Serial.println(currentBrakeLower);
  }
}

void loop() {
  if(!digitalRead(calSwitchPin)){    //TODO: do I need a 'not'?
    if serialPrintStatements{
      Serial.println("Starting calibration mode loop");
    }
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
    if(!calibrationMode && newConfigQueued){
      if bothInputsZero(){ //   TODO: make sure both inputs read 0
        currentThrottleUpper = potentialNewThrottleUpper;
        currentThrottleLower = potentialNewThrottleLower;
        currentBrakeUpper = potentialNewBrakeUpper;
        currentBrakeLower = potentialNewBrakeLower;

        write2bytes(throttleLowerEEPROMAddress, currentThrottleLower);
        write2bytes(throttleUpperEEPROMAddress, currentThrottleUpper);
        write2bytes(brakeLowerEEPROMAddress, currentBrakeLower);
        write2bytes(brakeUpperEEPROMAddress, currentBrakeLower);
        if serialPrintStatements{
          Serial.print("currentThrottleUpper written to EEPROM: ");
          Serial.println(currentThrottleUpper);
          Serial.print("currentThrottleLower written to EEPROM: ");
          Serial.println(currentThrottleLower);
          Serial.print("currentBrakeUpper written to EEPROM: ");
          Serial.println(currentBrakeUpper);
          Serial.print("currentBrakeLower written to EEPROM: ");
          Serial.println(currentBrakeLower);
        }
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
  tempOutput = map(constrain(value, -127, 127), -127, 127, 0, 180);
  if serialPrintStatements{
    Serial.print("new output: ");
    Serial.println(tempOutput);
  }
  output.write(tempOutput);
}

int calculate_output(int throttleIn, int BrakeIn){
  if throttleReverse && brakeReverse{
    return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 127, 0) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 127, 0));
  } else if throttleReverse && !brakeReverse{
    return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 127, 0) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 0, 127));
  } else if !throttleReverse && brakeReverse{
    return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 0, 129) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 127, 0));
  } 
  return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 0, 127) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 0, 127));
}

bool bothInputsZero(){
  if throttleReverse && brakeReverse{
    return(analogRead(throttleInPin) > potentialNewThrottleUpper) && (analogRead(brakeInPin) > potentialNewBrakeUpper);
  } else if throttleReverse && !brakeReverse{
    return(analogRead(throttleInPin) > potentialNewThrottleUpper) && (analogRead(brakeInPin) < potentialNewBrakeLower);
  } else if !throttleReverse && brakeReverse{
    return(analogRead(throttleInPin) < potentialNewThrottleLower) && (analogRead(brakeInPin) > potentialNewBrakeUpper);
  } 
  return(analogRead(throttleInPin) < potentialNewThrottleLower) && (analogRead(brakeInPin) < potentialNewBrakeLower);
}












