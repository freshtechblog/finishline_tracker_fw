#include <ArduinoJson.h>
#include "RaceData.h"
#include "Utils.h"
#include "BoardData.h"
#include <Wire.h>
#include <Adafruit_VCNL4040.h>
#include <Adafruit_Protomatter.h>
#include <Arduino.h>

bool debugMode = true; // set true to see printouts on serial

// I2C Multiplexer address
#define TCAADDR 0x70
#define MINI328_I2C_ADDRESS 0x08 // I2C address of the Metro Mini 328

// RGB matrix panel screen dimensions
#define WIDTH 64 
#define HEIGHT 32 

#define TIMEOUT 50 // timeout for reading data
#define SCREENTIME 250 // time to display message on screen during boot

// Metro M4 values for RGB matrix panel
#define CLK A4 // USE THIS ON METRO M4 (not M0)
#define OE 9
#define LAT 10
#define A A0
#define B A1
#define C A2
#define D A3
uint8_t rgbPins[] = {2, 3, 4, 5, 6, 7}; // All on PORTA 
uint8_t addrPins[] = {A, B, C, D}; // All on PORTA 
uint8_t clockPin = 12; // PORTA 
uint8_t latchPin = 13; // PORTA 
uint8_t oePin = 14; // PORTA 

// LED panel class
Adafruit_Protomatter matrix(WIDTH, 4, 1, rgbPins, 4, addrPins, CLK, LAT, OE, true);

// Proximity sensor, shared across sensors
Adafruit_VCNL4040 vcnl4040;

volatile bool racerInterruptFlags [4]; // interrupt status for sensors

const int startTimeInterruptPin = 11; // output to sensor microcontrollers for start time
const int raceStartInterruptPin = 12; // interrupt pin for direct triggering of race
bool cleared = false; // clear the screen flag

volatile bool raceStartInterruptFlag = false; // flag for direct start of race

RaceData raceData;
BoardData boardData;

String ErrorJson = "{\"status\":\"ERROR\"}";
String bottomText[] = {"X", "X", "X", "X"};
String old_msg = ""; // save the old message to minimize screen refresh
unsigned long lastConfigTime = millis(); // track the last config time to prevent frequent updates

void setup() {  
  matrix.begin();

  writeDebugText("begin \nsetup");
  delay(SCREENTIME);

  raceData = RaceData();
  boardData = BoardData();
  
  writeDebugText("connect \nserial");
  delay(SCREENTIME);

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  // if (debugMode) {
  //   while (!Serial) {
  //      ; // wait for serial port to connect. Needed for native USB port only
  //   }
  //   Serial.println("Serial connected");
  // }
  Serial1.begin(115200);
  // if (debugMode) {
  //   Serial.println("Serial1 connected");
  // }
  writeDebugText("Serial \nconnected");
  delay(SCREENTIME);

  pinMode(raceStartInterruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(raceStartInterruptPin), handleStartRace, FALLING);

  pinMode(startTimeInterruptPin, OUTPUT);
  digitalWrite(startTimeInterruptPin, HIGH);
  delay(50);
  digitalWrite(startTimeInterruptPin, LOW);
  delay(50);
  digitalWrite(startTimeInterruptPin, HIGH);
  Wire.begin();
  
  // Initialize sensors on all channels
  for (uint8_t i = 0; i < 4; i++) {
    tcaSelect(i);
    if (!vcnl4040.begin()) {
      if (debugMode) Serial.println("Sensor " + String(i) + " not found! Check wiring.");
      while (1);
    }
    if (debugMode) Serial.println("Sensor " + String(i) + " initialized.");
    
    vcnl4040.setProximityLEDCurrent(VCNL4040_LED_CURRENT_200MA);
    delay(10); // Add a 10ms delay
    vcnl4040.setProximityLEDDutyCycle(VCNL4040_LED_DUTY_1_320);
    delay(10); // Add a 10ms delay
    vcnl4040.setAmbientIntegrationTime(VCNL4040_AMBIENT_INTEGRATION_TIME_80MS);
    delay(10); // Add a 10ms delay
    vcnl4040.setProximityIntegrationTime(VCNL4040_PROXIMITY_INTEGRATION_TIME_1T);
    delay(10); // Add a 10ms delay
    vcnl4040.setProximityHighResolution(false);
    delay(10); // Add a 10ms delay

    vcnl4040.setProximityLowThreshold(4); 
    delay(10); // Add a 10ms delay
    vcnl4040.setProximityHighThreshold(5);
    delay(10); // Add a 10ms delay
    vcnl4040.enableProximityInterrupts(proximity_type::VCNL4040_PROXIMITY_INT_CLOSE); // Enable the proximity interrupt 
    delay(10); // Add a 10ms delay

    writeDebugText("sensor " + String(i) + "\nconfig'd");
    delay(SCREENTIME);
  }

  writeDebugText("setup \ncomplete");
  delay(SCREENTIME);

  if (debugMode) Serial.println("HW | connected!");
  clearScreen();
  lines();
}


