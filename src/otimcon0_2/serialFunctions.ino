#define CMP_STRINGS(X, Y) (strcmp_P(X, PSTR(Y))) 

/**
   function sends date time out through the serial

   @param DateTime t  which is sent out
*/
void serialDateTime (DateTime t) {
  serialDate(t);
  Serial.print(F(" "));
  serialTime(t);
}

void serialDate(DateTime t) {
  Serial.print(t.year(), DEC);
  Serial.print(F("/"));
  Serial.print(t.month() < 10 ? "0" : "");
  Serial.print(t.month(), DEC);
  Serial.print(F("/"));
  Serial.print(t.day() < 10 ? "0" : "");
  Serial.print(t.day(), DEC);
}

void serialTime(DateTime t) {
  Serial.print(t.hour() < 10 ? "0" : "");
  Serial.print(t.hour(), DEC);
  Serial.print(F(":"));
  Serial.print(t.minute() < 10 ? "0" : "");
  Serial.print(t.minute(), DEC);
  Serial.print(F(":"));
  Serial.print(t.second() < 10 ? "0" : "");
  Serial.print(t.second(), DEC);
}

/**
   function prints out the UID of the current RFID card

*/
void serialPrintUid() {
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
}

/**
   function prints >
*/
void printCursor() {
  Serial.print(F(">"));

  // reset the sleep counter
  shallowSleepCounter = 0;
}

/**
   function to set the time to DS3231

   This function is used by SerialCommand library
*/
void s_setTime(SerialCommand *cmd) {
  char *arg = cmd->nextToken();


  boolean timeSet = true; // say that time is set

  if (arg != NULL) {    // As long as a parameter exists, take it
    powerToRTC(RTC_POWER_ON);   // RTC and EEPROM are on the same board

    // there is no additional checkup, thread cerefully
    uint16_t year = 1000 * (arg[0] - '0') + 100 * (arg[1] - '0') + 10 * (arg[2] - '0') + arg[3] - '0';
    uint8_t month = 10 * (arg[4] - '0') + arg[5] - '0';
    uint8_t day = 10 * (arg[6] - '0') + arg[7] - '0';
    uint8_t hour = 10 * (arg[8] - '0') + arg[9] - '0';
    uint8_t minute = 10 * (arg[10] - '0') + arg[11] - '0';
    uint8_t second = 10 * (arg[12] - '0') + arg[13] - '0';
    // sometimes I2C gets stuck. This is a destucking mechanism :D
    while (! rtc.begin()) {
      delay(1);
    }
    DateTime t = DateTime( year, month, day, hour, minute, second); 

    // set the currentTime none the less 
    currentTime = t.unixtime();
    
    // set the time in RTC chip only if time is not frozen.
    if ( controlFunction != FROZEN_CONTROL ) {
      rtc.adjust(t);
    }
    else {
      // but if it is frozen, save it in EEPROM, in case of a  restart
      EEPROM.put(EEPROM_ADDRESS_FROZENTIME, currentTime );
    }
  }
  else {     // if there is no argument, then time is not set
    timeSet = false;
  }

  // if the new time is set, then return the time, otherwise send to unrecognized command
  if ( timeSet ) {
    s_getTime();
  }
  else {
    s_unrecognized(cmd);
  }
  powerToRTC(RTC_POWER_OFF);   // RTC and EEPROM are on the same board
}

/**
   returns time in UNIX epoch format (seconds from 1970/1/1 00:00:00) and in human readable form
*/
void s_getTime() {
  getTime();
  
  Serial.println();
  Serial.print(F("Time:"));
  
  DateTime t = DateTime((uint32_t) currentTime); 
  //rtc.now();

  //Serial.print(t.unixtime());
  Serial.print(currentTime);
  Serial.print(F(","));
  serialDateTime(t);
  Serial.println(F(""));
  printCursor();
}


