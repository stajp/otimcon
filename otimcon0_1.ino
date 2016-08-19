#define SW_VERSION "0.17"
/*
 *************************************************************
 *  Project: Open Timing Control (OTIMCON)
 *  Version: 0.14 
 *  Date: 20160817
 *  Created by: Stipe Predanic (stipe.predanic (at) gmail.com)
 **************************************************************
 *
 * Required:
 *  HW:
 *  - Arduino based board (tested with Arduino Uno - AtMega328 board, should work on other Arduino systems if they support I2C and SPI)
 *  - MRFC522 module http://playground.arduino.cc/Learning/MFRC522
 *  - RTC clock supported by RTCLib (eg. DS1307, RTC_PCF8523, RTC_DS3231)
 *  - EEPROM (optional - used for backup, comes with DS3231 on popular cheap Chinese modules) 
 *  - LED (optional, depending on the way to give feedback)
 *  - piezo buzzer (optional, depending on the way to give feedback)
 * 
 *  SW:
 *  - Arduino IDE on a host computer 
 *  - Arduino libraries:
 *    + MFRC522 (download from http://makecourse.weebly.com/week10segment1.html , others may work)
 *    + RTCLib (any version https://github.com/adafruit/RTClib/network )
 *    + SerialCommand Library ( https://github.com/kiwisincebirth/Arduino-SerialCommand )
 *    + LowPower Library (available in Arduino IDE)
 *    + PinChangeInterrupt (available in Arduino IDE)
 *
 **************************************************************
 *
 * HW connection (if used by 328 type board)
 *  - DS3231 module connected via I2C on A4 (SDA) and A5 (SCL)  
 *  - MFRC522 conected via SPI (D13 SCK, D12 MISO, D11 MOSI, with RESET AND SS are selected in code (in this case SS is D10, and RESET D9)
 *  - LED and or piezo are connected to pin selected in code (in this case LED is on D8 and PIEZO on D7). 
 *  
 *  Don't forget the resistors for current (LED, piezo) or for pull-up and pull-down on SPI/I2C
 *
 **************************************************************
 * History:
 * - version 0.1 20160601 
 *      First version is based on 
 *      - ideas about OPORCON ( my project based on 1-wire iButtons and using PIC16F628, available at http://stipe.predanic.com/?Projekti/OPORCON )
 *      - MFRC example by Rudy Schlaf for www.makecourse.com http://makecourse.weebly.com/week10segment1.html  -> READ IT, WATCH THE VIDEO!!!!!!
 *     Rudely hacked to prove that it works.
 *     
 * - version 0.11 20160605    
 *      - cleaned up the functions needed for this to work
 *                  
 * - version 0.12 20160816
 *      - further cleaning, added read back to check if everything is written as it should, added the bleep and basic sleep
 *      - a lot of ifdef preprocessing added, to make everything clean
 * 
 * - version 0.13 20160816
 *      - added support for readout and clear (NOT TESTED YET!)
 *   
 * - version 0.14 20160816
 *      - added support for serial CLI for setting and getting data 
 *      - support for different modes and control ID's done through CLI
 *      - changed name from OTICON to OTIMCON for search engines
 *      
 * - version 0.15 20160817    
 *      - CLI tested
 *      - added the AD conversion for battery measurement (not tested)
 *      - modes and control saved into ATMega328 EEPROM
 * 
 * - version 0.16 201619     
 *      - make backup of card UID and time to EEPROM on Ebay DS3231 boards
 *
 * - version 0.17 201619     
 *      - sleep for low power work -> done it poorly, should revisit one day
 *         -> done only for microcontroller, RFID and RTC should also go to sleep
 *         -> low power now puts the microcontroller to sleep for 125ms, wakes up, checks the RFID and goes back to sleep.
 *            after reading a card the system doesn't go to any kind of sleep for half a minute(!!!) and serial works as expected (also the reading of the cards is quicker!)
 *            --> if serial is used, that resets the counter for another minute. 
 *         -> if low power is not used (comment out a define), then serial always works as expected.
 *         -> by default LOW POWER is NOT USED!
 *         
 *         
 *            
 *      
 *  
*/


#include <SPI.h>          //include the SPI bus library
#include <MFRC522.h>      //include the RFID reader library
#include <Wire.h>
#include <RTClib.h>       // include timer library 
#include <SerialCommand.h>  // iclude serial command system
#include <EEPROM.h>
#include "LowPower.h"



// used for debugging purposes. Should be commented out in production or set to 0. Higher number, more information
#define DEBUG 0


