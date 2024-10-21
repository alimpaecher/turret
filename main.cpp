#define PASSCODE_LENGTH 4
#define CORRECT_PASSCODE "1234" // Change this to your desired passcode
#define MAX_COMMANDS 100

char passcode[PASSCODE_LENGTH + 1] = ""; // Buffer to store user input passcode
bool passcodeEntered = false; // Flag to indicate if passcode has been entered correctly

// Record mode variables
bool recordMode = false; // Flag to indicate if turret is in record mode
int recordedCommands[MAX_COMMANDS]; // Array to store recorded commands
int commandCount = 0; // Counter to keep track of number of recorded commands

//////////////////////////////////////////////////
                //  LIBRARIES  //
//////////////////////////////////////////////////
#include <Arduino.h>
#include <Servo.h>
#include "PinDefinitionsAndMore.h" // Define macros for input and output pin etc.
#include <IRremote.hpp>

#define DECODE_NEC  //defines the type of IR transmission to decode based on the remote. See IRremote library for examples on how to decode other types of remote

//defines the specific command code for each button on the remote
#define left 0x8
#define right 0x5A
#define up 0x52
#define down 0x18
#define ok 0x1C
#define cmd1 0x45
#define cmd2 0x46
#define cmd3 0x47
#define cmd4 0x44
#define cmd5 0x40
#define cmd6 0x43
#define cmd7 0x7
#define cmd8 0x15
#define cmd9 0x9
#define cmd0 0x19
#define star 0x16
#define hashtag 0xD

//////////////////////////////////////////////////
          //  PINS AND PARAMETERS  //
//////////////////////////////////////////////////
Servo yawServo; //names the servo responsible for YAW rotation, 360 spin around the base
Servo pitchServo; //names the servo responsible for PITCH rotation, up and down tilt
Servo rollServo; //names the servo responsible for ROLL rotation, spins the barrel to fire darts

int yawServoVal = 90; //initialize variables to store the current value of each servo
int pitchServoVal = 100;
int rollServoVal = 90;

int lastYawServoVal = 90; //initialize variables to store the last value of each servo
int lastPitchServoVal = 90; 
int lastRollServoVal = 90;

int pitchMoveSpeed = 8; //this variable is the angle added to the pitch servo to control how quickly the PITCH servo moves - try values between 3 and 10
int yawMoveSpeed = 90; //this variable is the speed controller for the continuous movement of the YAW servo motor. It is added or subtracted from the yawStopSpeed, so 0 would mean full speed rotation in one direction, and 180 means full rotation in the other. Try values between 10 and 90;
int yawStopSpeed = 90; //value to stop the yaw motor - keep this at 90
int rollMoveSpeed = 90; //this variable is the speed controller for the continuous movement of the ROLL servo motor. It is added or subtracted from the rollStopSpeed, so 0 would mean full speed rotation in one direction, and 180 means full rotation in the other. Keep this at 90 for best performance / highest torque from the roll motor when firing.
int rollStopSpeed = 90; //value to stop the roll motor - keep this at 90

int yawPrecision = 150; // this variable represents the time in milliseconds that the YAW motor will remain at it's set movement speed. Try values between 50 and 500 to start (500 milliseconds = 1/2 second)
int rollPrecision = 158; // this variable represents the time in milliseconds that the ROLL motor with remain at it's set movement speed. If this ROLL motor is spinning more or less than 1/6th of a rotation when firing a single dart (one call of the fire(); command) you can try adjusting this value down or up slightly, but it should remain around the stock value (270) for best results.

int pitchMax = 175; // this sets the maximum angle of the pitch servo to prevent it from crashing, it should remain below 180, and be greater than the pitchMin
int pitchMin = 10; // this sets the minimum angle of the pitch servo to prevent it from crashing, it should remain above 0, and be less than the pitchMax

int dartsFired = 0;

//////////////////////////////////////////////////
                //  S E T U P  //
