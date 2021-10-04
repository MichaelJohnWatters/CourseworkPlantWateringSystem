/*
  Michael John Watters
  B00751280

  This Code Runs on the Node MCU and performs the majority of the functionality.

  This includes triggering relays to pump water.
  Hosting a web server, which you can use to changes variables in the code.
  Managing sensor reads and deciding on action to take based of those input reads.
  Ouputs Strings over IC2 to the Nano board for processing and dsiplay to the LCD.
  
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                                       
#include <math.h>

// NODE MCU libs and webserver libs.
#include <Adafruit_ADS1X15.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// Capacitive soil moisture ADC Config
#define SENSOR_0_ADC 0
#define SENSOR_1_ADC 1
#define SENSOR_2_ADC 2 
#define SENSOR_3_ADC 3

// GPIO Relay Pins Config
#define RELAY_0_PIN 14
#define RELAY_1_PIN 12
#define RELAY_2_PIN 13
#define RELAY_3_PIN 15

// Static Config
#define INTERVAL 60 // Seconds
#define LOW_WATER_THRESHOLD 20.00 // Percentage

// Webserver Config
#define MAX_ML 50.00 // millilitres
#define MIN_ML 10.00 // millilitres
#define PORT 80

// Wifi Config
#ifndef WIFI_SSID
#define WIFI_SSID "PLEASE SET ME"
#define WIFI_PASSWORD "PLEASE SET ME"
#endif

// **************
// * ADC Object *
// **************
Adafruit_ADS1115 ads;

// **********************
// * Http Server Object *
// **********************
ESP8266WebServer server(PORT);

// **************
// * Wifi Config *
// **************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Serial Communication Receiver Board Terminator char and
// Serial Communication Receiver Board Newline indicator char.
const String terminator = String("^"); 
const String newLine = String("|");

// Sensor Config Dry voltage thresholds, used to map to percentage 0-100%
const int sensorConfigWet[] = {
    6992 + 160, //Sensor 0 + off set caused by circuit, possibly wire resistances, kept here to be clear of the reason.
    7104 + 160, //Sensor 1
    6784 + 160, //Sensor 2
    6928 + 160  //Sensor 3
};

// Sensor Config Wet voltage thresholds, used to map to percentage 0-100%
const int sensorConfigDry[] = {
    15088 - 1295, //Sensor 0 - 1295 offset caused by circuit, possibly wire resistances, kept here to be clear of the reason.
    15088 - 1295, //Sensor 1
    15088 - 1525, //Sensor 2 
    14768 - 1051  //Sensor 3 
};

// Amount to water in millilitres, Can be mutated by post requests.
double nodeWaterAmounts[] = {
  20.00, //millilitres
  30.00,
  40.00,
  50.00
};

// Epoch time of last water event, default to 0.
long epochLastWatered[] = {
  0,
  0,
  0,
  0
};


// ******************
// * Setup Function *
// ******************
// - In this function the ads is setup.
// - Wifi is setup.
// - Relay's are setup.
void setup() {

  // Set the Serial Rate.
  Serial.begin(9600);

  //----------------------------------------------------------------

  // ********************************
  // * START - ADS1115 Setup *
  // ********************************

  // Set ads gain.
  ads.setGain(GAIN_TWOTHIRDS); // 1 bit = 3 mV
  
  // Display to the LCD if the Analog to Digital Converter, fails to initialize
  if (!ads.begin()) {
    writeToLcd("Failed to |initialize ADC", 1);
    
    // Infinite Loop to alert issue to the user.
    while (1);
  }

  // ******************************
  // * END - ADS1115 Setup *
  // ******************************

  //----------------------------------------------------------------

  // ***********************
  // * START - Relay Setup *
  // ***********************

  // Set relay pins to OUTPUT.
  // No Need for a loop...
  pinMode(RELAY_0_PIN, OUTPUT);
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  pinMode(RELAY_3_PIN, OUTPUT);

  // Set all Relay pins to OFF (HIGH).
  // No Need for a loop...
  digitalWrite(RELAY_0_PIN, HIGH);
  digitalWrite(RELAY_1_PIN, HIGH);
  digitalWrite(RELAY_2_PIN, HIGH);
  digitalWrite(RELAY_3_PIN, HIGH);

  // *********************
  // * END - Relay Setup *
  // *********************

  //----------------------------------------------------------------

  // **********************************
  // * START Http Server + WIFI Setup *
  // **********************************

  // Begin Wifi and write to diplay while connecting...
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    writeToLcd("Waiting for|Wifi Connection", 0);
    delay(250);
  }

  // Build SSID String and display on LCD...
  String connectedSsid = "Connected to...|" + String(ssid);
  int connectedSsidLength = connectedSsid.length() + 1;
  char displayConnSsid[connectedSsidLength];
  connectedSsid.toCharArray(displayConnSsid, connectedSsidLength);
  writeToLcd(displayConnSsid, 2);
  
  // Build IP Address String and display on LCD...
  String ipAddress = "MCU IP address:|" + WiFi.localIP().toString();
  int ipAddressLength = ipAddress.length() + 1;
  char displayIpAdd[ipAddressLength];
  ipAddress.toCharArray(displayIpAdd, ipAddressLength);
  writeToLcd(displayIpAdd, 2);

  
  // Webserver
  // Establish Roots and root actions.
  server.on("/", handleRoot); // Handle requests on root
  server.on("/update/", handleUpdate); // Handle Updates
  server.onNotFound(error404);
  server.begin();
  
  // ********************************
  // * END Http Server + WIFI Setup *
  // ********************************
}


bool firstLoop = true;
void loop() {

  // Handle Http Requests.
  server.handleClient();
  
  // Wait for both boards to be ready..
  // Only needed at first loop run.
  if(firstLoop == true){
    wait(2); // seconds
    firstLoop = false;
  }
  
  //Prompt Beginning reads...
  writeToLcd("Beginning|Sensor Reads...", 3);

  // Loop through each 'Node' (Sensor/Pump/Plant) 0, 1, 2, 3
  for (int node_id = 0; node_id < 4; node_id++) {
    
    //If not the first node, write to lcd that the programme is moving to the next node.
    if(node_id != 0){
       writeToLcd("Reading next|sensor...", 3);
    }
    
    // Read and Convert Soil Moisture Sensor value.
    double percent_reading = readSensorMoisture(node_id);

    // Floor the percent_reading if it goes above 0% or 100%
    if (percent_reading > 100.00){
      percent_reading = 100.00;
    } else if(percent_reading < 0.00) {
      percent_reading = 0.00;
    }

    // Prepare Output of 'percent_reading' and write to LCD.
    String displayString = String("Read Sensor: ") + String(node_id) + newLine + "Moist: " + String(percent_reading) + "%";
    int length = displayString.length() + 1;
    char displayArr[length];
    displayString.toCharArray(displayArr, length);
    writeToLcd(displayArr, 3); // Write to LCD for 3 seconds.
    
    // Calculate time between waterings.
    long wateringGapSeconds = (millis() - epochLastWatered[node_id]) / 1000;
    
    // Format String to display last watered in seconds.
    String strWateringGap;
    if(epochLastWatered[node_id] == 0){
      strWateringGap = "Last Watered...|Never..." ;
    } else {
      strWateringGap = "Last Watered...|" +  String(wateringGapSeconds) + " seconds ago..";
    }
    
    // Format and write to lcd last watered time.
    int lenWateringGap = strWateringGap.length() + 1;
    char wateringGapArr[length];
    strWateringGap.toCharArray(wateringGapArr, lenWateringGap);
    writeToLcd(wateringGapArr, 3);

    // If the soil moisture is below the threshold proceed...
    if(percent_reading < LOW_WATER_THRESHOLD){
      
      // If its been more than 12 hours or if the plant has never been watered since the mcu started.
      if(wateringGapSeconds >= 43200 || epochLastWatered[node_id] == 0){ // 43200 seconds in 12 hours
        
          // Calculate for the current node(i) how long to turn the pump on.
          double seconds = calcPumpOnTime(nodeWaterAmounts[node_id], node_id);
        
          // Pump on for X seconds..
          pumpOn(node_id, seconds, nodeWaterAmounts[node_id]);
        
          // Update last watered epoch.
          epochLastWatered[node_id] = millis();

          //Read sensor post to watering.
          double post_percent_reading = readSensorMoisture(node_id);
          
          // Floor the post_percent_reading if it goes above 0% or 100%
          if (post_percent_reading > 100.00){
            post_percent_reading = 100.00;
          } else if(post_percent_reading < 0.00) {
            post_percent_reading = 0.00;
          }

          // Format and write to lcd Post watering reading.
          String postDisplayString = "Post water Read: " + String(node_id) + newLine + "Moist: " + String(post_percent_reading) + "%";
          int postlength = postDisplayString.length() + 1;
          char postDisplayArr[postlength];
          postDisplayString.toCharArray(postDisplayArr, postlength);
          writeToLcd(postDisplayArr, 3);
        
      } else {
        writeToLcd("Skipping pump..|time < 12 hrs...", 3);
      }
    } else {
      writeToLcd("Skipping pump..|Moisture is High", 3);
    }
  }

  
  /* 
   *  This loop has three functions see below.
   *  
   *  1. Wait for INTERVAL seconds before returning the beginninng of the loop function.
   *  2. Reconnects to WIFI if connection was lost, before enabling webserver.
   *  3. While in this loop enable HTTP server communication to update changes.
  */ 
  for (int j = 0; j < INTERVAL; j++){                                                        

      String prefix = String("Http Active for:|");
      String count = String(INTERVAL-j);
      String postFix = String(" seconds...");
      
      String countDown = prefix + count + postFix;
      int countDownLength = countDown.length();

      // Convert back to char array, using buffer 'countDownArr'.
      char countDownArr[countDownLength];
      countDown.toCharArray(countDownArr, countDownLength);

      // If wifi connection hass been lost reconnect.
      while (WiFi.status() != WL_CONNECTED) {
        writeToLcd("Re-connecting|to Wifi...", 0);
        WiFi.begin(ssid, password);
        delay(5000);
      }

      // Enable Http Server
      // Here twice to maintain uptime (times out after 250ms of blocking code)
      server.handleClient();
      writeToLcd(countDownArr, 1);
      server.handleClient();
  }
}

