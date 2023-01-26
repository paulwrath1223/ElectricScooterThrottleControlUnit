#include <ESP32Servo.h>
#include <EEPROM.h>


#define calSwitchPin 32
#define commitButtonPin 33
#define throttleInPin 35
#define brakeInPin 34
#define throttleOutputPin 23

#define reverseThrottle false
#define reverseBrake false

#define serialPrintStatements true

#define throttleLowerEEPROMAddress 0 // addresses are in chunks of 2 bytes
#define throttleUpperEEPROMAddress 2
#define brakeLowerEEPROMAddress 4
#define brakeUpperEEPROMAddress 6

bool calibrationMode = false;
bool newConfigQueued = false;
bool failedBootSanityCheck = false;

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

Servo output;

void IRAM_ATTR isr(){  
  if(digitalRead(commitButtonPin)){
    if (serialPrintStatements){
      Serial.println("commit interrupt triggered")
    }
    if(!calibrationMode && newConfigQueued){
      if (bothInputsZero()){ //   make sure both inputs read 0
        currentThrottleUpper = potentialNewThrottleUpper;
        currentThrottleLower = potentialNewThrottleLower;
        currentBrakeUpper = potentialNewBrakeUpper;
        currentBrakeLower = potentialNewBrakeLower;

        failedBootSanityCheck = false; // even though a new boot has not happened, recalibrating should override a failed boot sanity check

        write2bytes(throttleLowerEEPROMAddress, currentThrottleLower);
        write2bytes(throttleUpperEEPROMAddress, currentThrottleUpper);
        write2bytes(brakeLowerEEPROMAddress, currentBrakeLower);
        write2bytes(brakeUpperEEPROMAddress, currentBrakeLower);
        if (serialPrintStatements){
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
  byte byte1 = EEPROM.read(addressToRead);
  byte byte2 = EEPROM.read(addressToRead + 1);
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
  if (serialPrintStatements){
    Serial.print("new output: ");
    Serial.println(tempOutput);
  }
  output.write(tempOutput);
}

int calculate_output(int throttleIn, int brakeIn){
  if (reverseThrottle && reverseBrake){
    return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 127, 0) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 127, 0));
  } else if (reverseThrottle && !reverseBrake){
    return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 127, 0) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 0, 127));
  } else if (!reverseThrottle && reverseBrake){
    return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 0, 129) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 127, 0));
  } 
  return(map(throttleIn, currentThrottleLower, currentThrottleUpper, 0, 127) - map(brakeIn, currentBrakeLower, currentBrakeUpper, 0, 127));
}

bool bothInputsZero(){
  if (reverseThrottle && reverseBrake){
    return(analogRead(throttleInPin) > potentialNewThrottleUpper) && (analogRead(brakeInPin) > potentialNewBrakeUpper);
  } else if (reverseThrottle && !reverseBrake){
    return(analogRead(throttleInPin) > potentialNewThrottleUpper) && (analogRead(brakeInPin) < potentialNewBrakeLower);
  } else if (!reverseThrottle && reverseBrake){
    return(analogRead(throttleInPin) < potentialNewThrottleLower) && (analogRead(brakeInPin) > potentialNewBrakeUpper);
  } 
  return(analogRead(throttleInPin) < potentialNewThrottleLower) && (analogRead(brakeInPin) < potentialNewBrakeLower);
}





void setup() {
  // put your setup code here, to run once:
  if (serialPrintStatements){
    Serial.begin(115200);
  }

  
  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
	output.setPeriodHertz(50);    // standard 50 hz servo
	output.attach(throttleOutputPin, 1000, 2000); // attaches the servo on pin 18 to the servo object
	// using default min/max of 1000us and 2000us

  output.attach(throttleOutputPin);
  // initialize EEPROM with predefined size
  EEPROM.begin(8);
  pinMode(calSwitchPin, INPUT_PULLUP);
  pinMode(commitButtonPin, INPUT_PULLUP);

  attachInterrupt(commitButtonPin, isr, FALLING);

  potentialNewThrottleUpper = read2bytes(throttleUpperEEPROMAddress);
  potentialNewThrottleLower = read2bytes(throttleLowerEEPROMAddress);
  potentialNewBrakeUpper = read2bytes(brakeUpperEEPROMAddress);
  potentialNewBrakeLower = read2bytes(brakeLowerEEPROMAddress);
  if (serialPrintStatements){
    Serial.print("potentialNewThrottleUpper as read from EEPROM: ");
    Serial.println(potentialNewThrottleUpper);
    Serial.print("potentialNewThrottleLower as read from EEPROM: ");
    Serial.println(potentialNewThrottleLower);
    Serial.print("potentialNewBrakeUpper as read from EEPROM: ");
    Serial.println(potentialNewBrakeUpper);
    Serial.print("potentialNewBrakeLower as read from EEPROM: ");
    Serial.println(potentialNewBrakeLower);
  }
  // sanity check on the loaded config
  if (bothInputsZero()){
    if (serialPrintStatements){
      Serial.println("Sanity check passed, assigning potential limits to current inputs");
    }
    failedBootSanityCheck = false;
    currentThrottleUpper = potentialNewThrottleUpper;
    currentThrottleLower = potentialNewThrottleLower;
    currentBrakeUpper = potentialNewBrakeUpper;
    currentBrakeLower = potentialNewBrakeLower;
  } else{
    if (serialPrintStatements){
      Serial.println("Sanity check failed, raising flag");
    }
    failedBootSanityCheck = true;
  }

}

void loop() {
  if(!digitalRead(calSwitchPin)){    //TODO: do I need a 'not'?
    if (serialPrintStatements){
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

      tempBrakeIn = analogRead(brakeInPin);

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

    if (serialPrintStatements){
      Serial.print("potentialNewThrottleUpper with deadzones: ");
      Serial.println(potentialNewThrottleUpper);
      Serial.print("potentialNewThrottleLower with deadzones: ");
      Serial.println(potentialNewThrottleLower);
      Serial.print("potentialNewBrakeUpper with deadzones: ");
      Serial.println(potentialNewBrakeUpper);
      Serial.print("potentialNewBrakeLower with deadzones: ");
      Serial.println(potentialNewBrakeLower);
    }

    if(((potentialNewThrottleUpper - potentialNewThrottleLower) > 200) && ((potentialNewBrakeUpper - potentialNewBrakeLower) > 200)){
      newConfigQueued = true;
      if (serialPrintStatements){
        Serial.println("new limit check passed, flagged as valid");
      }
    }
    calibrationMode = false;
  } else if(!failedBootSanityCheck){
    output_to_esc(calculate_output(analogRead(throttleInPin), analogRead(brakeInPin)));
  } else{
    // if boot sanity check failed, basically just wait until user calibrates
    delay(10);
    output_to_esc(0);
  }
}











