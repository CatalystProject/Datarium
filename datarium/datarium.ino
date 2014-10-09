
#include <aJSON.h>
#include <Bridge.h>
#include <Process.h>
#include <HttpClient.h>
#include <Servo.h> 
#include "shrub.h"

/*
This code is intended to drive the hardware of the Datarium - 
created as part of the Catalyst project. The code is intended
to a web server, download its behaviour, position, and colour 
in the form of a JSON message, and reflect these changes. In 
addition, an LDR can be connected to act as a soft button - when
triggered, the Datarium would connect to the server and obtain data
forecast data.

The Datarium hardware was implemented using an Arduino Yun, which
allows the programmer to set up a HTTP client and connect to a 
server. The BASE_URL will have to be changed to point to the 
arduino_yun_api.php file located on the web server of your 
choosing.

Author - Catalyst Project (Peter Newman)
*/

//timer - poll evert 10 minutes
#define MSG_PERIOD 600000
//#define MSG_PERIOD 5000
#define MAX_MOVE_PROFILE 0
#define SENSOR_THRESHOLD 250
#define OUTPUT_ENABLED false
#define DEMO_MODE false

//[CHANGE]
#define BASE_URL ""
#define CURRENT_URL "get_current_hour"
#define PREDICTION_URL "get_24hour_prediction"
#define TWITCH 1
#define NORMAL 0
#define MAX_MOVEMENT 60
#define SERVO_PIN 3

HttpClient client;
Servo treeActuator;          //create servo object to actuate 
//movement function pointers
int msgNumber = 0;
int ldrPin = A0;             //ldr pin
//used for mapping LDR input
int sensorMin = 1023;        // minimum sensor value
int sensorMax = 0;           // maximum sensor value
int sensorValue = 0;         // the sensor value

int oldRed;
int oldGreen;  
int lastActivation = 0;
  
//Utility function used to check free RAM in arduino - not related
//to Datarium, but used for fixing memory leak
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
  
//This function is called when the Datarium determines a movement/
//colour change should occur. Included are the "movement profile",
// duty cycle percentage of both red and green.
void invokeBehaviour( int moveProfile, float red, float green ){
  if ( red > 100 )
    red = 100;
    
  if ( green > 100 )
    green = 100;
  
  if ( OUTPUT_ENABLED ){
    Serial.print("MSG : ");
    Serial.println(msgNumber);
  }
  
  //if move profile is twitch, punctuate display with movement
  if ( moveProfile == TWITCH ){
    //do twitch
    for ( int i = 0; i < 5; i++){
      treeActuator.write(90); 
      delay(200);  
      treeActuator.write(150);
      delay(200); 
    }      
  }
  
  //determine position of servo based on colour and centre position
  //of servo (which was about 100 on the servo used)
  int centre = 100;
  int movement = red - green;
  if (movement > MAX_MOVEMENT )
    movement = MAX_MOVEMENT;
  else if (movement < 0 && abs(movement) > MAX_MOVEMENT)
    movement = -MAX_MOVEMENT;
    
  if ( OUTPUT_ENABLED ){
    Serial.print("Mv : ");
    Serial.println(movement);
  }
  
  //move servo to new position
  treeActuator.write(centre + movement);
    
  //now effect changes to lights etc
  float redAdjusted = red / 100 * 255;
  float greenAdjusted = green / 100 * 255;

  if ( OUTPUT_ENABLED ){
    Serial.print("Red : ");
    Serial.println(redAdjusted);
    Serial.print("Green : ");
    Serial.println(greenAdjusted);
  }
  
  //write light colours to bi-colour LEDs
  analogWrite( 10, greenAdjusted);
  analogWrite( 11, redAdjusted);
  
  //If teitch profile, show lights for 5 seconds
  if ( moveProfile == TWITCH ){
    //if demo mode enabled, only pause for 1 second, else 5
    if (DEMO_MODE)
      delay(1000);
    else
      delay(5000); 
    
    //twitch Datarium again
    for ( int i = 0; i < 5; i++){
      treeActuator.write(90); 
      delay(200);  
      treeActuator.write(150);
      delay(200);    
    }    
    
    //reset to old values
    invokeBehaviour(NORMAL, oldRed, oldGreen);
  }
  else{
    //remember old colours
    oldRed = red;
    oldGreen = green; 
  }  
}

//This function is called by the LDR being triggered.
void processAction(){
  //if in Demo mode - there is no internet and thus just twitch
  if ( DEMO_MODE ){
    invokeBehaviour( TWITCH, 100, 100 );    
    return; 
  }
  
  //invoke service for information about prediction
  Message msg = getServiceResponse(PREDICTION_URL);
  
  if ( OUTPUT_ENABLED ){
    Serial.println(msg.len);
  }
  
  //if we received a message...
  if ( msg.len > 0 ){    
       //place string in char[] (for later processing)
      char buf[msg.len];
      if (msg.msg != "")   
        msg.msg.toCharArray( buf, msg.len );
       
      //parse message here and return as Action
      Action a = getActionRequired( buf );
      
      //actuate behaviour and supply action arguments
      invokeBehaviour( a.moveProfile, a.red, a.green );
  }
  
  //reset message
  msg.msg = "";
}