/*
 * This function is used to trigger a specfic relay by setting the relay pins LOW
 * 
 * param int relay_id - id of the relay.
 * param double seconds - how long to toggle the relay
 */
void triggerRelay(int relay_id, double seconds){
  
  // Set Relay to 'ON' complete pump circuit.
  digitalWrite(relay_id, LOW);
  
  // Wait for duration, while relay is 'ON'
  wait(seconds);
  
  //Set Relay to 'OFF' break pump circuit.
  digitalWrite(relay_id, HIGH);
}


/*
 * This function controls how long a relay is triggered completing the Pump circuit.
 * And pumping the water to each plant/node.
 * 
 * param int node_id - id of node
 * param double seconds - how long to keep pump on
 * param double millilitres - millilitres for displaying to LCD.
 */
void pumpOn(int node_id, double seconds, double millilitres){

  //Format the PrePumpMessage String and set to char array for writeToLcd.
  String strMillilitres = String("Pumping Node ") + String(node_id) + "|" + String(millilitres) + " ml";
  int ml_length = strMillilitres.length() + 1;
  char prePumpMessage[ml_length];
  strMillilitres.toCharArray(prePumpMessage, ml_length);
  
  // Match node_id with pump relay pin
  // To trigger the pump for a duration
  switch (node_id){
    
    case 0:
      writeToLcd(prePumpMessage, 1);
      triggerRelay(RELAY_0_PIN, seconds);
      writeToLcd("Finished Pump|Node 0...", 1);
      break;
    
    case 1:
      writeToLcd(prePumpMessage, 1);
      triggerRelay(RELAY_1_PIN, seconds);
      writeToLcd("Finished Pump|Node 1...", 1);
      break;
    
    case 2:
      writeToLcd(prePumpMessage, 1);
      triggerRelay(RELAY_2_PIN, seconds);
      writeToLcd("Finished Pump|Node 2...", 1);
      break;
    
    case 3:
      writeToLcd(prePumpMessage, 1);
      triggerRelay(RELAY_3_PIN, seconds);
      writeToLcd("Finished Pump|Node 3...", 1);
      break;
    
    default:
      writeToLcd("Failed to pump..", 3);
      break;
  }
}


