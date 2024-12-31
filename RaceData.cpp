#include "Arduino.h"
#include "RaceData.h"

RaceData::RaceData()
{
    for(int i = 0; i<4;i++)
    {
        inRace[i]=false;
        finishPosition[i]=0;
        finishTime[i]=0;
    }
    status = RaceData::SETUP;
    msg = "SETUP";
}
unsigned long RaceData::elapsedTime()
{
    return millis()-startTime;
}

void RaceData::ClearTimer()
{
    startTime = 0;
}

void RaceData::StartRace()
{
    startTime = millis();
}

void RaceData::updateRacers(JsonArray arr)
{
    // Convert to a boolean array 
    for (size_t i = 0; i < arr.size(); i++) 
    { 
        inRace[i]=arr[i]==1?true:false;
    }
}

void RaceData::updateRacers(int racers[])
{
  inRace[0]=racers[0]==1;
  inRace[1]=racers[1]==1;
  inRace[2]=racers[2]==1;
  inRace[3]=racers[3]==1;
}

String RaceData::racerString()
{
    String str = "Racers participating : ";
    str += inRace[0]? "O":"X";
    str += " | ";
    str += inRace[1]? "O":"X";
    str += " | ";
    str += inRace[2]? "O":"X";
    str += " | ";
    str += inRace[3]? "O":"X";

    return str;
}

void RaceData::resetData()
{
    msg = "";
    for(int i = 0; i<4;i++)
    {
        finishPosition[i]=0;
        finishTime[i]=0;
    }    
}

/// @brief Method to serialize the data for this class
/// @return string of the json data
String RaceData::toJson()
{
    JsonDocument doc;
    doc["status"] = statusToString(status);
    doc["message"] = msg;
    JsonArray racerStateArr = doc["inRace"].to<JsonArray>(); 
    JsonArray finishPositionArr = doc["finishPosition"].to<JsonArray>(); 
    JsonArray finishTimeArr = doc["finishTime"].to<JsonArray>(); 
    for (size_t i = 0; i < 4; i++) 
    { 
        racerStateArr.add(inRace[i]?1:0); 
        finishPositionArr.add(finishPosition[i]);
        finishTimeArr.add(finishTime[i]);
    }
    
    
    // Measure the size required to serialize the JSON document 
    size_t jsonSize = measureJson(doc) + 1; // +1 for the null terminator 
    
    // Allocate a buffer of the measured size 
    char output[jsonSize]; // Serialize the JSON document into the buffer 

    size_t len = serializeJson(doc, output, jsonSize);
 
    String raceOutput = output;

    doc.clear(); 

    return raceOutput;
}

String RaceData::statusToString(RaceData::Status stat) 
{ 
    switch (stat) 
    { 
        case RaceData::READY: return "READY"; 
        case RaceData::SETUP: return "SETUP"; 
        case RaceData::COUNTDOWN: return "COUNTDOWN"; 
        case RaceData::RACE: return "RACE";
        case RaceData::FINISHED: return "FINISHED";
        default: return "UNKNOWN";
    }
}
