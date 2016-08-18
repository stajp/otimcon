#define SW_VERSION "0.14NT"
/*
 *************************************************************
 *  Project: Open Timing Control (OTIMCON)
 *  Version: 0.14NT (Not Tested)
 *  Date: 20160816
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
 *      - added support for serial CLI for setting and getting data (NOT TESTED YET!)
 *      - support for different modes and control ID's done through CLI (NOT TESTED YET!)
 *      - changed name from OTICON to OTIMCON for search engines
 *      
 *
 * To do (either doesn't work or not even started) :
 * - test it 
 * - make a proper sleep
 * - work out the battery voltage measurement
 * - make backup to EEPROM
*/


#include <SPI.h>          //include the SPI bus library
#include <MFRC522.h>      //include the RFID reader library
#include <Wire.h>
#include <RTClib.h>       // include timer library 
#include <SerialCommand.h>  // iclude serial command system

// used for debugging purposes. Should be commented out in production
#define DEBUG 


// comment these if you don't have them, so the code will be smaller
// preprocessing command are used!
#define USE_EEPROM_BACKUP
#define USE_LED
#define USE_PIEZO

// comment this if you want to use serial only if neccessary. If this is uncommented it will always use serial port, even if nothing is connected.
#define USE_SERIAL_ALWAYS                       





#define DS3231_I2C_ADDRESS 0x68    // I2C address for DS3231
#define EEEPROM_I2C_ADDRESS 0x57   // default I2C address for EEPROM on DS3231 modules


#define SS_PIN             10                 //slave select pin for SPI (used by MFRC522)
#define RST_PIN             9                 //reset pin

#define FEEDBACK_LED        8                 // pin for LED
#define FEEDBACK_PIEZO     7                 // piezo

#define SERIAL_ACTIVE_PIN   6                // if you want to use serial only when needed, then select a pin (this is D6) and connect it HIGH 

#define LAST_TIME_FROM_WRITING 30             // number of seconds between two consecutive writes if the same card user comes to the station (from now on called control)


//#define VOLTAGE_REFERENCE 2                   // used for measuring the voltage, easier to code if 5V is used (error is 2.4%). It will output 512 for 5V.
#define VOLTAGE_REFERENCE 1024. / 500       // used for measuring the voltage, voltage will be as fixed point arithmetic (100 = 1V)  500 = 5V


//#define VOLTAGE_REFERENCE 3                 // used for measuring the voltage, easier to code if 3.3V is used (error is around 3.3%)
//#define VOLTAGE_REFERENCE 1024. / 330       // used for measuring the voltage, voltage will be as fixed point arithmetic (100 = 1V)  330 = 3.3V


// using internal refence for ADC is suggested but it needs to be done right. So by default this is not done!
//#define VOLTAGE_REFERENCE 9                 // used for measuring the voltage, easier to code if 1.1V internal reference is used (error is around 3.4%)
//#define VOLTAGE_REFERENCE 1024. / 110       // used for measuring the voltage, voltage will be as fixed point arithmetic (100 = 1V)  330 = 1.1V


// define ID's for START and FINISH controls. as 1-250 are for controls, available numbers are 253-255 and 0
#define START_CONTROL_ID 251
#define FINISH_CONTROL_ID 252


// define the function of the control
#define CONTROL 1                // works as a standard control
#define CONTROL_WITH_READOUT 2   // works as a control which has a readout of all previouos controls
#define READOUT 3                // works as a readout only
#define CLEAR 4                  // works as a clear





static RTC_DS3231 rtc;
static MFRC522 mfrc522(SS_PIN, RST_PIN);        // instatiate a MFRC522 reader object.
static MFRC522::MIFARE_Key key;                 //create a MIFARE_Key struct named 'key', which will hold the card information

static byte controlId;                         // current ID of this station (from now on called control)    
static byte readbackblock[18];                 //This array is used for reading out a block. The MIFARE_Read method requires a buffer that is at least 18 bytes to hold the 16 bytes of a block.
static long currentTime;                // currentTime, used for saving the time from RTC (without the need of continuos read in functions).