// comment these if you don't have them, so the code will be smaller
// preprocessing command are used!
#define USE_EEPROM_BACKUP
#define USE_LED
#define USE_PIEZO

// comment this if you want to use serial only if neccessary. If this is uncommented it will always use serial port, even if nothing is connected.
#define USE_SERIAL_ALWAYS  
                     
// comment this if you don't need low power work (the system won't go to sleep but serial will always work)
//#define LOW_POWER  





#define DS3231_I2C_ADDRESS 0x68    // I2C address for DS3231
#define EEEPROM_I2C_ADDRESS 0x57   // default I2C address for EEPROM on DS3231 modules


#define SS_PIN             10                 //slave select pin for SPI (used by MFRC522)
#define RST_PIN             9                 //reset pin

#define FEEDBACK_LED        8                 // pin for LED
#define FEEDBACK_PIEZO     7                 // piezo

#define SERIAL_ACTIVE_PIN   6                // if you want to use serial only when needed, then select a pin (this is D6) and connect it HIGH 

#define LAST_TIME_FROM_WRITING 30             // number of seconds between two consecutive writes if the same card user comes to the station (from now on called control)


#define ANALOG_REFERENCE  EXTERNAL
//#define VOLTAGE_REFERENCE 2                   // used for measuring the voltage, easier to code if 5V is used (error is 2.4%). It will output 512 for 5V.
#define VOLTAGE_REFERENCE 1024. / 500       // used for measuring the voltage, voltage will be as fixed point arithmetic (100 = 1V)  500 = 5V


//#define VOLTAGE_REFERENCE 3                 // used for measuring the voltage, easier to code if 3.3V is used (error is around 3.3%)
//#define VOLTAGE_REFERENCE 1024. / 330       // used for measuring the voltage, voltage will be as fixed point arithmetic (100 = 1V)  330 = 3.3V


// using internal refence for ADC is suggested but it needs to be done right. So by default this is not done!
//#define ANALOG_REFERENCE  INTERNAL
//#define VOLTAGE_REFERENCE 9                 // used for measuring the voltage, easier to code if 1.1V internal reference is used (error is around 3.4%)
//#define VOLTAGE_REFERENCE 1024. / 110       // used for measuring the voltage, voltage will be as fixed point arithmetic (100 = 1V)  330 = 1.1V


#define SERIAL_BAUD 19200

// define ID's for START and FINISH controls. as 1-250 are for controls, available numbers are 253-255 and 0
#define START_CONTROL_ID 251
#define FINISH_CONTROL_ID 252


// define the function of the control
#define CONTROL 1                // works as a standard control with id
#define CONTROL_WITH_READOUT 2   // works as a control which has a readout of all previouos controls
#define READOUT 3                // works as a readout only
#define CLEAR 4                  // works as a clear


#define EEPROM_ADDRESS_MODE       10  // address in EEPROM for saving mode
#define EEPROM_ADDRESS_CONTROL_ID 11  // address in EEPROM for saving control ID   


// change this from time to time for wear leveling, as write is done after _every card_
#define EEPROM_ADDRESS_24C32_LOCATION 16  // location of high part of address in external EEPROM for saving backup of used cards   

#define EXTERNAL_EEPROM_SIZE       4096  // maximum number of bytes in external EEPROM 

static RTC_DS3231 rtc;
static MFRC522 mfrc522(SS_PIN, RST_PIN);        // instatiate a MFRC522 reader object.
static MFRC522::MIFARE_Key key;                 //create a MIFARE_Key struct named 'key', which will hold the card information

static byte controlId;                         // current ID of this station (from now on called control)    
static byte readbackblock[18];                 //This array is used for reading out a block. The MIFARE_Read method requires a buffer that is at least 18 bytes to hold the 16 bytes of a block.
static long currentTime;                // currentTime, used for saving the time from RTC (without the need of continuos read in functions).


boolean useSerial = true;
boolean cardPresent = false;

byte controlFunction;

#ifdef USE_EEPROM_BACKUP
static short int locationOnExternalEEPROM = 0;
#endif

static unsigned short sleepCounter;

static SerialCommand  sCmd   = SerialCommand();       // The demo SerialCommand object
static CommandHandler sHand1 = CommandHandler(sCmd);  // the main command handler
static CommandHandler sHand2 = CommandHandler(sCmd);  // the sub command handler for SET command
static CommandHandler sHand3 = CommandHandler(sCmd);  // the sub command handler for GET command


/**
 * setup function of the Arduino toolchain
 * 
 */
