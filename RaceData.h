#include <ArduinoJson.h>

/**
 * @brief Class to hold information about the race
 */
class RaceData
{
    public:
        /**
         * @brief Enum for the state of the race
         */
        enum Status
        {
            SETUP = 10,
            READY = 20,
            COUNTDOWN = 30,
            RACE  = 40,
            FINISHED = 50
        };

        RaceData(); 

        /**
         * @brief Update the participation of the racers
         * 
         * @param racers int array of the participation of the racers where 0 is no and 1 is yes
         */
        void updateRacers(int racers[]);
        
        /**
         * @brief Update the participation of the racers
         * 
         * @param racers JsonArray of the participation of the racers where 0 is no and 1 is yes
         */
        void updateRacers(JsonArray arr);

        /**
         * @brief Reset the data for the race including message, position, times
         */
        void resetData();

        /**
         * @brief String of the participation of the racers
         * 
         * Used for debug purposes
         * 
         * @return String String of the racer participation where X is not participating 
         * and O is participating
         */
        String racerString();

        /**
         * @brief Output the important data into a json string
         * 
         * @return String Json string of the data
         */
        String toJson();

        /**
         * @brief The elapse time of the race
         * 
         * @return unsigned long the time in milliseconds
         */
        unsigned long elapsedTime();

        /**
         * @brief Start the race
         */
        void StartRace();

        /**
         * @brief Clear the race timer for the race 
         */
        void ClearTimer();

        volatile bool inRace[4]; // participation of the racers
        volatile int finishPosition[4]; // finish position of the racers
        unsigned long finishTime[4]; // finish time of the racers
        unsigned long startTime = 0; // start time of the race
        String msg; // messsage for the race
        Status status; // status (state) of the race


    private:
        /**
         * @brief Output the status of the race to a string
         * 
         * @param stat The status to output
         * @return String String representation of the status
         */
        String statusToString(Status stat);
};