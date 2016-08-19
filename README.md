# OTIMCON

Open TIMing CONtrol is a hardware part of a sport timing 
system. The system consists of controls (stations) which have RFID 
readers/writers, while the competitors wear RFID chips.


### Description
OTIMCON hardware consists of:
- Arduino board (currently only ATmega 328 based)
- DS3231 RTC board
- MRFC522 RFID board


It is loosely based on popular orienteering systems, and it's best used for orienteering, trekking, and other sports in which timing on several controls are needed.

This is done as the price of RFID modules went down, good RTC is easy to find, and microcontroller is just a glue. It's open so other makers can upgrade the system, or add additional modules like communication over WiFi / Bluetooth, a small screen with information etc.


### Usage of the system
1. before start a CLEAR control (station) is used to clear out the card and prepare it
2. on start an additional station can be used (with control ID 251). On mass start such station isn't needed.
3. on the racetrack the competitors touch the RFID card by the control, where each control has its unique id (from 1 to 250)
4. on the finish the competitors touch against the finish control (ID 252).
5. after the finish the station in READOUT mode is used to read the timing on each control.
      
Before step 1. an administrator can reset the backup memory of each control so the memory of which cards passed can be saved.
      
### Usage of the station
Stations support serial communication (9600 8 N 1). All configuration is done through serial terminal. Use "?" for help.
_Note_ : use Arduino serial monitor for communication or a system that sends _newline_ at the end of the line, as most systems don't.

### Serial commands
* __SET__ 
	* TIME YYYYMMDDhhmmss : Set time in year[Y], month[M], day[D], hour[h], minute[m], second[s] format  (eg. 20160817120024 is 2016/08/17 12:00:24)
	* CTRL number         : Set control ID as number (1-250), with special codes for START and FINISH stations (defined in code)
	* MODE type           : Set modes (possible modes are CONTROL, CONTROL_WITH_READOUT, READOUT, CLEAR)
	* RESET_BACKUP        : Reset pointer to 0th location on external EEPROM backup memory. In reality not neccessary as it rolls over, but nice to have a clean start.

* __GET__
	* TIME                : Get time in format YYYY/MM/DD hh:mm:ss - year[Y], month[M], day[D], hour[h], minute[m], second[s] eg. 2016/08/17 12:00:24
	* CTRL                : Get control ID as number (1-250 for normal controls, special codes for START and FINISH)
	* MODE                : Get mode status (CONTROL, CONTROL_WITH_READOUT, READOUT, CLEAR)
	* BACKUP              : Get backup of whole external EEPROM (backup) memory. It returns all locations, even those which are not used.
	* VERSION             : Get current firmware version
	* VOLTAGE             : Get current battery voltage if the battery is connected to analog port 2.

### Questions:
* What are CONTROL, CONTROL_WITH_READOUT, READOUT and CLEAR controls?
	* CONTROL is a normal station which saves data to RFID card, and sends (via serial) the UID of the card, timestamp in UNIX epoch format and a readable time.
	* CONTROL_WITH_READOUT does the same as control, but additionaly reads every previous control off the card. This is usable if some of the controls in the middle of the race are connected over Internet. Also it's usable to have one control for FINAL + automatic readout
	* READOUT reads every written control on the RFID card, and sends it via serial.
	*CLEAR completely erases all the data on the RFID card.


* How many controls can one RFID card accommodate?
	* Short answer: 125
	* Long answer: The system is prepared for MiFare 1K cards, which have 64 blocks (divided in 16 sectors) of 16 bytes. Each sector has 3 usable blocks, and one trailer block which has special meaning. First sector (blocks 0 - 3)is also off the limits. Second sector is prepared for info block (block 4) while blocks 5 and 6 are left free for anything (eg. competitor names). Data starts at block 8. Out of 56 blocks, there are 14 trailer blocks which are unusable, so control data can come to 42 blocks. Data about the control is saved in 5 bytes: 1 byte is control ID (number between 0-255), and 4 bytes is UNIX timestamp of the coming to control. So there are 3 controls per block - in total 42 x 3 = 126. There is a hard limit in code to 125. 
	* For an visual answer look at the [CardAllocation](https://github.com/stajp/otimcon/blob/master/CardAllocationOTIMCON.ods)
	


* Is data safe on RFID card?
	* Data retention of the RFID memory is 10 years, or 200000 writes. So it's OK for a normal use. Additionaly, every block on the card has an additional CRC8 check on the end, so the data is written as it should.

 
* For what is BACKUP used?
	* If there is support for external EEPROM, then after writing data about the control onto the RFID chip, there is an additional write to external EEPROM with UID of the RFID card and timestamp.


* How big is BACKUP?
	* Backup saves last 512 cards (one card can be more than once, for those who forget to erase).


* What is info block in the RFID card?
	* Info block is block 4, where the last location is saved, last control and time of the last control.