//////////////////////////////////////////////////
void setup() {
    Serial.begin(9600);

    yawServo.attach(10); //attach YAW servo to pin 3
    pitchServo.attach(11); //attach PITCH servo to pin 4
    rollServo.attach(12); //attach ROLL servo to pin 5

    // Just to know which program is running on my Arduino
    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));

    // Start the receiver and if not 3. parameter specified, take LED_BUILTIN pin from the internal boards definition as default feedback LED
    IrReceiver.begin(9, ENABLE_LED_FEEDBACK);

    Serial.print(F("Ready to receive IR signals of protocols: "));
    printActiveIRProtocols(&Serial);
    Serial.println(F("at pin " STR(9)));

  homeServos();
}

////////////////////////////////////////////////
                //  L O O P  //
////////////////////////////////////////////////

void loop() {
    if (IrReceiver.decode()) { //if we have recieved a comman this loop...
        int command = IrReceiver.decodedIRData.command; //store it in a variable
        IrReceiver.resume(); // Enable receiving of the next value
        handleCommand(command); // Handle the received command through switch statements
    }
    delay(5); //delay for smoothness
}

void checkPasscode() {
    if (strcmp(passcode, CORRECT_PASSCODE) == 0) {
        // Correct passcode entered, shake head yes
        Serial.println("CORRECT PASSCODE");
        passcodeEntered = true;
        shakeHeadYes();
    } else {
        // Incorrect passcode entered, shake head no
        passcodeEntered = false;
        shakeHeadNo();
        Serial.println("INCORRECT PASSCODE");
    }
    passcode[0] = '\0'; // Reset passcode buffer
}



void addPasscodeDigit(char digit) {
    if (!passcodeEntered && digit >= '0' && digit <= '9' && strlen(passcode) < PASSCODE_LENGTH) {
        strncat(passcode, &digit, 1); //adds a digit to the passcode
        Serial.println(passcode); //print the passcode to Serial
    } else if (strlen(passcode) > PASSCODE_LENGTH+1){
      passcode[0] = '\0'; // Reset passcode buffer
      Serial.println(passcode);
    }
}

/************************\
// RECORDING FUNCTIONS //
\************************/

void startRecording() {
    recordMode = true;
    commandCount = 0;
}

void recordMovement(int command) {
    // if command is up, down, left, right, or fire, record the command
    switch ( command ) {
        case up:
        case down:
        case left:
        case right:
        case ok:
            if ( commandCount >= MAX_COMMANDS ) {
                recordMode = false;
                return;
            }
            recordedCommands[commandCount] = command;
            commandCount++;
        break;
        default:
            return;
    }
}

void playbackRecordedCommands() {
    for (int i = 0; i < commandCount; i++) {
        handleCommand(recordedCommands[i]);
        delay(5);
    }
}

void beginRecording() {
    recordMode = true;
    commandCount = 0;
}

void stopRecording() {
    recordMode = false;
}