void setup() {
      Serial.begin(SERIAL_BAUD);        // Initialize serial communications with the PC
      SPI.begin();               // Init SPI bus

      mfrc522.PCD_Init();        // Init MFRC522 card (in case you wonder what PCD means: proximity coupling device)
     
      Serial.println(F("Starting..."));     
      // if RTC cannot be started, then there is no RTC.
      if (! rtc.begin()) {
        Serial.println(F("No RTC"));
        while (1);   // sketch halts in an endless loop
      }

      // if RTC is running, check the time. If the time differs more than 15 seconds off the computer clock, then it's set back to computer clock 
      // warning: it will reset back to uploading time, if there is no backup battery on DS3231
      DateTime now = rtc.now();
      DateTime compiled = DateTime(F(__DATE__), F(__TIME__));
      
      if (abs(now.unixtime() - compiled.unixtime()) > 15 ) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      }

        
        controlId = EEPROM.read(EEPROM_ADDRESS_CONTROL_ID);              // set ID of the control station. Any number between 1-250. Beware: it can be anything if new Arduino is used.
         
        controlFunction = EEPROM.read(EEPROM_ADDRESS_MODE);              // set function (mode) of control. Beware: it can be anything if new Arduino is used. 
        if (controlFunction > CLEAR || controlFunction == 0) controlFunction = READOUT;  // if controlFunction is something crazy (eg. new Arduino), set it to READOUT.

#ifdef USE_EEPROM_BACKUP        
       EEPROM.get(EEPROM_ADDRESS_24C32_LOCATION, locationOnExternalEEPROM);
#endif
       
       // define pin modes for LED and PIEZO, if they are used
#ifdef USE_LED
       pinMode(FEEDBACK_LED, OUTPUT);
#endif
#ifdef USE_PIEZO
       pinMode(FEEDBACK_PIEZO, OUTPUT);
#endif

      // if USE_SERIAL_ALWAYS is not set, then it's needed to set a pin SERIAL_ACTIVE_PIN
#ifndef USE_SERIAL_ALWAYS
       pinMode(SERIAL_ACTIVE_PIN, INPUT);
#endif


      
        // Prepare the security key for the read and write functions - all six key bytes are set to 0xFF at chip delivery from the factory.
        // Since the cards in the kit are new and the keys were never defined, they are 0xFF
        // if we had a card that was programmed by someone else, we would need to know the key to be able to access it. This key would then need to be stored in 'key' instead.
        
        // NOTICE: If you want to block reading and writing cards by somebody else, change block3 of each sector on a card and then change this lines to something else.
        for (byte i = 0; i < 6; i++) {
                key.keyByte[i] = 0xFF;//keyByte is defined in the "MIFARE_Key" 'struct' definition in the .h file of the library
        }

/****
*/
        // uncomment this if you want to use internal analog reference of 1.1V
        // BE CAREFUL WITH SETTING THE INPUT.
        // analogReference(INTERNAL);

/*
****** */

  // set the main handler for serial command
  sCmd.setHandler(sHand1);

  // Setup callbacks for the main SerialCommand. These are the main commands
  sHand1.addHandler("SET",    sHand2);          // Handler for everything starting with SET
  sHand1.addHandler("GET",    sHand3);          // Handler for everything starting with GET
  sHand1.addCommand("?",      help);            // Handler for help 
  sHand1.setDefault(          unrecognized);    // Handler for command that isn't matched  (says "What?")

  
  // Setup the commands the SET
  sHand2.addCommand("TIME",  setTime);         // Set time
  sHand2.addCommand("CTRL",  setControl);      // Set control number
  sHand2.addCommand("MODE",  setMode);         // Set mode
  sHand2.addCommand("RESET_BACKUP",   setResetBackup); // reset backup memory pointer to 0
  sHand2.setDefault(         unrecognized);    // Handler for command that isn't matched  (says "What?")
  
  sHand3.addCommand("TIME",  getTime);         // Get current time
  sHand3.addCommand("CTRL",  getControl);      // Get current control number
  sHand3.addCommand("MODE",  getMode);         // Get current mode
  sHand3.addCommand("VOLTAGE",  getVoltage);   // Get battery voltage
  sHand3.addCommand("VERSION",  getVersion);   // Get current firmware version
  sHand3.addCommand("BACKUP",  getBackup);     // Get current memory backup in external EEPROM
  sHand3.setDefault(         unrecognized);    // Handler for command that isn't matched  (says "What?")





#if DEBUG > 0
  Serial.println("Setup finished!");
