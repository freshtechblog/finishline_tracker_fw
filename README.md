# finishline_tracker_fw
Firmware to run the race for the finishline tracker.

Firmware controls the main board of the finishline tracker hardware. This firmware runs on a ATSAMD51 chipset. It has been tested on a Metro M4 board.

In addition to this board, the firmware is expecting to talk to another board over serial communication which is responsible for taking physical user input. Firmware for this board can be found at https://github.com/freshtechblog/finishline_input_fw

The firmware also communicates with the finishline proximity sensors and finishline time trackers over I2C. This is done using a I2C multiplexer. The time trackers are separate metro mini 328 v2 boards, one for each lane. The firmware for those boards can be found at https://github.com/freshtechblog/finishline_sensor_fw

Results are displayed on a 63x32 pixel RGB panel.

communication to this board can be done over USB by sending it json strings. sending it a json str with method defined as setup, ready, race will move the firmware through its various racing states. When in setup, you can set the method to config and pass in an "inRace" key with an array of the racers who will be participating. The firmware is set for four lanes so you need to pass in an array of four 0s or 1s. For example, you can pass in {"method":"config","inRace":[1 1 1 0]} when in the setup state to configure the race to have the first three lanes participating in the race.
