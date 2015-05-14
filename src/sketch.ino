#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 3

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

Servo valveServo;
Servo topServo;
Servo bottomServo;

int valveSpeed = 90;    // variable to store the servo position
int ironBottomSpeed = 90;    // variable to store the servo position
int ironTopSpeed = 90;    // variable to store the servo position

boolean nextTimeSet = false;
unsigned long valveStopTime = 0;
unsigned long ironBottomStopTime = 0;
unsigned long ironTopStopTime = 0;
unsigned long timeNextState = 0;
unsigned long pourTime = 16;

unsigned long baseCookTime = 20000l;
double baseTemperature = 162;
long timePerDegree = 1000l;

boolean maintMode = false;

double temp = 0;

unsigned int state = 0;

boolean isValveOpen = false;
boolean isIronUp = true;
boolean isTopClosed = true;

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  // Start up the library
  sensors.begin(); // IC Default 9 bit. If you have troubles consider upping it 12. Ups the delay giving the IC more time to process the temperature measurement

  valveServo.attach(9);  // attaches the servo on pin 9 to the servo object
  topServo.attach(6);
  bottomServo.attach(5);

}

void OpenValve(){
  if(isValveOpen && !maintMode) return;
  Serial.println("Opening valve");
  valveStopTime = millis()+600;
  valveSpeed = 140;
  isValveOpen = true;
}
void CloseValve(){
  if(!isValveOpen && !maintMode) return;
  Serial.println("Closing valve");
  valveStopTime = millis()+600;
  valveSpeed = 40;
  isValveOpen = false;
}


void CloseTop(){
  if(isTopClosed && !maintMode) return;
  Serial.println("Closing top");
  ironTopStopTime = millis()+2600;
  ironTopSpeed = 130;
  isTopClosed = true;
}
void OpenTop(){
  if(!isTopClosed && !maintMode) return;
  Serial.println("Opening top");
  ironTopStopTime = millis()+2023;
  ironTopSpeed = 40;
  isTopClosed = false;
}

void FlipDown(){
  if(!isIronUp && !maintMode) return;
  Serial.println("Flipping top down");
  ironBottomStopTime = millis()+500;
  ironBottomSpeed = 115;
  isIronUp = false;
}
void FlipUp(){
  if(isIronUp && !maintMode) return;
  Serial.println("Flipping top up");
  ironBottomStopTime = millis()+800;
  ironBottomSpeed = 70;
  isIronUp = true;
}

String commandbuffer;

unsigned long lastTempReq = 0;

void loop()
{
  if(millis()-lastTempReq > 2000 && !AnyMotorsMoving())
  {
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
    lastTempReq = millis();
  }

  bool cmdReady = false;
  while( Serial.available() ) {
    char inp = Serial.read();
    Serial.print(inp);
    if(inp == 13){
      cmdReady = true;
      Serial.println(commandbuffer);
      break;
    }else
    {
      commandbuffer += inp;
    }
  }

  if(cmdReady){
    String strCmd = commandbuffer;
    strCmd.trim();

    if(strCmd == "temp")
    {
      Serial.print("Current temperature: ");
      Serial.println(temp);
    }
    else if(strCmd == "openv" || strCmd == "ov"){
      OpenValve();
    }
    else if(strCmd == "closev" || strCmd == "cv"){
      CloseValve();
    }
    else if(strCmd == "togglev" || strCmd == "tv"){
      if(isValveOpen) CloseValve();
      else OpenValve();
    }
    else if(strCmd == "opent" || strCmd == "ot"){
      OpenTop();
    }
    else if(strCmd == "closet" || strCmd == "ct"){
      CloseTop();
    }
    else if(strCmd == "togglet" || strCmd == "tt"){
      if(isTopClosed) OpenTop();
      else CloseTop();
    }else if(strCmd == "upb" || strCmd == "ub"){
      FlipUp();
    }else if(strCmd == "maint"){
      maintMode = !maintMode;
      Serial.println("Maint mode: "+maintMode);
    }
    else if(strCmd == "downb" || strCmd == "db"){
      FlipDown();
    }
    else if(strCmd == "toggleb" || strCmd == "tb"){
      if(isIronUp) FlipDown();
      else FlipUp();
    }
    else if(strCmd == "makeWaffle"){
      state = 1;
    }else if(strCmd == "abort"){
      state = 0;
      nextTimeSet = false;
      timeNextState = 0;
      CloseValve();
      Serial.println("Aborted to state: "+state);
    }else if(strCmd == "forceclosetop"){
      isTopClosed = false;
      CloseTop();
    }else if(strCmd == "stop")
    {
      valveStopTime = 0;
      ironBottomStopTime = 0;
      ironTopStopTime = 0;
      Serial.println("Stopped!");
    }
    else{
      Serial.println("I do not recognize: "+strCmd);
    }
    commandbuffer = "";
  }

  ProcessMotorSpeeds();
  makeWaffleLoop();
}