/*
 * This Function Sends Serial data over to the 2nd Board (Nano) that controls the LCD display.
 * 
 * param char dataString[] - String to write to LCD/Send over IC2 bus.
 * param int seconds - Block for X seconds.
 * 
 * return void.
 */
void writeToLcd(char dataString[], int seconds){
  
  // Convert to Ardino String().
  String data = String(dataString);
  
  // Add terminator char marker
  String packet = data + terminator;
  int packetlength = packet.length() + 1;
  
  // Convert back to char array, using buffer 'packetToCharArr'.
  char packetToCharArr[packetlength];
  packet.toCharArray(packetToCharArr, packetlength);
    
  Serial.write(packetToCharArr);
  wait(seconds);
}

// Custom seconds Delay/Wait.
void wait(int seconds){
  delay(seconds * 1000);
}

/*
 * This Function returns the raw reading of a moisture sensor via a ADC.
 * 
 * param int sensor_id - id of the sensor.
 * 
 * returns int
 */
int readSensor(int sensor_id){
  
  int16_t raw_read;
  
  //Match Sensor ID with sensors pin
  switch (sensor_id){
    
    case 0:
      raw_read = ads.readADC_SingleEnded(SENSOR_0_ADC);
      break;
    case 1:
      raw_read = ads.readADC_SingleEnded(SENSOR_1_ADC);
      break;
    case 2:
      raw_read = ads.readADC_SingleEnded(SENSOR_2_ADC);
      break;
    case 3:
      raw_read = ads.readADC_SingleEnded(SENSOR_3_ADC);
      break;
  }
  
  //return raw read
  return raw_read;
}