void handleCommand(int command) {
    if((IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) && !passcodeEntered){ // this checks to see if the command is a repeat
    Serial.println("DEBOUNCING REPEATED NUMBER - IGNORING INPUT");
    return; //discarding the repeated numbers prevent you from accidentally inputting a number twice
    }

    if (recordMode) {
        recordMovement(command);
    }

    switch (command) {
        case up:
            if (passcodeEntered) {
                // Handle up command
                upMove(1);
            } else {
                //shakeHeadNo();
            }
            break;

        case down:
            if (passcodeEntered) {
                // Handle down command
                downMove(1);
            } else {
                //shakeHeadNo();
            }
            break;

        case left:
            if (passcodeEntered) {
                // Handle left command
                leftMove(1);
            } else {
                //shakeHeadNo();
            }
            break;

        case right:
            if (passcodeEntered) {
              // Handle right command
              rightMove(1);
            } else {
                //shakeHeadNo();
            }
            break;

        case ok:
            if (passcodeEntered) {
                // Handle fire command
                fire();
                Serial.println("FIRE");
            } else {
                //shakeHeadNo();
            }
            break;

        case star:
            if (passcodeEntered) {
              Serial.println("LOCKING");
                // Return to locked mode
                passcodeEntered = false;
            } else {
                //shakeHeadNo();
            }
            break;

        case cmd1: // Add digit 1 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('1');
            }
            break;

        case cmd2: // Add digit 2 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('2');
            }
            break;

        case cmd3: // Add digit 3 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('3');
            }
            break;

        case cmd4: // Add digit 4 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('4');
            }
            break;

        case cmd5: // Add digit 5 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('5');
            }
            break;

        case cmd6: // Add digit 6 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('6');
            }
            break;

        case cmd7: // Add digit 7 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('7');
            } else if (!recordMode) {
                beginRecording();
            } else {
                stopRecording();
            }
            break;

        case cmd8: // Add digit 8 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('8');
            } else if (!recordMode)
            {
                playbackRecordedCommands();
            }
            break;

        case cmd9: // Add digit 9 to passcode
            if (!passcodeEntered) {  
              addPasscodeDigit('9');
            } else {
              shakeHeadAndFire();
            }
            break;

        case cmd0: // Add digit 0 to passcode
            if (!passcodeEntered) {
                addPasscodeDigit('0');
            }
            break;               
        case hashtag:
          randomRoulette();
        break;

        default:
            // Unknown command, do nothing
            Serial.println("Command Read Failed or Unknown, Try Again");
            break;
    }
    if (strlen(passcode) == PASSCODE_LENGTH){
        checkPasscode();
    }
}



void shakeHeadYes() {
      Serial.println("YES");
    int startAngle = pitchServoVal; // Current position of the pitch servo
    int lastAngle = pitchServoVal;
    int nodAngle = startAngle + 20; // Angle for nodding motion

    for (int i = 0; i < 3; i++) { // Repeat nodding motion three times
        // Nod up
        for (int angle = startAngle; angle <= nodAngle; angle++) {
            pitchServo.write(angle);
            delay(7); // Adjust delay for smoother motion
        }
        delay(50); // Pause at nodding position
        // Nod down
        for (int angle = nodAngle; angle >= startAngle; angle--) {
            pitchServo.write(angle);
            delay(7); // Adjust delay for smoother motion
        }
        delay(50); // Pause at starting position
    }
}

void shakeHeadNo() {
    Serial.println("NO");
    int startAngle = pitchServoVal; // Current position of the pitch servo
    int lastAngle = pitchServoVal;
    int nodAngle = startAngle + 60; // Angle for nodding motion

    for (int i = 0; i < 3; i++) { // Repeat nodding motion three times
        // rotate right, stop, then rotate left, stop
        yawServo.write(140);
        delay(190); // Adjust delay for smoother motion
        yawServo.write(yawStopSpeed);
        delay(50);
        yawServo.write(40);
        delay(190); // Adjust delay for smoother motion
        yawServo.write(yawStopSpeed);
        delay(50); // Pause at starting position
    }
}

void shakeHeadAndFire() {
    Serial.println("NO");
    int startAngle = pitchServoVal; // Current position of the pitch servo
    int lastAngle = pitchServoVal;
    int nodAngle = startAngle + 60; // Angle for nodding motion
    int delayTime = 500;

    // right fire
    rightMove(2)
    fire();
    delay(delayTime);

    // up fire
    upMove(2)
    fire();
    delay(500);

    // left fire
    leftMove(2)
    fire();
    delay(delayTime);

    // down fire
    downMove(2)
    fire();
    delay(delayTime);
}

