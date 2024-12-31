#include "BoardData.h"
#include <Arduino.h>

BoardData::BoardData(){}

String BoardData::toJson() {
    JsonDocument boardDoc;

    boardDoc["setupRacers"] = setupRacers ? 1 : 0;
    boardDoc["setup"] = setup ? 1 : 0;
    boardDoc["ready"] = ready ? 1 : 0;
    boardDoc["race"] = race ? 1 : 0;
    boardDoc["clock"] = clock;
    boardDoc["boardReset"] = boardReset;
    JsonArray racerArr = boardDoc["racers"].to<JsonArray>();
    JsonArray proximityArr = boardDoc["proximity"].to<JsonArray>();
    for (size_t i = 0; i < 4; i++) 
    { 
        racerArr.add(racers[i]?1:0); 
        proximityArr.add(racerProximity[i]);
    }
    
    // Measure the size required to serialize the JSON document
    size_t jsonSize = measureJson(boardDoc) + 1; // +1 for the null terminator

    // Allocate a buffer of the measured size
    char output[jsonSize];

    // Serialize the JSON document into the buffer
    size_t len = serializeJson(boardDoc, output, jsonSize);

    // Convert the buffer to a String object
    String boardOutput = output;

    boardDoc.clear();

    return boardOutput;
}