/**
    function to set the control identifing number

   This function is used by SerialCommand library
*/
void s_setControl(SerialCommand *cmd) {
  const char *arg = cmd->nextToken();

  // set the controlId to -1, just to check if read fails.
  int newControlId = -1;

  boolean controlSet = true;  // imagine that the controlid will be properly set

  if (arg != NULL) { // if there is a parameter, use it.
     if (CMP_STRINGS(arg, "START") == 0) {
        newControlId = START_CONTROL_ID;
     }
     else if (CMP_STRINGS(arg, "FINISH") == 0) {
        newControlId = FINISH_CONTROL_ID;
     } 
     else { 
         newControlId = atoi(arg);  // convert to number
     }
     if (newControlId > 0 && newControlId < 255) { // check for validity
       controlId = newControlId;
    }
    else {
      controlSet = false;
    }
  }
  else {
    controlSet = false;
  }

  // if controlId is set, then update the EEPROM, and return the new control id.
  if (  controlSet  ) {
    EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, controlId);              // set ID of the control station. Any number between 1-254.
    s_getControl();
  }
  else {
    s_unrecognized(cmd);
  }
}

/**
   returns current control number
*/
void s_getControl() {
  // write warning if control number is asked while not in control mode
  if (controlFunction != CONTROL && 
      controlFunction != CONTROL_WITH_READOUT &&
      controlFunction != FROZEN_CONTROL) {
      Serial.println(F("WARNING: Not in control mode."));
      s_getMode();       
   }
   else {
     Serial.println();
     Serial.print(F("Control:"));
     switch (controlId) {
        case START_CONTROL_ID:  Serial.print(F("START - ")); 
                                Serial.println(controlId);
                                break;
        case FINISH_CONTROL_ID: Serial.print(F("FINISH - ")); 
                                Serial.println(controlId);
                                break;
                               
        default              :  Serial.println(controlId);
                                break;                       
        }
   }
  printCursor();
}


/**
    function to set the control mode

   This function is used by SerialCommand library
*/
void s_setMode(SerialCommand *cmd) {
  char *arg = cmd->nextToken();

  boolean modeSet = true;

  if (arg != NULL) {    // As long as parameter exists, take it
    if (CMP_STRINGS(arg, "CONTROL") == 0) { // set the wanted control mode
      controlFunction = CONTROL;
    }
    else if (CMP_STRINGS(arg, "CONTROL_WITH_READOUT") == 0) {
      controlFunction = CONTROL_WITH_READOUT;
    }
    else if (CMP_STRINGS(arg, "READOUT") == 0) {
      controlFunction = READOUT;
    }
    else if (CMP_STRINGS(arg, "CLEAR") == 0) {
      controlFunction = CLEAR;
    }
    else if (CMP_STRINGS(arg, "PRINT") == 0) {
      controlFunction = PRINT;
      arg = cmd->nextToken();
      if (arg !=NULL && CMP_STRINGS(arg, "POS") == 0) {
         printEscPos = true;
         EEPROM.write(EEPROM_ADDRESS_OPTIONS, EEPROM.read(EEPROM_ADDRESS_OPTIONS) | 0x1 );
      }
      else {
        printEscPos = false;
        EEPROM.write(EEPROM_ADDRESS_OPTIONS, EEPROM.read(EEPROM_ADDRESS_OPTIONS) & 0xFE );      
      }
    }
    else if (CMP_STRINGS(arg, "WRITER") == 0) {
      controlFunction = WRITER;
    }
    else if (CMP_STRINGS(arg, "FULL_READOUT") == 0) {
      controlFunction = FULL_READOUT;
    }
    else if (CMP_STRINGS(arg, "FROZEN_CONTROL") == 0) {
      // forcebly change into control, to update the currentTime variable to latest RTC value
      controlFunction = CONTROL;
      getTime();

      // after setting the value, set the mode to FROZEN_CONTROL
      controlFunction = FROZEN_CONTROL;
    }
    else  {    // if everything fails, we're here!
      modeSet = false;
    }
  }
  else {
    modeSet = false;
  }

  // if the new mode is set, then return the mode, otherwise send to unrecognized command
  if ( modeSet ) {
    EEPROM.update(EEPROM_ADDRESS_MODE, controlFunction); // set function as control
    s_getMode();
  }
  else {
    s_unrecognized(cmd);
  }
}