boolean useSerial = true;
byte controlFunction;

static SerialCommand  sCmd   = SerialCommand();       // The demo SerialCommand object
static CommandHandler sHand1 = CommandHandler(sCmd);  // the main command handler
static CommandHandler sHand2 = CommandHandler(sCmd);  // the sub command handler for SET command
static CommandHandler sHand3 = CommandHandler(sCmd);  // the sub command handler for GET command


/**
 * setup function of the Arduino toolchain
 * 
 */
void setup() {
      // if RTC cannot be started, then there is no RTC.
      if (! rtc.begin()) {
        if (useSerial) Serial.println(F("No RTC"));
        while (1);   // sketch halts in an endless loop
      }

      // if RTC is running, check the time. If the time differs more than 15 seconds off the computer clock, then it's set back to computer clock 
      DateTime now = rtc.now();
      DateTime compiled = DateTime(F(__DATE__), F(__TIME__));
      
      if (abs(now.unixtime() - compiled.unixtime()) > 15 ) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      }
      
        Serial.begin(9600);        // Initialize serial communications with the PC
        controlId=62;              // set ID of the control station. Any number between 1-250.      
        controlFunction = CONTROL; // set function as control

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


        SPI.begin();               // Init SPI bus

        mfrc522.PCD_Init();        // Init MFRC522 card (in case you wonder what PCD means: proximity coupling device)
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
  sHand2.setDefault(         unrecognized);    // Handler for command that isn't matched  (says "What?")
  
  sHand3.addCommand("TIME",  getTime);         // Get current time
  sHand3.addCommand("CTRL",  getControl);      // Get current control number
  sHand3.addCommand("MODE",  getMode);         // Get current mode
  sHand3.addCommand("VOLTAGE",  getVoltage);   // Get battery voltage
  sHand3.addCommand("VERSION",  getVersion);   // Get current firmware version
  sHand3.setDefault(         unrecognized);    // Handler for command that isn't matched  (says "What?")
  
  
  Serial.print(">");
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
 

        /*****************************************establishing contact with a tag/card**********************************************************************/
        
  	// Look for new cards (in case you wonder what PICC means: proximity integrated circuit card)
	if ( ! mfrc522.PICC_IsNewCardPresent()) {//if PICC_IsNewCardPresent returns 1, a new card has been found and we continue
		return;//if it did not find a new card is returns a '0' and we return to the start of the loop
	}

	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial()) {//if PICC_ReadCardSerial returns 1, the "uid" struct (see MFRC522.h lines 238-45)) contains the ID of the read card.
		return;//if it returns a '0' something went wrong and we return to the start of the loop
	}


// If the code comes to here, then a card is found and selected!


// DEBUG CODE
#ifdef DEBUG
         if (useSerial) Serial.println(F("card selected"));
#endif
// END DEBUG CODE

      // controlId 1-252 are controls.
      // 1-250 regular controls, 251 is start, 252 is finish
      // work as control should be set up in controlSetup
      if ( controlFunction == CONTROL || controlFunction == CONTROL_WITH_READOUT ) {
          timeToBleep = writeControl();
          if ( timeToBleep && useSerial ) {
                Serial.print(controlId);
                Serial.print(F(","));

                serialPrintUid();
                
                Serial.print(F(","));
                Serial.println(currentTime);

                if (controlFunction == CONTROL_WITH_READOUT) {
                   readOutAllControls();
                }
            }
      }
      if ( controlFunction == READOUT ) {
        if (  useSerial ) {
          Serial.print(F("Read card"));
          serialPrintUid();
          Serial.println("");
          timeToBleep = readOutAllControls();
        }
      }
      if ( controlFunction == CLEAR  ) {
           timeToBleep = clearCard();          
      }

      if (timeToBleep) { 
            bleep();
           
      }

     // check serial, if there is something new on connected link
     if (useSerial) {
        sCmd.readSerial();     // We don't do much, just process serial commands
     }
     
     sleep();
         
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


