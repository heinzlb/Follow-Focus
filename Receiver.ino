////////////////////////
//Moteino FF Receiver///
////////////////////////

//      Adi Soffer  2013       //
//    for more info visit      //
// http://adisoffer.tumblr.com //
#include <SPI.h>
#include <RFM69.h>
#include <AccelStepper.h>
#include <avr/sleep.h>

#define FREQUENCY   RF69_868MHZ
#define IS_RFM69HW   

//encoder/motor/driver setup
int easyDriverMicroSteps = 1; 
int rotaryEncoderSteps = 75; 
int motorStepsPerRev = 200; 
int MinPulseWidth = 50; //too low and the motor will stall, too high and it will slow it down

int easyDriverStepPin = 15;
int easyDriverDirPin = 16;
int enablePin = 17;

volatile long encoderValue = 0;
long lastencoderValue = 0;

//Values for focus points
int inPoint;
int outPoint;

//value for variable speed
int speedValue = 1000;

//Values for Lens Calibration
int lowEndMark = -50000;
int highEndMark = 50000;

boolean lowEndSwitch = false;
boolean highEndSwitch = false;


//ON LED
#define onLed 12

int mode;
#define Rewind 1
#define Play 2
// standBy mode is meant to enable changes in speed
#define standBy 3
#define realTime 4
#define Stop 5
#define LENSCALIB 6

AccelStepper stepper(1, easyDriverStepPin, easyDriverDirPin);

//Sleep Function - to disable ED when not active
long previousMillis = 0;
int sleepTimer = 5000;

// You will need to initialize the radio by telling it what ID it has and what network it's on
// The NodeID takes values from 1-127, 0 is reserved for sending broadcast messages (send to all nodes)
// The Network ID takes values from 0-255
// By default the SPI-SS line used is D10 on Atmega328. You can change it by calling .SetCS(pin) where pin can be {8,9,10}
#define NODEID        1  //network ID used for this unit
#define NETWORKID     99  //the network ID we are on
#define GATEWAYID     2  //the node ID we're sending to
#define SERIAL_BAUD 115200

//encryption is OPTIONAL
//to enable encryption you will need to:
// - provide a 16-byte encryption KEY (same on all nodes that talk encrypted)
// - to call .Encrypt(KEY) to start encrypting
// - to stop encrypting call .Encrypt(NULL)
uint8_t KEY[] = "ABCDABCDABCDABCD";

//int for incoming radio data
int a;
int sum;

//variables for outgoing radio data
char input = 0;
int data = 0;

// Need an instance of the Radio Module
RFM69 radio;
byte sendSize=0;
char payload[] = "1234567890:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ";
bool requestACK=false;


void setup()
{
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
        //comment this out to disable encryption

  stepper.setMinPulseWidth(MinPulseWidth); 
  stepper.setMaxSpeed(speedValue);             //variable to later determine speed play/rewind
  stepper.setAcceleration(100000); 
  stepper.setSpeed(50000); 

  pinMode(enablePin, OUTPUT);
}

void loop()
{
  stepper.run();

  if (radio.receiveDone())
  {    
    lastencoderValue = encoderValue;
    digitalWrite (enablePin, LOW);
    previousMillis = millis();

    sum = 0;
    for (byte i = 0; i < radio.DATALEN; i++) 
    {
      a = (radio.DATA[i]);
    }
    sum+=a;
    dataSort ();
    stepperMove();

  }
  else
  {
    //Stepper sleep after 5sec of no data
    unsigned long currentMillis = millis ();
    if (currentMillis - previousMillis>sleepTimer)
      digitalWrite (enablePin, HIGH);
  }

  switch (mode)
  {

  case standBy:
    break;

  case Stop:
    break;

  case realTime:

    dataSort();
    stepperMove();

    break;

  case Rewind:
    //take care of variable speed
    stepper.setMaxSpeed(speedValue);
    stepper.moveTo(inPoint);
    stepper.run();
    digitalWrite (enablePin, LOW);
    if(stepper.currentPosition()==inPoint)
    {
      Transmit (3);
      mode=standBy; 
    }

    break; 

  case LENSCALIB:
    highEndMark = 50000;
    lowEndMark = -50000;
    lowEndSwitch = false;
    highEndSwitch = false;
    mode = Stop;
    break;

  case Play:
    //take care of variable speed
    stepper.setMaxSpeed(speedValue);
    stepper.moveTo(outPoint);
    stepper.run();
    digitalWrite (enablePin, LOW);
    if(stepper.currentPosition()==outPoint)
    {
      Transmit (3);
      mode=standBy;
    }
    break;
  }
}

void dataSort()
{

  if (sum == 54)
  {
    inPoint = stepper.currentPosition(); 
    Transmit (1);
  }
  else if (sum == 55)
  {
    outPoint = stepper.currentPosition();
    Transmit (2);
  }
  else if (sum == 49)
  {
    encoderValue++;
  }
  else if (sum == 50)
  {
    encoderValue--;
  }
  else if (sum == 56)
  {
    lowEndMark = stepper.currentPosition();
    lowEndSwitch = true;
    Transmit (1);
  }
  else if (sum == 57)
  {
    highEndMark = stepper.currentPosition();
    highEndSwitch = true; 
    Transmit (2);
  }
  else if (sum == 52)
  {
    mode = LENSCALIB; 
  }

  else if (sum == 51)
  {
    mode = Stop;
  }
  else if (sum == 53)
  {
    //function to make Play or Rewind
    defineDirection();
  }
  else if (sum == 58)
  {
    speedValue -=30;
    if (speedValue <100)
      speedValue = 100;
  }
  else if (sum = 59)
  {
    speedValue+=30;
    if (speedValue>5500)
      speedValue = 6000;
  }
}

void defineDirection()
{
  if (stepper.currentPosition()!=inPoint)
  { 
    mode = Rewind; 
  }
  else if (stepper.currentPosition() == inPoint)
  {
    mode = Play;
  }
}

void stepperMove ()
{
  int stepsPerRotaryStep = (motorStepsPerRev * easyDriverMicroSteps) / rotaryEncoderSteps;
  int motorMove = (encoderValue * stepsPerRotaryStep);
  if (mode==standBy)return; 
  //Lens Calibration
  if (motorMove >lowEndMark)
    motorMove = lowEndMark; 
  if (motorMove<highEndMark)
    motorMove = highEndMark;

  stepper.run();
  stepper.setMaxSpeed(1000);
  stepper.moveTo(motorMove);
}

int Transmit (int data)
{
  
  radio.send(GATEWAYID, payload, sendSize+(data));
  radio.sleep();
}
