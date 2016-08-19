
/**
 * function sends date time out through the serial
 * 
 * @param DateTime t  which is sent out
 */
void serialDateTime( DateTime t) {
   Serial.print(t.year(), DEC);
   Serial.print(F("/"));
   Serial.print(t.month()<10 ? "0" : "");
   Serial.print(t.month(), DEC);
   Serial.print(F("/"));
   Serial.print(t.day()<10 ? "0" : "");
   Serial.print(t.day(), DEC);
   Serial.print(F(" "));
   Serial.print(t.hour()<10 ? "0" : "");
   Serial.print(t.hour(), DEC);
   Serial.print(F(":"));
   Serial.print(t.minute()<10 ? "0" : "");
   Serial.print(t.minute(), DEC);
   Serial.print(F(":"));
   Serial.print(t.second()<10 ? "0" : "");
   Serial.print(t.second(), DEC);
}

/**
 * function prints out the UID of the current RFID card
 * 
 */
void serialPrintUid() {
       for (byte i = 0; i < mfrc522.uid.size; i++) {
                   Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
                   Serial.print(mfrc522.uid.uidByte[i], HEX);
                } 
}

/**
 * function prints >
 */
void printCursor() {
   Serial.print(F(">"));

    // reset the sleep counter
    sleepCounter = 0;
}

/**
 * function to set the time to DS3231
 * 
 * This function is used by SerialCommand library
 */
void setTime(SerialCommand *cmd) {
  char *arg = cmd->nextToken();


  boolean timeSet=true;  // say that time is set
  
  if (arg != NULL) {    // As long as a parameter exists, take it
    
       // there is no additional checkup, thread cerefully 
       uint16_t year = 1000 * (arg[0] - '0') + 100 * (arg[1] - '0') + 10 * (arg[2] - '0') + arg[3] - '0';
       uint8_t month = 10 * (arg[4] - '0') + arg[5] - '0';
       uint8_t day = 10 * (arg[6] - '0') + arg[7] - '0';
       uint8_t hour = 10 * (arg[8] - '0') + arg[9] - '0';
       uint8_t minute = 10 * (arg[10] - '0') + arg[11] - '0';
       uint8_t second = 10 * (arg[12] - '0') + arg[13] - '0';

       rtc.adjust(DateTime( year, month, day, hour, minute, second));
  }
  else {     // if there is no argument, then time is not set
     timeSet=false;
  }

  // if the new time is set, then return the time, otherwise send to unrecognized command 
  if ( timeSet ) {
    getTime();
  }
  else {
    unrecognized(cmd); 
  }
}

/**
 * returns time in UNIX epoch format (seconds from 1970/1/1 00:00:00) and in human readable form
 */
void getTime() {
  Serial.println();
  Serial.print(F("Time:"));
  DateTime t = rtc.now();
  
  Serial.print(t.unixtime());
  Serial.print(F(","));
  serialDateTime(t);
  Serial.println(F(""));
  printCursor();
}


/**
  * function to set the control identifing number
 * 
 * This function is used by SerialCommand library
 */
void setControl(SerialCommand *cmd) {
  char *arg = cmd->nextToken();

  // set the controlId to -1, just to check if read fails.
  int newControlId=-1;
  
  boolean controlSet = true;  // imagine that the controlid will be properly set
  
  if (arg != NULL) { // if there is a parameter, use it.
    newControlId = atoi(arg);  // convert to number
    if (newControlId > 0 && newControlId<255) {   // check for validity
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
    EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, controlId);              // set ID of the control station. Any number between 1-250.      
    getControl();
  }
  else {
    unrecognized(cmd);
  }
}

/**
 * returns current control number
 */
void getControl() {
  Serial.println();
  Serial.print(F("Control:"));
  Serial.println(controlId);
  printCursor();
}


/**
  * function to set the control mode
 * 
 * This function is used by SerialCommand library
 */
void setMode(SerialCommand *cmd) {
  char *arg = cmd->nextToken();
  
  boolean modeSet=true;
  
  if (arg != NULL) {    // As long as parameter exists, take it
     if (strcmp(arg, "CONTROL") == 0){  // set the wanted control mode
       controlFunction = CONTROL; 
     } 
     else 
        if (strcmp(arg, "CONTROL_WITH_READOUT") == 0){
          controlFunction = CONTROL_WITH_READOUT;
        }
        else 
          if (strcmp(arg, "READOUT") == 0){
            controlFunction = READOUT;
          }
          else 
            if (strcmp(arg, "CLEAR") == 0){
              controlFunction = CLEAR;  
            }
            else  {    // if everything fails, we're here!
              modeSet=false;
            }
  }
  else {
     modeSet=false;
  }

  // if the new mode is set, then return the mode, otherwise send to unrecognized command 
  if ( modeSet ) {
    EEPROM.update(EEPROM_ADDRESS_MODE, controlFunction); // set function as control
    getMode();
  }
  else {
    unrecognized(cmd); 
  }
}