/**
   returns current mode
*/
void s_getMode() {
  Serial.println();
  Serial.print(F("Mode:"));
  switch (controlFunction) {
    case CONTROL:               Serial.println(F("CONTROL")); break;
    case CONTROL_WITH_READOUT:  Serial.println(F("CONTROL_WITH_READOUT")); break;
    case READOUT:               Serial.println(F("READOUT")); break;
    case CLEAR:                 Serial.println(F("CLEAR")); break;
    case PRINT:                 Serial.print(F("PRINT "));
                                if (printEscPos) { Serial.println(F("POS")); }
                                else { Serial.println(); } 
                                break;
    case WRITER:                Serial.println(F("WRITER")); break;
    case FULL_READOUT:          Serial.println(F("FULL_READOUT")); break;
    case FROZEN_CONTROL:        Serial.println(F("FROZEN_CONTROL")); break;
    default: Serial.println(F("UNKNOWN"));
  }
  printCursor();
}

/**
   function which serial outputs voltage connected to analog port 2

   use the VOLTAGE_REFERENCE for the maximum value of 1024

*/
void s_getVoltage() {
  ADCSRA =  bit (ADEN);

  analogReference (ANALOG_REFERENCE);
  Serial.println();
  Serial.print(F("Voltage:"));
  uint16_t sum;

  //take 8 samples, so we can easily divide the voltage
  for (byte sample_count = 0; sample_count < 8; sample_count++ ) {
    sum += analogRead(A2);
    sample_count++;
    delay(10);
  }

  // divide sum by 8 and then by VOLTAGE_REFERENCE (the difference 1024/500 or whatever was maximum voltage)
  // 8 is because of 8 samples
  // additional to get right voltage. A fast version would be to set Voltage_reference to 2 (it would be quicker although less accurate)
  Serial.println((int) (sum >> 3) / VOLTAGE_REFERENCE  );

  printCursor();

}

/**
   function outputs version over the serial
*/
void s_getVersion() {
  Serial.println();
  Serial.print(F("Version:"));
  Serial.println(F(SW_VERSION));
  printCursor();
}

/**
   this functions resets the pointer from where the external memory starts to record data about cards


   this isn't necessary as there is an rollover for locationOnExternalEEPROM.
   but it's nice to start from 0.

*/
void s_setResetBackup() {
  powerToRTC(RTC_POWER_ON);   // RTC and EEPROM are on the same board
  // sometimes I2C gets stuck. This is a destucking mechanism :D
  while (! rtc.begin()) {
    delay(1);
  }
  Serial.println();
  Serial.println(F("Backup pointer resetted to 0!"));
  locationOnExternalEEPROM = 0;
  EEPROM.update(EEPROM_ADDRESS_24C32_LOCATION, (short int) 0);

#ifndef USE_EEPROM_BACKUP
  Serial.println(F("Although - there is no support for backup!"));
#endif

  printCursor();
  powerToRTC(RTC_POWER_OFF);   // RTC and EEPROM are on the same board

}

