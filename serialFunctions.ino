
void serialDateTime( DateTime t) {
   Serial.print(t.year(), DEC);
   Serial.print(F("/"));
   Serial.print(t.month(), DEC);
   Serial.print(F("/"));
   Serial.print(t.day(), DEC);
   Serial.print(F(" "));
   Serial.print(t.hour(), DEC);
   Serial.print(F(":"));
   Serial.print(t.minute(), DEC);
   Serial.print(F(":"));
   Serial.print(t.second(), DEC);
}

void serialPrintUid() {
       for (byte i = 0; i < mfrc522.uid.size; i++) {
                   Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
                   Serial.print(mfrc522.uid.uidByte[i], HEX);
                } 
}

void printCursor() {
   Serial.print(F(">"));
}

void setTime(SerialCommand *cmd) {
  char *arg = cmd->nextToken();
  
  boolean timeSet=true;
  
  if (arg != NULL) {    // As long as it existed, take it
       uint16_t year = 1000 * (arg[0] - '0') + 100 * (arg[1] - '0') + 10 * (arg[2] - '0') + arg[3] - '0';
       uint8_t month = 10 * (arg[4] - '0') + arg[5] - '0';
       uint8_t day = 10 * (arg[6] - '0') + arg[7] - '0';
       uint8_t hour = 10 * (arg[8] - '0') + arg[9] - '0';
       uint8_t minute = 10 * (arg[10] - '0') + arg[11] - '0';
       uint8_t second = 10 * (arg[12] - '0') + arg[13] - '0';

       rtc.adjust(DateTime( year, month, day, hour, minute, second));
  }
  else {
     timeSet=false;
  }

  // if the new mode is set, then return the mode, otherwise send to unrecognized command 
  if ( timeSet ) {
    getTime();
  }
  else {
    unrecognized(cmd); 
  }
}

void getTime() {
  Serial.print(F("Time:"));
  DateTime t = rtc.now();
  
  Serial.print(t.unixtime());
  Serial.print(F(","));
  serialDateTime(t);
  Serial.println(F(""));
  printCursor();
}


void getControl() {
  Serial.print(F("Control:"));
  Serial.println(controlId);
  printCursor();
}

void setControl(SerialCommand *cmd) {
  char *arg = cmd->nextToken();
  int newControlId=-1;
  
  boolean controlSet = true;
  
  if (arg != NULL) {
    newControlId = atoi(arg); 
    if (newControlId > 0 && newControlId<255) {
      controlId = newControlId;
    }  
    else {
      controlSet = false;
    }
  }
  else {
    controlSet = false;
  }
  
  if (  controlSet  ) {
    getControl();
  }
  else {
    unrecognized(cmd);
  }
}


void setMode(SerialCommand *cmd) {
  char *arg = cmd->nextToken();
  
  boolean modeSet=true;
  
  if (arg != NULL) {    // As long as it existed, take it
     if (strcmp(arg, "CONTROL") == 0){
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
            else  {
              modeSet=false;
            }
  }
  else {
     modeSet=false;
  }

  // if the new mode is set, then return the mode, otherwise send to unrecognized command 
  if ( modeSet ) {
    getMode();
  }
  else {
    unrecognized(cmd); 
  }
}

void getMode() {
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
 * function which serial outputs voltage
 * 
 * use the VOLTAGE_REFERENCE for the maximum value of 1024
 * 
 */
void getVoltage() {
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
  Serial.println((sum >> 3) / VOLTAGE_REFERENCE  ); 
  
  printCursor();
 
}

void getVersion() {
  Serial.println(F("Get Version"));
  Serial.println(F(SW_VERSION));
  printCursor();
}


void help(SerialCommand *cmd) {
  char *arg = cmd->nextToken();
  if (arg == NULL) {
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
      Serial.println(F("CONTROL number      : Set control ID as number (1-250) [special code for START/FINISH"));
      Serial.println(F("MODE type           : Set modes"));
    }
    else if (strcmp(arg, "GET") == 0) {
      Serial.println();
      Serial.println(F("Get options"));
      Serial.println(F("TIME                : Get time in format YYYY/MM/DD hh:mm:ss - year[Y], month[M], day[D], hour[h], minute[m], second[s]"));
      Serial.println(F("CONTROL             : Get control ID as number (1-250) [special code for START/FINISH]"));
      Serial.println(F("MODE                : Get mode status"));
      Serial.println(F("VERSION             : Get current firmware version"));
      Serial.println(F("VOLTAGE             : Get current battery voltage"));
    }
      else unrecognized(cmd);
  }
  printCursor();
}

// This gets set as the default handler, and gets called when no other command matches.
void unrecognized(SerialCommand *cmd) {
  Serial.print(F("Unknown Argument '"));
  Serial.print(cmd->lastToken());
  Serial.println(F("'"));
}