/*
 * This Function calculates the amount of time the pump needs to be on for,
 * To reach the past param 'amount_ml', using node_id X's Config.
 * 
 * Using the following equation:
 *              in ml          arbituray Num    + time taken to fill tube, then divided 1000 for seconds.
 * time_on = (amount of water * water_flow_rate + tube volumne/length) / 1000
 * 
 * param double amount_ml - amount to pump
 * param int node_id - node id
 * 
 * returns double
 */
double calcPumpOnTime(double amount_ml, int node_id){
  
  double seconds = 0.0;
     
  //Match node_id with pump + tube config values
  switch (node_id){
    
    case 0:
      //seconds = (amount of water * water_flow_rate + tube volumne/length) / 1000
      seconds = (amount_ml * 80.0 + 600.0) / 1000.0;
      break;
    
    case 1:
      seconds = (amount_ml * 70.0 + 600.0) / 1000.0;
      break;
    
    case 2:
      seconds = (amount_ml * 75.0 + 600.0) / 1000.0;
      break;
    
    case 3:
      seconds = (amount_ml * 83 + 600.0) / 1000.0;
      break;
  }
  
  return seconds;
}


/*
 * This Function reads the Sensor based on the param sensor_id
 * 
 * It takes reading and converts it into a scale from 0% mosit to 100% moist.
 * 
 * param int sensor_id - sensors id
 * 
 * returns double
 */
double readSensorMoisture(int sensor_id){
  
  // Read Moisture Sensor X raw value.
  int raw_read = readSensor(sensor_id);
  
  // Get max Wet/Dry config for sensor X.
  int raw_wet = sensorConfigWet[sensor_id];
  int raw_dry = sensorConfigDry[sensor_id];
  
  // Calculate Percentage.
  double percentage = map(raw_read, raw_dry, raw_wet, 0, 100);
  
  // Return Percentage
  return percentage;
}

/*
  ----------------------------------------------------------------
  ----------------------Web Server Functions START----------------
  ----------------------------------------------------------------
  - Below is all the functions related to the Webserver
*/

/* 
 *  Function handleRoot
 *  This method sents html page with forms the user can interact with..
 *  Handle requests to '/'
 *  
 *  returns void
*/ 
void handleRoot() {
  server.send(200, "text/html", htmlBuilder());
}

/* 
 *  Function handleUpdate
 *  This method is used to update variables in the application.
 *  By handling requests to /update/
 *  
 *  Depending on the argument passed by a form.
 *  
 *  Example if argument is node_0_update_ml=30.00
 *  
 *  node_0_update_ml water amount will be changed to 30.00 ml
 *  
 *  MAX = 50.00
 *  MIN value = 10.00
 *  
 *  returns void
*/ 
void handleUpdate() {
  if (server.method() != HTTP_POST) {
    // If its Not a Post request return an error..
    server.send(405, "text/plain", "invalid method, POST only..");
  } else {

    // Check Args sent from Page, update that node if less than max
    if(server.hasArg("node_0_update_ml")){
      double value = String(server.arg("node_0_update_ml")).toDouble();
      if (value <= MAX_ML && value >= MIN_ML){
        nodeWaterAmounts[0] = value;
      } else {
        error404();
      }
    } else if (server.hasArg("node_1_update_ml")){
      double value = String(server.arg("node_1_update_ml")).toDouble();
      if (value <= MAX_ML && value >= MIN_ML){
         nodeWaterAmounts[1] = value;
      } else {
        error404();
      }
    } else if (server.hasArg("node_2_update_ml")){
      double value = String(server.arg("node_2_update_ml")).toDouble();
      if (value <=  MAX_ML && value >= MIN_ML){
         nodeWaterAmounts[2] = value;
      } else {
        error404();
      }
    } else if (server.hasArg("node_3_update_ml")){
      double value = String(server.arg("node_3_update_ml")).toDouble();
      if (value <= MAX_ML && value >= MIN_ML){
        nodeWaterAmounts[3] = value;
      } else {
        error404();
      }
    } else {
      // None of the expect args returned display an error.
      error404();
    }

    // Display to the user the update they made..
    server.send(200, "text/plain", "POST body was:\n" + server.arg("plain"));
  }
}