void loop() { 
  boardData.clock = millis();
  String method = "";

  updateBoardData();

  //read in any serial data. returns true if successful
  String json_str = readSerial();
  
  // get the method name
  if (json_str!="") {
    JsonDocument jdoc;
    
    // Parse the JSON data
    DeserializationError error = deserializeJson(jdoc, json_str);
    method = GetStringValue(jdoc.as<JsonObject>(), "method");
    method.toLowerCase();
  }  

  //output the board data if meth is "board"
  if (method == "board") {
    OutputBoardData();
    return;
  }

  /* state machine to control the race */
  if (raceData.status==RaceData::SETUP) { //if race is in setup, allow setup tasks
    if (method == "ready" || boardData.ready) { // switch to ready mode
      raceData.status = RaceData::READY;
      raceData.msg = "READY";
      raceStartInterruptFlag = false; //set the race start interrupt to false to clear
    } else if (method == "config" || boardData.setupRacers) { // config the race
      if (boardData.setupRacers) { // config the race using the input board data
        if (millis()-lastConfigTime>1000) {
          lastConfigTime=millis();
          String str = "{\"inRace\":[" +
             String(boardData.racers[0] == 1 ? "1" : "0") + "," +
             String(boardData.racers[1] == 1 ? "1" : "0") + "," +
             String(boardData.racers[2] == 1 ? "1" : "0") + "," +
             String(boardData.racers[3] == 1 ? "1" : "0") + "]}";
          ConfigRace(str);
          setTopText("SETUP");
          lines();
        }
      } else {
        ConfigRace(json_str); 
        setTopText("SETUP");
        lines();
      }
    }
  }
  //else if race is in ready
  else if (raceData.status == RaceData::READY) { // if in ready state, allow start of race or setup
    if (raceStartInterruptFlag == true) { // skip to starting race if the interrupt was called
      raceStartInterruptFlag = false;
      raceData.StartRace();
      raceData.msg = "GO!";
      clearSensorInterrupts();
      digitalWrite(startTimeInterruptPin, LOW);
      raceData.StartRace();
      delay(25);
      digitalWrite(startTimeInterruptPin, HIGH);
      raceData.status = RaceData::RACE;
    }
    if (method == "setup" || boardData.setup) { // go back to steup state
      raceData.status = RaceData::SETUP;
      for (int i = 0; i < 4; i++) {
        if (bottomText[i] != "X") 
          bottomText[i]="";
      }
      raceData.msg = "SETUP";
    } else if (method == "race" || boardData.race) { // start the race with a countdown
      raceData.status = RaceData::COUNTDOWN;
      //set the start time so the count down can begin
      raceData.StartRace();
    }
  } 
  else if (raceData.status == RaceData::COUNTDOWN) { //perform countdown
    unsigned long elapsedTime = raceData.elapsedTime();
    if(elapsedTime < 1000)
      raceData.msg = "3";
    else if(elapsedTime < 2000)
      raceData.msg = "2";
    else if(elapsedTime < 3000)
      raceData.msg = "1";
    else {
      raceData.msg = "GO!";
      clearSensorInterrupts();
      digitalWrite(startTimeInterruptPin, LOW);
      raceData.StartRace();
      delay(25);
      digitalWrite(startTimeInterruptPin, HIGH);
      raceData.status = RaceData::RACE;
    }
  }
  else if (raceData.status == RaceData::RACE) { // else if in race, clear the race and start it
    //update the message for time
    unsigned long elapsedTime = raceData.elapsedTime();
    if(elapsedTime>1000){
      //update the time of the race
      unsigned long seconds = elapsedTime / 1000;
      unsigned long milliseconds = elapsedTime % 1000;
      String msg_time = String(seconds) + ".";
      // Ensure milliseconds are printed as three significant figures 
      if (milliseconds < 100) 
        msg_time+="0";
      if (milliseconds < 10) 
        msg_time+="0"; 
      msg_time+=String(milliseconds)+"s";
      raceData.msg = msg_time;
    }
   
    CheckFinish(); // check the results of the race

    //state changes
    if (method == "setup" || boardData.setup) { // go to setup state
      raceData.status = RaceData::SETUP;
      resetRace();
    } else if (IsRaceFinished()) { // if race finished, go to finish state 
      raceData.msg = "FINISHED";
      raceData.status = RaceData::FINISHED;
    }
  }
  else if (raceData.status==RaceData::FINISHED) { // if in finish state, allow to go back to setup
    if (method == "setup" || boardData.setup) {
      raceData.status = RaceData::SETUP;
      resetRace();
    }
  }

  // push out the race data as a json str to guarantee a json is returned if one is received
  if (method != "") {
    Serial.println(raceData.toJson());
    Serial.flush();
  }
  
  // update the screen
  if (raceData.msg != old_msg) { 
    old_msg = raceData.msg;
    if(IsSpecialMessage()) { // show fullscreen for 1,2,3 or GO!
      writeFullScreenText(raceData.msg.c_str());
      cleared = false;
    } else { // update the the screen text
      if (!cleared) {
        cleared = true;
        clearScreen();
      }
      lines();
      setTopText(raceData.msg.c_str());
      setBottomText(bottomText);
    }
  }
  clearBadData();
}