/**
 * returns current mode
 */
void getMode() {
  Serial.println();
  Serial.print(F("Mode:"));
  switch (controlFunction) {
      case CONTROL:  Serial.println(F("CONTROL")); break;
      case CONTROL_WITH_READOUT:  Serial.println(F("CONTROL_WITH_READOUT")); break;
      case READOUT:  Serial.println(F("READOUT")); break;
      case CLEAR:  Serial.println(F("CLEAR")); break;
      default: Serial.println(F("UNKNOWN"));
  }
  printCursor();
}

/**
 * function which serial outputs voltage connected to analog port 2
 * 
 * use the VOLTAGE_REFERENCE for the maximum value of 1024
 * 
 */
void getVoltage() {
 ADCSRA =  bit (ADEN); 
 
  analogReference (ANALOG_REFERENCE);
  
  Serial.println(F("Voltage:"));
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
 * function outputs version over the serial
 */
void getVersion() {
  Serial.println();
  Serial.println(F("Version:"));
  Serial.println(F(SW_VERSION));
  printCursor();
}

/**
 * this functions resets the pointer from where the external memory starts to record data about cards
 * 
 * 
 * this isn't necessary as there is an rollover for locationOnExternalEEPROM.
 * but it's nice to start from 0.  
 *
 */
void setResetBackup() {
  Serial.println();
  Serial.println(F("Backup pointer resetted to 0!"));
  locationOnExternalEEPROM = 0;
  EEPROM.update(EEPROM_ADDRESS_24C32_LOCATION,(short int) 0);

#ifndef USE_EEPROM_BACKUP  
  Serial.println(F("Although - there is no support for backup!"));
#endif

  printCursor();  
}

/**
 * this function outputs to serial the data from the whole external EEPROM memory, even the parts which were not used.
 */
void getBackup() {
#ifdef USE_EEPROM_BACKUP  
  byte dataFromEEPROM[8]; // 8 bytes will be read from the EEPROM to this array
  byte uid[4];            // after reading data will be separated into UID and time
  byte timeArray[4];
  
  Serial.println();
  Serial.println(F("Backup:"));

  // go though whole external memory (in chunks of 8 bytes)
  for (short int i=0; i<EXTERNAL_EEPROM_SIZE; i+=8) {
      // read 8 bytes from the "i" position
      i2c_eeprom_read_bytes(EEEPROM_I2C_ADDRESS, i, dataFromEEPROM, 8);
      
      Serial.print(F("^"));

      // separate the time and uid
      memcpy(timeArray, dataFromEEPROM, 4);
      memcpy(uid, dataFromEEPROM+4, 4);

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
  
}

/**
 * function to print help to screen
 * 
 *  
 * Used by SerialCommand library
 *
 */
void help(SerialCommand *cmd) {
  char *arg = cmd->nextToken();
  if (arg == NULL) {
    Serial.println();
    Serial.println(F("SET *: Options for setting data "));
    Serial.println(F("GET *: Options for getting data"));
    Serial.println();
    Serial.println(F("* Use ? OPTION (eg ? SET) to get detailed information"));
   
  }
  else {
    if ( strcmp(arg, "SET") == 0) {
      Serial.println();
      Serial.println(F("Set options"));
      Serial.println(F("TIME YYYYMMDDhhmmss : Set time in year[Y], month[M], day[D], hour[h], minute[m], second[s] format"));
      Serial.println(F("CTRL number         : Set control ID as number (1-250) [special code for START/FINISH"));
      Serial.println(F("MODE type           : Set modes"));
      Serial.println(F("RESET_BACKUP        : Reset pointer of internal memory backup"));
      
    }
    else if (strcmp(arg, "GET") == 0) {
      Serial.println();
      Serial.println(F("Get options"));
      Serial.println(F("TIME                : Get time in format YYYY/MM/DD hh:mm:ss - year[Y], month[M], day[D], hour[h], minute[m], second[s]"));
      Serial.println(F("CTRL                : Get control ID as number (1-250) [special code for START/FINISH]"));
      Serial.println(F("MODE                : Get mode status"));
      Serial.println(F("BACKUP              : Get backup of whole card memory"));
      Serial.println(F("VERSION             : Get current firmware version"));
      Serial.println(F("VOLTAGE             : Get current battery voltage"));
    }
      else unrecognized(cmd);
  }
  printCursor();
  
}

/**
 * This gets set as the default handler, and gets called when no other command matches.
 *
 * Used by SerialCommand library
  */
void unrecognized(SerialCommand *cmd) {
  Serial.println();
  Serial.print(F("Unknown Argument '"));
  Serial.print(cmd->lastToken());
  Serial.println(F("'"));
  printCursor();
}