#endif  
  Serial.print(">");

  sleepCounter = 0;
}


void loop()
{
 
        static boolean timeToBleep;                // the boolean which starts the bleeping (LED & PIEZO activation)
        

      
        
               
/*** 
 *  check serial state - check the #define, by default is on pin D6
 */
#ifdef USE_SERIAL_ALWAYS
    useSerial = true;
#else
    useSerial = (HIGH == digitalRead(SERIAL_ACTIVE_PIN)) ? true : false ;
#endif
     
     
      
     // serial will work for about a minute after the card is read, but other times the system is just sleep <-> RFID
     if (sleepCounter > 0x03FF) {
        Serial.flush();
        sleep();
     } 
     else {
          // check serial, if there is something new on connected link
        if (useSerial) {
            sCmd.readSerial();     // We don't do much, just process serial commands
          }
        sleepCounter++;
     }

// if LOW_POWER is commented out, then it it will never go to sleep and Serial will be more responsive.     
#ifndef LOW_POWER
      Serial.flush();
      sleepCounter = 0;
#endif     
     
   
        /*****************************************establishing contact with a tag/card**********************************************************************/
        
  	// Look for new cards (in case you wonder what PICC means: proximity integrated circuit card)
	if ( ! mfrc522.PICC_IsNewCardPresent()) {     //if PICC_IsNewCardPresent returns 1, a new card has been found and we continue
    cardPresent = false;
    return;                                     //if it did not find a new card is returns a '0' and we return to the start of the loop
	}

	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial()) {       //if PICC_ReadCardSerial returns 1, the "uid" struct (see MFRC522.h lines 238-45)) contains the ID of the read card.
		cardPresent = false;
		return;                                     //if it returns a '0' something went wrong and we return to the start of the loop
	}
  
// If the code comes to here, then a card is found and selected!


// DEBUG CODE
#if DEBUG > 0
         if (useSerial) Serial.println(F("card selected"));
#endif
// END DEBUG CODE

   
    // at the end of this code cardPresent is set to true. If there is no card on the reader, then it's set to false.
    // 
    // So, it will do something only if the card was removed and then new or the same card is presented again.
    // It will bleep the same (or not to bleep if there is an error) until card is removed
    if ( ! cardPresent) {       
      // controlId 1-252 are controls.
      // 1-250 regular controls, 251 is start, 252 is finish
      // work as control should be set up in controlSetup
      if ( controlFunction == CONTROL || controlFunction == CONTROL_WITH_READOUT ) {
          timeToBleep = writeControl();
          if ( timeToBleep && useSerial ) {
                Serial.print(F("^"));
                Serial.print(controlId);
                Serial.print(F(","));

                serialPrintUid();
                
                Serial.print(F(","));
                Serial.print(currentTime);             // this will print just the UNIX timestamp format. This is _by design_!
                Serial.println(F("$"));
                

                if (controlFunction == CONTROL_WITH_READOUT) {
                   readOutAllControls();
                   Serial.println(F("%"));
                }
                Serial.println(F(">"));
            }
      }
      if ( controlFunction == READOUT ) {
        if (  useSerial ) {
          Serial.print(F("Card:"));
          serialPrintUid();
          Serial.println("");
          timeToBleep = readOutAllControls();
          Serial.println(F("%"));
          Serial.println(F(">"));
          
        }
      }

      
      if ( controlFunction == CLEAR  ) {
           timeToBleep = clearCard();
           if ( useSerial && timeToBleep) {
              Serial.print(F("Cleared card:"));
              serialPrintUid();
              Serial.println(F("$"));
              Serial.println(F(">"));
              
                        
           }
      }

    }
      
      // for some reason, my hardware doesn't work without reinitialization.
      // usually there should be some delay between new use (they say 100ms is enough), so do it prior to bleep which is at least 300ms
      mfrc522.PCD_Init();     
      
      // if it's time to bleep, then bleep! :D
      if (timeToBleep) { 
            bleep();
           
      }

     cardPresent = true;
     sleepCounter = 0x1FF;
     
         
/*  DEBUG DATA
        for (int block=0; block<60; block++) { 
           readBlock(block, readbackblock);//read the block back
           Serial.print(F("read block: "));Serial.println(block);
           for (int j=0 ; j<16 ; j++)//print the block contents
           {
             Serial.print (readbackblock[j],HEX);//Serial.write() transmits the ASCII numbers as human readable characters to serial monitor
             Serial.print(" ");
           }
         }
  */        
}