/** 
 * @brief Handles the interrupt for starting the race by setting start flag
 * 
 * The flag that is set for this method call is meant to skip the countdown
 * for the start of the race
*/
void handleStartRace() {
  raceStartInterruptFlag = true;
}

/**
 * @brief Updates the input board data
 * 
 * This pulls data from the input board over serial comm
 */
void updateBoardData() {

  unsigned long t_out = millis();
  while(Serial1.available() > 0) 
  {
    Serial1.read();
    delay(1);
    //stop trying to get data if it is taking too long
    if(millis()-t_out>TIMEOUT)
    { 
      if(debugMode) {Serial.println("Clearing buffer timeout");}
      return;
    }
  }

  byte dat;
  Serial1.write('R'); 
  // Wait for a response from Arduino Uno 
  while(Serial1.available() == 0) 
  { 
    delay(1);
    //stop trying to get data if it is taking too long
    if(millis()-t_out>TIMEOUT)
    {
      if(debugMode) {Serial.println("Timeout from Input board");}
      return;
    }
  }
  dat = Serial1.read(); 

  // data is sent as one byte with bits starting from LSB:
  // setup trigger, racer setup trigger, ready trigger,
  // star the race trigger, racer 1 participating,
  // racer 2 participating, racer 3 participating,
  // racer 4 participating
  boardData.setup = dat & 0b00000001;
  boardData.setupRacers = dat & 0b00000010;
  boardData.ready = dat & 0b00000100;
  boardData.race = dat & 0b00001000;

  boardData.racers[0] = (dat & 0b00010000)==0?0:1;
  boardData.racers[1] = (dat & 0b00100000)==0?0:1;
  boardData.racers[2] = (dat & 0b01000000)==0?0:1;
  boardData.racers[3] = (dat & 0b10000000)==0?0:1;
}

/**
 * @brief Select the channel on the I2C multiplexer
 * 
 * @param i The channel to select
 */
void tcaSelect(uint8_t i) {
  if (i > 7) return;  // Validate the channel number
  uint8_t error = -1;
  unsigned long startMillis = millis();

  while(error != 0) {
    Wire.beginTransmission(TCAADDR);  // Address of TCA9548A multiplexer
    Wire.write(1 << i);               // Select channel
    error = Wire.endTransmission();   // End transmission and capture error code

    if (error != 0) {
      boardData.boardReset++;
    }
    if (millis() - startMillis > TIMEOUT) {
      if (debugMode) {Serial.println("I2C channel select timeout");}
      break;
    }
  }
}

