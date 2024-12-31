#include <ArduinoJson.h>

/**
 * @brief A class to hold the data read in from the boards
 * 
 * This class is mainly meant to be used as a holder of information
 * and a means to output the data into a json string
 */
class BoardData{
    public:

        /**
         * @brief Construct a new Board Data object
         */
        BoardData();

        /**
         * @brief Create a json string of the relevant data 
         * 
         * @return String 
         */
        String toJson();

        bool setup = false; // setup trigger
        bool setupRacers = false; // setup racers trigger
        bool ready = false; // ready trigger
        bool race = false; // start race trigger
        int racers[4]; // participating racers trigger
        uint16_t racerProximity[4]; // proximity values
        unsigned long clock = 0; // clock time since boot
        int boardReset = 0; // how many times the board needed a reset
        
};