void leftMove(int moves){
    for (int i = 0; i < moves; i++){
        yawServo.write(yawStopSpeed + yawMoveSpeed); // adding the servo speed = 180 (full counterclockwise rotation speed)
        delay(yawPrecision); // stay rotating for a certain number of milliseconds
        yawServo.write(yawStopSpeed); // stop rotating
        delay(5); //delay for smoothness
        Serial.println("LEFT");
  }

}

void rightMove(int moves){ // function to move right
  for (int i = 0; i < moves; i++){
      yawServo.write(yawStopSpeed - yawMoveSpeed); //subtracting the servo speed = 0 (full clockwise rotation speed)
      delay(yawPrecision);
      yawServo.write(yawStopSpeed);
      delay(5);
      Serial.println("RIGHT");
  }
}

void upMove(int moves){
  for (int i = 0; i < moves; i++){
      if(pitchServoVal > pitchMin){//make sure the servo is within rotation limits (greater than 10 degrees by default)
        pitchServoVal = pitchServoVal - pitchMoveSpeed; //decrement the current angle and update
        pitchServo.write(pitchServoVal);
        delay(50);
        Serial.println("UP");
      }
  }
}

void downMove (int moves){
  for (int i = 0; i < moves; i++){
        if(pitchServoVal < pitchMax){ //make sure the servo is within rotation limits (less than 175 degrees by default)
        pitchServoVal = pitchServoVal + pitchMoveSpeed;//increment the current angle and update
        pitchServo.write(pitchServoVal);
        delay(50);
        Serial.println("DOWN");
      }
  }
}


void homeServos(){
    yawServo.write(yawStopSpeed); //setup YAW servo to be STOPPED (90)
    delay(20);
    rollServo.write(rollStopSpeed); //setup ROLL servo to be STOPPED (90)
    delay(100);
    pitchServo.write(100); //set PITCH servo to 100 degree position
    delay(100);
    pitchServoVal = 100; // store the pitch servo value
}

void fire() { //function for firing a single dart
    rollServo.write(rollStopSpeed + rollMoveSpeed);//start rotating the servo
    delay(rollPrecision);//time for approximately 60 degrees of rotation
    rollServo.write(rollStopSpeed);//stop rotating the servo
    delay(5); //delay for smoothness
}

void fireAll() { //function to fire all 6 darts at once
    rollServo.write(rollStopSpeed + rollMoveSpeed);//start rotating the servo
    delay(rollPrecision * 6); //time for 360 degrees of rotation
    rollServo.write(rollStopSpeed);//stop rotating the servo
    delay(5); // delay for smoothness
}

void randomRoulette() {
  Serial.println("ENTERING ROULETTE MODE");
  //unsigned long startTime = millis();
  dartsFired = 0;
  while(dartsFired < 6){ //while we still have darts, stay within this while loop
    if (dartsFired < 6){ // if we still have darts do stuff (this is redundancy to help break out of the while loop in case something weird happens)
      pitchServoVal = 110;
      pitchServo.write(pitchServoVal); // set PITCH servo to flat angle
      yawServo.write(145); // set YAW to rotate slowly
      delay(400); // rotating for a moment
      yawServo.write(90); // stop
      delay(400 * random(1,4)); //wait for a random delay each time

      if(random(3) == 0){ // you have a 1 in six chance of being hit
        delay(700); 
        if(random(2) == 0){ // 50% chance to either shake the head yes before firing or just fire
          shakeHeadYes();
          delay(150);
          fire(); // fire 1 dart
          delay(100);
        // } else if(random(6) == 0){
        //   shakeHeadYes();
        //   delay(50);
        //   fireAll(); // fire all the darts
        //   delay(50);
        } else {
          fire(); // fire 1 dart
          delay(50);
        }
      }else{
        if(random(6) == 5){
          delay(700);
          shakeHeadNo();
          delay(300);
        } else{
          delay(700);
        }
      }
    } else{
      yawServo.write(90); // redundancy to stop the spinning and break out of the while loop
      return;
    }
  }
  yawServo.write(90); // finally, stop the yaw movement
}