/**
 * @brief Clear bad data received from the sensor boards
 * 
 * @details Sometimes the proximity boards will return 65535. This is clearly wrong
 * and the data should not be considered reliable.
 */
void clearBadData() {
  //reset the I2C comm if the sensors report bad data
  if(boardData.racerProximity[0] == 65535 ||
      boardData.racerProximity[1] == 65535 ||
      boardData.racerProximity[2] == 65535 ||
      boardData.racerProximity[3] == 65535) {
    boardData.boardReset++;
  }
}

/**
 * @brief Go through the promixity sensors on the multiplexer and get the interrupt
 * 
 * @details the interrupt status on the sensors need to be read in order for it to
 * trigger again
 */
void clearSensorInterrupts() {  
  for(int i = 0; i<4;i++) {
    //get the data from the sensor
    tcaSelect(i);

    //need to call this to re-enable the interrupt
    vcnl4040.getInterruptStatus();      
  }
}

/**
 * @brief Reset the data for the race. this includes the screen
 */
void resetRace() {
  raceData.resetData();
  clearScreen() ;
  lines();
  for (int i = 0; i < 4; i++) {
    racerInterruptFlags[i] = false;
    if (bottomText[i] != "X")
      bottomText[i]="";
  }
  setBottomText(bottomText);
  raceData.msg = "SETUP";
}

/**
 * @brief checks if the message is special and should be shown fullscreen
 * 
 * @return true if the message is 1,2,3 or GO!
 * @return false if anything else
 */
bool IsSpecialMessage() {
  return (raceData.msg == "3" ||
    raceData.msg == "2" ||
    raceData.msg == "1" ||
    raceData.msg == "GO!");
}

/**
 * @brief Checks to see if the race has completed
 * 
 * Determines if the race is completed by checking to see if participating
 * races have been giving a finish position.
 * 
 * @return true if all participating racers have finished the race
 * @return false if not all participating racers have finished the race yet
 */
bool IsRaceFinished() {
  return (raceData.finishPosition[0] !=0 || raceData.inRace[0] == 0) &&
  (raceData.finishPosition[1] != 0 || raceData.inRace[1] == 0) &&
  (raceData.finishPosition[2] != 0 || raceData.inRace[2] == 0) &&
  (raceData.finishPosition[3] != 0 || raceData.inRace[3] == 0);
}

/**
 * @brief Check the race data and determine assign finish positions to the racers
 * 
 */
void CheckFinish() {
  for (int i = 0; i < 4; i++) { // check the four racers
    if (raceData.inRace[i] && raceData.finishPosition[i] == 0) { // only look at participating racers who haven't finished
      if(racerInterruptFlags[i] == false) { // check if interrupt flags have been set by the sensors
        tcaSelect(i);
        uint8_t intrpt = vcnl4040.getInterruptStatus();    
        if(intrpt == 2) // 2 means the sensor detected a close interrupt
          racerInterruptFlags[i] = true;
      }

      if(racerInterruptFlags[i] == true) { // get the race time for the racers who have finished
        tcaSelect(i + 4);

        unsigned long startMillis = millis();
        int bytesRequested = Wire.requestFrom(MINI328_I2C_ADDRESS, sizeof(unsigned long));
        if (bytesRequested != sizeof(unsigned long)) {
          Serial.println("data received from sensor is wrong size");
          // Skip this iteration if the request fails
          continue;
        }

        // Add a timeout to prevent hanging
        bool timeoutOccurred = false;
        while (Wire.available() < sizeof(unsigned long)) {
          if (millis() - startMillis > TIMEOUT) { 
              timeoutOccurred = true;
              Serial.println("Timeout occured getting sensor data");
              break;
          }
        }

        if (!timeoutOccurred && Wire.available() >= sizeof(unsigned long)) {
          unsigned long elapsedTime;
          Wire.readBytes((char*)&elapsedTime, sizeof(unsigned long));

          if (elapsedTime != 0) {
              raceData.finishTime[i] = elapsedTime;
              racerInterruptFlags[i] = false;
          }
        }
      }
    }
  }

  // figure out the finish positions based on the finish times using Insertion Sort
  unsigned long sortedArray[4]; 
  int sortedPositions[4];

  // Copy the array and initialize positions
  for (int i = 0; i < 4; i++) {
      sortedArray[i] = raceData.finishTime[i];
      sortedPositions[i] = (raceData.finishTime[i] == 0) ? 0 : -1; // Initialize positions, 0 for zero values
  }

  // Insertion Sort implementation
  for (int i = 1; i < 4; i++) {
      unsigned long key = sortedArray[i];
      int j = i - 1;
      while (j >= 0 && sortedArray[j] > key) {
          sortedArray[j + 1] = sortedArray[j];
          j = j - 1;
      }
      sortedArray[j + 1] = key;
  }

  // Assign positions
  int currentPosition = 1;
  for (int i = 0; i < 4; i++) {
      if (sortedArray[i] != 0) {
          for (int j = 0; j < 4; j++) {
              if (raceData.finishTime[j] == sortedArray[i] && sortedPositions[j] == -1) {
                  sortedPositions[j] = currentPosition;
                  currentPosition++;
                  break;
              }
          }
      }
  }

  // Copy sorted positions back to the original positions array
  for (int i = 0; i < 4; i++) {
      raceData.finishPosition[i] = sortedPositions[i];
      if (raceData.inRace[i] == 0)
          bottomText[i] = "X";
      else if (raceData.finishPosition[i] == 0)
          bottomText[i] = "";
      else
          bottomText[i] = raceData.finishPosition[i];
  }
}