//This function parses the JSON message into an Action object
Action getActionRequired( char *buf ){
  aJsonObject* root;  
  //get root node of JSON
  root = aJson.parse(buf);
  
  //if no root, there is no valid action - error
  if ( !root ){   
    Action a = {0, 0, 0};
    return a; 
  }
  
  //get move profile
  aJsonObject* subObject = aJson.getObjectItem(root, "move_profile");
  //again, if not object, something is wrong, clean up and go
  if ( !subObject ){        
    Action a = {0, 0, 0};
    aJson.deleteItem(root);
    return a; 
  }
  
  int moveProfile = subObject->valueint;
  int green = 0;
  int red = 0;     
  
  //get green light intensity
  subObject = aJson.getObjectItem(root, "green");
  if ( !subObject ){   
    Action a = {0, 0, 0}; 
    aJson.deleteItem(root);    
    return a; 
  }
  
  green = subObject->valueint;
  
  //get red light intensity
  subObject = aJson.getObjectItem(root, "red");
  if ( !subObject ){   
    Action a = {0, 0, 0}; 
    aJson.deleteItem(root);    
    return a; 
  }
  
  red = subObject->valueint;      
  
  Action a = {moveProfile, red, green};  
  //clear root of json - leaks memory otherwise    
  aJson.deleteItem(root);
  return a;
}

//This function performs a service call to the Datarium server
Message getServiceResponse(String serviceCall){
  String msg = "";
  int len = 1;   
  //try to connect to server API
  client.get( BASE_URL + "arduino_yun_api.php?func=" + serviceCall);
  
  //wait a short while for bytes to arrive
  delay(200);
  //while there are bytes coming from the client...
  while (client.available()){
    //read each byte, and add to message
    char c = client.read();
    msg+=c;
    len++;
  }    
  
  if (OUTPUT_ENABLED){
    Serial.flush(); 
    Serial.println("["+msg+"]");
  }
  
  //return message contents and its length (not buffer length)
  Message r = {msg, len};
  return r;
}

void setup() {
  //configure LED13 to bink when message is received...
  pinMode( 13, OUTPUT);  
  digitalWrite(13, LOW);
    
  //first, let the ldr check the take an average of the light it receives 
  analogRead(ldrPin);

  //pins for the energy detector - another project used to move display in Demo Mode. Provides test inpiut over two channels
  pinMode( A2, INPUT);
  pinMode( A5, INPUT); 

  //colour LED channels
  pinMode( 11, OUTPUT );
  pinMode( 10, OUTPUT );

  //setup servo
  treeActuator.attach(SERVO_PIN);
   
  if (OUTPUT_ENABLED){
    //start Serial library...
    Serial.begin(9600);
    while (!Serial);
    
    Serial.println("[i] Serial initialised.");
    
  }
  
  //begin bridge and alert console.
  if (!DEMO_MODE){
    //begin the Arduino Yun Bridge (talk to Linux box on Arduino Yun)
    Bridge.begin();
    if (OUTPUT_ENABLED)
      Serial.println("[i] Bridge interface initialised."); 
    
    //status light on
    digitalWrite(13, HIGH);
  }
  else{
    
    //change analog ref to capture small voltages of Energy Detector
    analogReference(INTERNAL);
    Serial.println("[db] Debug enabled."); 
  }
}

bool firstTime = true;
void loop() { 
  String err = "";  
  
  //remember previous activation of Datarium
  if ( lastActivation > millis() )
    lastActivation = millis();
  
  if (!DEMO_MODE){
    int delta = (millis() - lastActivation);
    //is this the first time or has enough time elapsed? If so, get next message from server
    if ( delta > MSG_PERIOD || firstTime ){
      Message msg = getServiceResponse(CURRENT_URL);
      //check if message if empty - if so, do nothing, just try again in 5 seconds...
      if ( msg.len > 0 ){    
         //place string in char[] (for later processing)
        char buf[msg.len];
        if (msg.msg != "")   
          msg.msg.toCharArray( buf, msg.len );
         
        //Parse message and get Action
        Action a = getActionRequired( buf );
        //actuate behaviour
        invokeBehaviour( a.moveProfile, a.red, a.green );
      }
      lastActivation = millis();
      //cleanup
      msg.msg = "";
      msg.len = 0;
      firstTime = false;
    }
  }
  else{
    //instead of getting behaviour from server - check analog pins (Energy Detector)    
    int windVal = analogRead(A5);
    //multiplier
    windVal *= 8;
    windVal = map(windVal, 0, 1023, 0, 100);
    
    //multiplier
    int lightVal = analogRead(A2);    
    lightVal *= 1.5;
    
    lightVal = map(lightVal, 0, 1023, 0, 100);
    //map values onto 0 - 100
  
    invokeBehaviour( NORMAL, lightVal, windVal );
  }
  
  //check for input - no need to map value
  sensorValue = analogRead(ldrPin);  
  
  if ( OUTPUT_ENABLED){
    Serial.print("Sensor");
    Serial.println(sensorValue);
  //sensorValue = map(sensorValue, sensorMin, sensorMax, 0, 255);
  }
  //figure out what the threshold is - probably anything below 100
  if ( sensorValue < SENSOR_THRESHOLD ){
    //process action... 
    processAction();
  } 
  
  delay( 100 );
}