/* 
 *  Function error404
 *  
 *  This method is used to return errors if a bad request is made to the api.
 *  
 *  returns void
*/ 
void error404() {
  server.send(404, "text/plain", "An Error occured");
}


/* 
 *  Function htmlBuilder
 *  This method is used to build a HTML page.
 *  
 *  It contains forms for the user to update water amounts to each node.
 *  It contains data about each node.
 *  
 *  returns void
*/ 
String htmlBuilder(){
  
  String htmlStart = "<html>";
  
  String head = "<head><title>Watering System Server</title><style>body{background-color: #132639;Color: #cccccc;font-family: Arial;}</style></head>";

  String pageHeader = "<h1>Watering System</h1><br><h3>Plants are watered every 12 hours...Update Range between 10.00ml to 50.00ml Otherwise throws error</h3></br>";
  
  String bodyStart = "<body>";

  String bodyNode_0_last_watered = "  ----   Last Watered: " + String(epochLastWatered[0] / 1000) + " seconds ago...";
  String bodyNode_1_last_watered = "  ----   Last Watered: " + String(epochLastWatered[1] / 1000) + " seconds ago...";
  String bodyNode_2_last_watered = "  ----   Last Watered: " + String(epochLastWatered[2] / 1000) + " seconds ago...";
  String bodyNode_3_last_watered = "  ----   Last Watered: " + String(epochLastWatered[3] / 1000) + " seconds ago...";

  String bodyNode_0 = "<p><bold>Node_0</bold> - Watering amount(ml): " + String(nodeWaterAmounts[0]) + bodyNode_0_last_watered + "</p>";
  String bodyNode_1 = "<p><bold>Node_1</bold> - Watering amount(ml): " + String(nodeWaterAmounts[1]) + bodyNode_1_last_watered + "</p>";
  String bodyNode_2 = "<p><bold>Node_2</bold> - Watering amount(ml): " + String(nodeWaterAmounts[2]) + bodyNode_2_last_watered + "</p>";
  String bodyNode_3 = "<p><bold>Node_3</bold> - Watering amount(ml): " + String(nodeWaterAmounts[3]) + bodyNode_3_last_watered + "</p>";

  String formNode_0 = String("<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/update/\">")
                      + String("<input type=\"text\" name=\"node_0_update_ml\" value=\"20.00\"><br>")
                      + String("<input type=\"submit\" value=\"Submit\">")
                      + String("</form>");
  
  String formNode_1 = String("<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/update/\">")
                      + String("<input type=\"text\" name=\"node_1_update_ml\" value=\"20.00\"><br>")
                      + String("<input type=\"submit\" value=\"Submit\">")
                      + String("</form>");
  
  String formNode_2 = String("<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/update/\">")
                      + String("<input type=\"text\" name=\"node_2_update_ml\" value=\"20.00\"><br>")
                      + String("<input type=\"submit\" value=\"Submit\">")
                      + String("</form>");
  
  String formNode_3 = String("<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/update/\">")
                      + String("<input type=\"text\" name=\"node_3_update_ml\" value=\"20.00\"><br>")
                      + String("<input type=\"submit\" value=\"Submit\">")
                      + String("</form>");
    
  String bodyEnd = "</body>";
  String htmlEnd = "</html>";

  // Return the Generated HTML string.
  return htmlStart +
          head + 
            pageHeader +
              bodyStart +
                bodyNode_0  + formNode_0 +
                bodyNode_1  + formNode_1 +
                bodyNode_2  + formNode_2 +
                bodyNode_3  + formNode_3 +
              bodyEnd + 
         htmlEnd;
}

/*
  ----------------------------------------------------------------
  ----------------------Web Server Functions END------------------
  ----------------------------------------------------------------
*/