/**
 * @brief configure the race
 * 
 * @param json_str json string of the data for the race. only has "inRace" data
 */
void ConfigRace(String json_str) {
  clearScreen();
  JsonDocument jdoc;
  
  // Parse the JSON data
  deserializeJson(jdoc, json_str);
  JsonObject obj = jdoc.as<JsonObject>();

  //config racers to be racing or not
  if(obj["inRace"].is<JsonArray>()) {

    // Get the array of bools from the JsonObject 
    JsonArray racersArray = obj["inRace"].as<JsonArray>(); 
    raceData.updateRacers(racersArray);
    
    // Convert to a boolean array 
    for (size_t i = 0; i < racersArray.size(); i++) { 
        bottomText[i]=racersArray[i]==1?"":"X";
    }

    setBottomText(bottomText); // set the bottom text of the screen for participants
  }
}

/**
 * @brief Output the board data to the serial comm
 */
void OutputBoardData() {
  Serial.println(boardData.toJson()); 
  Serial.flush();
}

/**
 * @brief Read the data from the usb serial comm
 * 
 * @return String of the data read from the usb serial comm
 */
String readSerial() {
  String return_str = "";
  String serialText = "";
  unsigned long start_time = millis();
  unsigned long timeout = 500; // Timeout after 1 second
  bool complete_message = true;

  while(Serial.available() > 0 && (millis() - start_time < timeout)) {
    complete_message = false;
    if (Serial.available() > 0) {
      char c = (char)Serial.read();
      serialText += c;
      if (c == '\n') {
        complete_message = true;
        JsonDocument jdoc;
        // Parse the JSON data
        DeserializationError error = deserializeJson(jdoc, serialText);
        // Check for errors in deserialization
        if (error) {
          if(debugMode) {
            Serial.print(F("HW | deserializeJson() failed: "));
            Serial.println(error.f_str());
            Serial.println(ErrorJson);
            Serial.flush();
          }
        } else {
          return_str = serialText;
        }
        serialText = ""; // Clear the string for the next message
        break; // Exit the loop after processing the message
      }
    }
  }

  if (!complete_message) {
    if(debugMode) {
      Serial.println(ErrorJson);
      Serial.flush();
    }
  }

  return return_str;
}

/**
 * @brief Clear the screen of data
 */
void clearScreen() { 
    matrix.fillScreen(matrix.color565(0, 0, 0)); // Fill screen with black 
    matrix.show(); // Update display 
}

/**
 * @brief draw separating lines for the screen
 * 
 * These lines separate the top of the screen from the bottom. They also separate
 * the bottom of the screen into four sections for each lane
 */
void lines() {
    // Draw a horizontal line in the middle
    int middle = (matrix.height() / 2)-3;
    for (int x = 0; x < matrix.width(); x++) {
        matrix.drawPixel(x, middle, matrix.color565(0, 0, 255)); // blue line
    }
    
    int bottomHalfStart = (matrix.height() / 2) - 2;
    int bottomHalfHeight = (matrix.height() / 2) + 2;
    int interval = matrix.width() / 4;
    int shift = 2; // Variable to adjust the horizontal position

    for (int i = 1; i < 4; i++) {
        for (int y = bottomHalfStart; y < matrix.height(); y++) {
            matrix.drawPixel(i * interval, y, matrix.color565(0, 0, 255)); // blue vertical lines
        }
    }
    matrix.show();
}