/**
   this function outputs to serial the data from the whole external EEPROM memory, even the parts which were not used.
*/
void s_getBackup() {
#ifdef USE_EEPROM_BACKUP
  byte dataFromEEPROM[8]; // 8 bytes will be read from the EEPROM to this array
  byte uid[4];            // after reading data will be separated into UID and time
  byte timeArray[4];

  powerToRTC(RTC_POWER_ON);   // RTC and EEPROM are on the same board

  Serial.println();
  Serial.println(F("Backup:"));

  // sometimes I2C gets stuck. This is a destucking mechanism :D
  while (! rtc.begin()) {
    delay(1);
  }
  // go though whole external memory (in chunks of 8 bytes)
  for (short int i = 0; i < EXTERNAL_EEPROM_SIZE; i += 8) {
    // read 8 bytes from the "i" position
    i2c_eeprom_read_bytes(EEEPROM_I2C_ADDRESS, i, dataFromEEPROM, 8);

    Serial.print(F("^"));

    // separate the time and uid
    memcpy(timeArray, dataFromEEPROM, 4);
    memcpy(uid, dataFromEEPROM + 4, 4);

    // print uid
    for (byte j = 0; j < 4; j++) {
      Serial.print(uid[j] < 0x10 ? " 0" : " ");
      Serial.print(uid[j], HEX);
    }
    Serial.print(F(","));

    // print time in UNIX epoch timestamp and human form
    long int timestamp =  (((long int)timeArray[3]) << 24) | (((long int) timeArray[2]) << 16) | (((long int)timeArray[1]) << 8) | (timeArray[0]);
    DateTime t(timestamp);
    Serial.print(timestamp);
    Serial.print(F(","));
    serialDateTime(t);
    Serial.println(F("#"));
  }
  Serial.println(F("$"));
#else
  Serial.println(F("No support for backup!"));
#endif
  printCursor();
  powerToRTC(RTC_POWER_OFF);   // RTC and EEPROM are on the same board

}

/**
   function to print help to screen


   Used by SerialCommand library

*/
void s_help(SerialCommand *cmd) {
  char *arg = cmd->nextToken();
  if (arg == NULL) {
    Serial.println();
    Serial.println(F("SET *: Options for setting data "));
    Serial.println(F("GET *: Options for getting data"));
    if (controlFunction == WRITER) {
      Serial.println(F("WRITE *: Options for writing to card"));
    }
    Serial.println();
    Serial.println(F("* Use ? OPTION (eg ? SET) to get detailed information"));

  }
  else {
    if ( CMP_STRINGS(arg, "SET") == 0) {
      Serial.println();
      Serial.println(F("Set options"));
      Serial.println(F("TIME YYYYMMDDhhmmss : Set time in year[Y], month[M], day[D], hour[h], minute[m], second[s] format"));
      Serial.println(F("CTRL number         : Set control ID as number (1-250) [special code for START/FINISH"));
      Serial.println(F("MODE type           : Set modes"));
      Serial.println(F("RESET_BACKUP        : Reset pointer of internal memory backup"));

    }
    else if (CMP_STRINGS(arg, "GET") == 0) {
      Serial.println();
      Serial.println(F("Get options"));
      Serial.println(F("TIME                : Get time in format YYYY/MM/DD hh:mm:ss - year[Y], month[M], day[D], hour[h], minute[m], second[s]"));
      Serial.println(F("CTRL                : Get control ID as number (1-250) [special code for START/FINISH]"));
      Serial.println(F("MODE                : Get mode status"));
      Serial.println(F("BACKUP              : Get backup of whole card memory"));
      Serial.println(F("VERSION             : Get current firmware version"));
      Serial.println(F("VOLTAGE             : Get current battery voltage"));
    }
    else if (CMP_STRINGS(arg, "WRITE") == 0) {
      Serial.println();
      Serial.println(F("Write options"));
      Serial.println(F("INFO arbitrary text           : Set info line in the card"));
      Serial.println(F("CONTROL number YYYYMMDDhhmmss : Writes a control at the end of current controls"));
    }
    else s_unrecognized(cmd);
  }
  printCursor();

}

