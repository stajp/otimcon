# OTIMCON

Open TIMing CONtrol is a hardware part of a sport timing 
system. The system consists of controls (stations) which have RFID 
readers/writers, while the competitors wear RFID chips.

### Description
OTIMCON hardware consists of:
- Arduino board (currently only ATmega 328 based)
- DS3231 RTC board
- MRFC522 RFID board


It is loosely based on popular orienteering systems, and it's best used 
for orienteering, trekking, and other sports in which timing on several
controls are needed.

### Usage of the system
1. before start a CLEAR control (station) is used to clear out the 
card and prepare it
2. on start an additional station can be used (with control ID 251). On 
mass start such station isn't needed.
3. on the racetrack the competitors touch the RFID card by the control, where 
each control has its unique id (from 1 to 250)
4. on the finish the competitors touch against the finish control (ID 252).
5. after the finish the station in READOUT mode is used to read the timing on
each control.
      
### Usage of the station
Stations support serial communication (9600 8 N 1). All configuration is 
done through serial terminal. Use "?" for help.