/**
 * @brief Set the text on the top of the screen
 * 
 * @param text The text that will be written 
 */
void setTopText(const String& text) {
    matrix.setTextSize(2); // Set text size to larger size
    // Clear the area where the text will be written 
    matrix.fillRect(0, 0, matrix.width(), (matrix.height() / 2) - 3, matrix.color565(0, 0, 0)); // Fill top half with black

    matrix.setTextSize(1); // Set text size to original size
    matrix.setTextColor(matrix.color565(255, 255, 255)); // White text
    int16_t x1, y1;
    uint16_t w, h;

    // Convert String to const char*
    const char* cText = text.c_str();

    matrix.getTextBounds(cText, 0, 0, &x1, &y1, &w, &h); // Get text bounds
    int16_t x = (matrix.width() - w) / 2; // Center horizontally
    int16_t y = ((matrix.height() / 2 - 2) / 2) - (h / 2); // Center vertically in the top half accounting for line shift

    matrix.setCursor(x, y);
    matrix.print(cText);

    matrix.show();
}

/**
 * @brief Write full screen text for debug purposes
 * 
 * @param text The text to write to the screen
 */
void writeDebugText(const String& text) {
  matrix.fillScreen(0); // Clear the matrix 
  matrix.setTextSize(1); // Set text size 
  matrix.setTextColor(matrix.color565(255, 255, 255)); // Set text color to white 
  matrix.setCursor(0, 0); // Set cursor position 
  // Convert String to const char*
  const char* cText = text.c_str();
  matrix.print(cText); // Print text to the matrix matrix.show(); // Update the matrix display
  matrix.show();
}

/**
 * @brief Set the text in the four sections of the bottom of the screen
 * 
 * @param texts The text for the four sections of the bottom of the screen
 */
void setBottomText(const String texts[4]) {
  matrix.setTextSize(2); // Set text size to 2
  int bottomHalfStart = (matrix.height() / 2) - 2;
  int bottomHalfHeight = (matrix.height() / 2) + 2;
  int interval = matrix.width() / 4;
  int shift = 2; // Variable to adjust the horizontal position

  for (int i = 0; i < 4; i++) {
      int16_t x1, y1;
      uint16_t w, h;
      matrix.getTextBounds(texts[i].c_str(), 0, 0, &x1, &y1, &w, &h); // Get text bounds

      // Center horizontally in its section and adjust by shift
      int16_t x = (i * interval) + (interval / 2) - (w / 2) + shift;
      // Center vertically in bottom half
      int16_t y = 1 + bottomHalfStart + (bottomHalfHeight / 2) - (h / 2);

      // Set text color based on content
      if (texts[i] == "X") {
          matrix.setTextColor(matrix.color565(255, 0, 0)); // Red text for "X"
      } else {
          matrix.setTextColor(matrix.color565(255, 255, 255)); // White text for others
      }

      matrix.setCursor(x, y);
      // Clear the area where the text will be written 
      matrix.fillRect(x, y, w, h, matrix.color565(0, 0, 0)); // Clear area with black
      matrix.print(texts[i].c_str());

  }
  matrix.show();
}

/**
 * @brief Write full screen, centered, large, text
 * 
 * @param text The text to write to the screen
 */
void writeFullScreenText(const String& text) {
    clearScreen(); // Clear the screen before writing new text
    matrix.setTextSize(3); // Set text size to larger scale to fill the screen
    matrix.setTextColor(matrix.color565(255, 255, 255)); // White text
    int16_t x1, y1;
    uint16_t w, h;

    // Convert String to const char*
    const char* cText = text.c_str();

    matrix.getTextBounds(cText, 0, 0, &x1, &y1, &w, &h); // Get text bounds
    int16_t x = (matrix.width() - w) / 2; // Center horizontally
    int16_t y = (matrix.height() - h) / 2; // Center vertically

    matrix.setCursor(x, y);
    matrix.println(cText);
    matrix.show(); // Update display
}