/**
   function to write info about the card owner or racing number or whatever.

   written in blocks 5 and 6. Maximum of 30 characters.

*/
void s_writeInfo(SerialCommand *cmd) {

  if (controlFunction != WRITER) {
     Serial.println(F("WARNING: Not in WRITER mode, so nothing will be done..."));
     return;
  }
    byte i, j;

    char *arg = cmd->nextToken();
    
    byte infoLength = 0;
    while (arg != NULL && infoLength < 28 ) {
     
      while (*arg && infoLength < 28){
        writerData[infoLength++]=*(arg);
        arg++;
      }
      writerData[infoLength++] = ' ';
      arg = cmd->nextToken();
    }

   writerJob = 1;
   Serial.println(F("Waiting for card to write info..."));
}

void s_writeControl(SerialCommand *cmd) {
  if (controlFunction != WRITER) {
       Serial.println(F("WARNING: Not in WRITER mode, so nothing will be done..."));
     return;
  }
 
    byte controlToSet = -1;
    char *arg = cmd->nextToken();
  
    if (arg != NULL) { // if there is a parameter, use it.
      controlToSet = atoi(arg);  // convert to number
      if (!(controlToSet > 0 && controlToSet < 255)) { // check for validity
        Serial.println(F("ERROR: Problem with the id of the control..."));
        return;
      }
      writerData[0] = controlToSet;
    }
    arg = cmd->nextToken(); 
    long int controlTime = -1;
    if (arg != NULL) {    // As long as a parameter exists, take it
      uint16_t year = 1000 * (arg[0] - '0') + 100 * (arg[1] - '0') + 10 * (arg[2] - '0') + arg[3] - '0';
      uint8_t month = 10 * (arg[4] - '0') + arg[5] - '0';
      uint8_t day = 10 * (arg[6] - '0') + arg[7] - '0';
      uint8_t hour = 10 * (arg[8] - '0') + arg[9] - '0';
      uint8_t minute = 10 * (arg[10] - '0') + arg[11] - '0';
      uint8_t second = 10 * (arg[12] - '0') + arg[13] - '0';

      
      DateTime controlDateTime (year, month, day, hour, minute, second);
      controlTime = controlDateTime.unixtime();
      writerData[1] = (controlTime >> 24) & 0x000000FF;
      writerData[2] = (controlTime >> 16) & 0x000000FF;
      writerData[3] = (controlTime >> 8)  & 0x000000FF;
      writerData[4] = controlTime & 0x000000FF;

#if DEBUG > 1  
      Serial.print(F("year:"));
      Serial.println(year);
      Serial.print(F("month:"));
      Serial.println(month);
      Serial.print(F("day:"));
      Serial.println(day);
      Serial.print(F("hour:"));
      Serial.println(hour);
      Serial.print(F("minute:"));
      Serial.println(minute);
      Serial.print(F("second:"));
      Serial.println(second);
      Serial.print(F("writerData1:"));
      Serial.println(writerData[1]);
      Serial.print(F("writerData2:"));
      Serial.println(writerData[2]);
      Serial.print(F("writerData3:"));
      Serial.println(writerData[3]);
      Serial.print(F("writerData4:"));
      Serial.println(writerData[4]);


#endif            
      
      Serial.print(F("Waiting for card to write control"));
      Serial.print(controlToSet);
      Serial.print(F(" with time "));
      Serial.print(arg);
      Serial.print(F(" [timestamp: ")); 
      Serial.print(controlTime);
      Serial.println(F("]..."));
      writerJob = 2;
    }
    else {
       Serial.println(F("ERROR: Problem with the time..."));
      return;
    }
}



/**
   function to just send PONG after receiving PING

   Should be used as keepalive.
*/
void s_pong(){
   Serial.println();
   Serial.println(F("PONG"));
   printCursor();
}


/**
   This gets set as the default handler, and gets called when no other command matches.

   Used by SerialCommand library
*/
void s_unrecognized(SerialCommand *cmd) {
  Serial.println();
  Serial.print(F("Unknown Argument '"));
  Serial.print(cmd->lastToken());
  Serial.println(F("'"));
  printCursor();
}