void ProcessMotorSpeeds(){
  //Valve speed.
  if(millis() >= valveStopTime)
    valveServo.write(95);
  else
    valveServo.write(valveSpeed);

  //Top speed.
  if(millis() >= ironTopStopTime)
    topServo.write(95);
  else
    topServo.write(ironTopSpeed);

  //Bottom speed.
  if(millis() >= ironBottomStopTime)
    bottomServo.write(95);
  else
    bottomServo.write(ironBottomSpeed);
}

bool AnyMotorsMoving()
{
  unsigned long now = millis();
  return now >= valveStopTime && now >= ironTopStopTime && now >= ironBottomStopTime;
}

//stage
//1 = starting
// - TOP UP
// - VALVE CLOSED
// - BOTTOM UP
//2 = pouring
// - TOP UP
// - VALVE OPEN
// - BOTTOM UP
// - Set next state time to 2 sconds further
//3 = wait for drip to stop
// - VALVE CLOSE
// - TOP UP
// - BOTTOM UP
//- Set next time to 3 second
//4 = Close
// - TOP DOWN
// - VALVE CLOSE
// - BOTTOM UP
// Wait until ironTopStopTime is less than milis
//5 = Cook
// - TOP DOWN
// - VALVE CLOSE
// - BOTTOM UP
// Set time to 90,000 ms further
//6 = Finish
// - TOP UP
// - VALVE CLOSE
// - BOTTOM UP
// Wait until ironTopStopTime is less than milis
void makeWaffleLoop(){
  switch(state){
    case 1:
      OpenTop();
      CloseValve();
      if(!nextTimeSet){
        nextTimeSet = true;
        timeNextState = ironTopStopTime;
        Serial.println(((String)"[WaffleProgram] Starting up, next state in ")+((timeNextState-millis())/1000)+((String)" seconds."));
      }
      if(millis() > timeNextState){
        state++;
        nextTimeSet = false;
      }
      break;
    case 2:
      OpenTop();
      OpenValve();
      if(!nextTimeSet){
        nextTimeSet = true;
        timeNextState = millis()+pourTime;
        Serial.println(((String)"[WaffleProgram] Pouring, next state in ")+((timeNextState-millis())/1000)+((String)" seconds."));
      }
      if(millis() > timeNextState){
        state++;
        nextTimeSet = false;
      }
      break;
    case 3:
      OpenTop();
      CloseValve();
      if(!nextTimeSet){
        nextTimeSet = true;
        timeNextState = millis()+3000;
        Serial.println(((String)"[WaffleProgram] Finishing pour, next state in ")+((timeNextState-millis())/1000)+((String)" seconds."));
      }
      if(millis() > timeNextState){
        state++;
        nextTimeSet = false;

      }
      break;
    case 4:
      CloseValve();
      CloseTop();
      if(millis() > ironTopStopTime){
        state++;
        nextTimeSet = false;
      }
      break;
    case 5:
      CloseTop();
      CloseValve();
      if(!nextTimeSet){
        nextTimeSet = true;
        timeNextState = millis()+(baseCookTime+((temp-baseTemperature)*timePerDegree));
        Serial.println(((String)"[WaffleProgram] Cooking, next state in ")+((timeNextState-millis())/1000)+((String)" seconds."));
      }
      //if(((timeNextState-millis())/1000 % 10) == 0){
      //    Serial.println(((String)"[WaffleProgram] Starting up, next state in ")+(timeNextState-millis())/1000+((String)" seconds."));
      //}
      if(millis() > timeNextState){
        state++;
        nextTimeSet = false;
      }
      break;
    case 6:
      OpenTop();
      CloseValve();
      if(millis() > ironTopStopTime){
        state++;
        nextTimeSet = false;
      }
      break;
    case 7:
      OpenTop();
      CloseValve();
      FlipDown();
      if(!nextTimeSet){
        nextTimeSet = true;
        timeNextState = millis()+8000;
        Serial.println(((String)"[WaffleProgram] Flipping down, next state in ")+((timeNextState-millis())/1000)+((String)" seconds."));
      }
      if(millis() > timeNextState){
        state++;
        nextTimeSet = false;
      }
      break;
    case 8:
      OpenTop();
      CloseValve();
      FlipUp();
      if(!nextTimeSet){
        nextTimeSet = true;
        timeNextState = ironBottomStopTime;
        Serial.println(((String)"[WaffleProgram] Flipping up, next state in ")+((timeNextState-millis())/1000)+((String)" seconds."));
      }
      if(millis() > timeNextState){
        state++;
        nextTimeSet = false;
      }
      break;
    case 9:
      //CloseTop();
      CloseValve();
      FlipUp();
      if(!nextTimeSet){
        nextTimeSet = true;
        timeNextState = ironTopStopTime;
        Serial.println(((String)"[WaffleProgram] Closing top, next state in ")+((timeNextState-millis())/1000)+((String)" seconds."));
      }
      if(millis() > timeNextState){
        state = 0;
        nextTimeSet = false;
      }
      break;
    default:
      break;
  }
}
