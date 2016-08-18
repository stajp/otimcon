

// this readblock works only if number of blocks is under 256
byte readBlock(byte blockNumber, byte arrayAddress[]) 
{
  int largestModulo4Number=blockNumber/4*4;
  int trailerBlock=largestModulo4Number+3;//determine trailer block for the sector

  /*****************************************authentication of the desired block for access***********************************************************/
  byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  //byte PCD_Authenticate(byte command, byte blockAddr, MIFARE_Key *key, Uid *uid);
  //this method is used to authenticate a certain block for writing or reading
  //command: See enumerations above -> PICC_CMD_MF_AUTH_KEY_A  = 0x60 (=1100000),    // this command performs authentication with Key A
  //blockAddr is the number of the block from 0 to 15.
  //MIFARE_Key *key is a pointer to the MIFARE_Key struct defined above, this struct needs to be defined for each block. New cards have all A/B= FF FF FF FF FF FF
  //Uid *uid is a pointer to the UID struct that contains the user ID of the card.
  if (status != MFRC522::STATUS_OK) {
#if DEBUG > 2   
         Serial.print(F("PCD_Authenticate() failed (read): "));
         Serial.println(mfrc522.GetStatusCodeName(status));
#endif         
         return 3;//return "3" as error message
  }
  //it appears the authentication needs to be made before every block read/write within a specific sector.
  //If a different sector is being authenticated access to the previous one is lost.


  /*****************************************reading a block***********************************************************/
        
  byte buffersize = 18;//we need to define a variable with the read buffer size, since the MIFARE_Read method below needs a pointer to the variable that contains the size... 
  status = mfrc522.MIFARE_Read(blockNumber, arrayAddress, &buffersize);//&buffersize is a pointer to the buffersize variable; MIFARE_Read requires a pointer instead of just a number
  if (status != MFRC522::STATUS_OK) {
#if DEBUG > 3 
          Serial.print(F("MIFARE_read() failed: "));
          Serial.println(mfrc522.GetStatusCodeName(status));
#endif         
          return 4;//return "4" as error message
  }
#if DEBUG > 3 
  Serial.println(F("block was read"));
#endif
  return 1;
}



/**
 * CRC-8
 * 
 * based on the CRC8 formulas by Dallas/Maxim
 * code released under the therms of the GNU GPL 3.0 license
 * 
 * @param data  array to calculate CRC
 * @param len   length of the array 
 * 
 * @return crc  CRC of the array
 */
byte CRC8(const byte *data, byte len) {
  byte crc = 0x00;
  while (len--) {
    byte extract = *data++;
    for (byte tempI = 8; tempI; tempI--) {
      byte sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }
  return crc;
}


/**
 * write data about this control to a location on the card
 * 
 * 
 * @param location    where to write. should be a number in the range [0,125] 
 * @param id          id of the control
 * @param timestamp   timestamp to write to control
 */
boolean writeDataToLocation(byte location, byte id, long int timestamp) {

  byte writingInfo;                                  // used to get error value from the writeBlock function
  
  // convert timestamp to 4 separate bytes
  static byte timestampArray[4] = {0};
  
  timestampArray[0] = (byte) (timestamp & 0xFF);       // set the lowest 8 bits of timestamp
  timestampArray[1] = (byte) ((timestamp >> 8) & 0xFF);
  timestampArray[2] = (byte) ((timestamp >> 16) & 0xFF);
  timestampArray[3] = (byte) ((timestamp >> 24) & 0xFF);  // set the highest 8 bits of timestamp

  // if card is full (and somehow it got past the writeControl() then send serial info about it, and return 0 as write was not successfull.
  if (location > 125) { 
    if ( useSerial ) {
       Serial.println(F("Error: card full - "));
       Serial.print(id);
       Serial.print(F(","));
      serialPrintUid();
         
    }
    return 0;
  }
  // send data over serial only if serial is ON
    // DEBUG DATA FOR EASIER READING
#if DEBUG > 1    
      Serial.println(F("Writing location, id, timestamp"));
   // END DEBUG DATA
    Serial.print(location);
    Serial.print(F(","));
    Serial.print(id);
    Serial.print(F(","));
    Serial.println(timestamp);
#endif

  
/* write data to right place */

  
  byte blockNumber = 8+location/3+location/9;  // calculate the block number -> this jumps over the section trailer blocks
  byte locationInBlock = 5*(location%3);       // calculate the location inside one block - there are 3 locations (each is 5 bytes) in one 16 byte block -> bytes 0, 5 or 10. 
 
  readBlock(blockNumber, readbackblock);      //read the whole block back


   // write data for the location: first byte = id of the control; next 4 bytes = timestamp
   readbackblock[locationInBlock] = id;
   readbackblock[locationInBlock+1] = timestampArray[0];
   readbackblock[locationInBlock+2] = timestampArray[1];
   readbackblock[locationInBlock+3] = timestampArray[2];
   readbackblock[locationInBlock+4] = timestampArray[3];

   // last, 16th byte is the CRC for all the bytes in that block.
   readbackblock[15] = CRC8(readbackblock,15);
 
   writingInfo = writeBlock(blockNumber,readbackblock);     // write block back to the card, info should be 1 (meaning success)

   if (writingInfo == 1) {
     // write data to infoBlock (block 4)
     readBlock(4, readbackblock);               //read the block back
     readbackblock[1] = location;               // set the location of the last written control
     readbackblock[5] = id;                     // set the ID of the last written control
     readbackblock[6] = timestampArray[0];       // set the timestamp of the last written control
     readbackblock[7] = timestampArray[1];
     readbackblock[8] = timestampArray[2];
     readbackblock[9] = timestampArray[3]; 
     readbackblock[15] = CRC8(readbackblock,15);  // set the CRC
     writingInfo = writeBlock(4,readbackblock);               // write it to infoBlock (block 4)
     
     if (writingInfo == 1) {                      // if written propely (1 means success, 2-4 are errors), try to readout and check for success
        readBlock(blockNumber, readbackblock);      //read the whole location block back and check it!
        if ( readbackblock[locationInBlock] == id &&
             readbackblock[locationInBlock+1] == timestampArray[0] && 
             readbackblock[locationInBlock+2] == timestampArray[1] &&
             readbackblock[locationInBlock+3] == timestampArray[2] &&
             readbackblock[locationInBlock+4] == timestampArray[3] && 
             readbackblock[15] == CRC8(readbackblock,15)) {
              
                // if we're here, then the read of the location block was OK. Read the info block (block 4)
                 readBlock(4, readbackblock);               //read the block back
                 if ( readbackblock[1] == location && readbackblock[5] == id && 
                      readbackblock[6] == timestampArray[0] && 
                      readbackblock[7] == timestampArray[1] &&
                      readbackblock[8] == timestampArray[2] && 
                      readbackblock[9] == timestampArray[3] &&  
                      readbackblock[15] == CRC8(readbackblock,15)) {
                        
                          return true;                 // return 1 as writing was successfull!
                          
                      } else return false;   
             } else return false;
     } else return false;
   } else return false;                                 // return 0 as writing wasn't successfull
}

/**
 * Writes a whole block on the card
 * 
 * Be careful while using, as there are 3 usable blocks and then the trailer block for each sector. There is a safety for trailer blocks!
 * 
 * 
 * 
 * @param blockNumber   the number of the wanted block!
 * @param arrayAddress  array which has to be written. It's always 16 bytes in size!
 * 
 * @return  information about errors or success  
 *          1  success
 *          2  tried to write to a trailer block
 *          3  card authentication failed (trailer block keys are wrong)
 *          4  write failed 
 *          
 */

    // warning, a lot of DEBUG data in here
byte writeBlock(byte blockNumber, byte arrayAddress[]) 
{
  //this makes sure that we only write into data blocks. Every 4th block is a trailer block for the access/security info.
  int largestModulo4Number=blockNumber/4*4;
  int trailerBlock=largestModulo4Number+3;//determine trailer block for the sector
  
  if (blockNumber > 2 && (blockNumber+1)%4 == 0) //block number is a trailer block (modulo 4); quit and send error code 2
    {
#if DEBUG > 4      
      Serial.print(blockNumber);
      Serial.println(F(" is a trailer block:"));
#endif      
      return 2;
    }
#if DEBUG > 4    
  Serial.print(blockNumber);
  Serial.println(F(" is a data block:"));
#endif
  
  /*****************************************authentication of the desired block for access***********************************************************/
  byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  //byte PCD_Authenticate(byte command, byte blockAddr, MIFARE_Key *key, Uid *uid);
  //this method is used to authenticate a certain block for writing or reading
  //command: See enumerations above -> PICC_CMD_MF_AUTH_KEY_A	= 0x60 (=1100000),		// this command performs authentication with Key A
  //blockAddr is the number of the block from 0 to 15.
  //MIFARE_Key *key is a pointer to the MIFARE_Key struct defined above, this struct needs to be defined for each block. New cards have all A/B= FF FF FF FF FF FF
  //Uid *uid is a pointer to the UID struct that contains the user ID of the card.
  if (status != MFRC522::STATUS_OK) {
#if DEBUG > 3    
         Serial.print(F("PCD_Authenticate() failed: "));
         Serial.println(mfrc522.GetStatusCodeName(status));
#endif         
         return 3;//return "3" as error message
  }
  
  //it appears the authentication needs to be made before every block read/write within a specific sector.
  //If a different sector is being authenticated access to the previous one is lost.


  /*****************************************writing the block***********************************************************/
  status = mfrc522.MIFARE_Write(blockNumber, arrayAddress, 16);//valueBlockA is the block number, MIFARE_Write(block number (0-15), byte array containing 16 values, number of bytes in block (=16))
  //status = mfrc522.MIFARE_Write(9, value1Block, 16);
  if (status != MFRC522::STATUS_OK) {
#if DEBUG > 3     
           Serial.print(F("MIFARE_Write() failed: "));
           Serial.println(mfrc522.GetStatusCodeName(status));
#endif           
           return 4;//return "4" as error message
  }

#if DEBUG > 3   
  Serial.println(F("block was written"));
#endif
  
  return 1;

}


// USE_PIEZO    USE_LED
void bleep() {
  
  // do this 3 times: turn on LED, make a 2.5kHz tone for 50ms, turn off LED, make a 3.5kHz tone for 50ms
  // sound on for 300ms, LED on for 150ms
  
  for (int i=0; i<3; i++) {
    
#ifdef USE_LED
    digitalWrite(FEEDBACK_LED, HIGH);
#endif
#ifdef USE_PIEZO
    tone(FEEDBACK_PIEZO, 2500, 50);    
#endif
#ifdef USE_LED
    digitalWrite(FEEDBACK_LED, LOW);
#endif
#ifdef USE_PIEZO
    tone(FEEDBACK_PIEZO, 3500, 50);
#endif
  }
  
}
void sleep() {
#if DEBUG > 3
  Serial.print(F("Sleeping"));
#endif
  unsigned long previousMillis = millis();
 // while ( millis() - previousMillis < 100 );
  
}

boolean writeControl() {
        byte location;                      // location on a card where the last data is written
        byte lastWrittenId;                 // last ID of the control writen on the card  
        long int lastTime;                      // time when the last control was written on a card
        boolean writingDone;                       // the bolean used to check if writing to card is properly done

       
  
          readBlock(4, readbackblock);                        //read block 4 which has all the basic data. This array will be used (and reused) to detect several necessary information 

     
         if ( readbackblock[0] != 1 ) {                       // read the first byte which houses the check if the card is prepared for OTICON. If not, bail out.
            if (useSerial) Serial.println(F("Error: Card not prepared"));
            
            return false;   // card is not initialized for use in OTICON
         }


         location      = readbackblock[1];   // read location of the last written control
         lastWrittenId = readbackblock[5];   // check what was the last written control

         lastTime      = (((long int)readbackblock[9]) << 24) | (((long int) readbackblock[8]) << 16) | (((long int)readbackblock[7]) << 8) | ((long int)readbackblock[6]);// check when was the last control written

       
         // get current time from the RTC
         currentTime=  rtc.now().unixtime(); 

#if DEBUG > 1
         Serial.println("Info block:");
         Serial.print("Location:");
         Serial.println(location);
         Serial.print("Last Id:");
         Serial.println(lastWrittenId);
         Serial.print("Current time:");
         Serial.println(currentTime);
         Serial.print("Read time:");
         Serial.println((long int) lastTime); 
        
#endif
         
         // check if it's the same control, and if the time between now and the last writing is less than xy seconds (defined in LAST_TIME_FROM_WRITING, by default 30 seconds)
         // if it is, then just bail out. Otherwise, try to write the data to the card.
         if ( lastWrittenId == controlId  &&  currentTime - lastTime <= LAST_TIME_FROM_WRITING) { 
            return true; 
         }

         // if card is full, don't write, but don't bleep either, so return false!
         if ( location >= 125 ) {
            return false;
         }


        
      
         // if writing is necessary then write to the next location. 
          writingDone = false;  // presume that writing will fail
          
          int counter = 0;     // additional counter for several tries before giving up
          do {
              // code could by smaller by reusing the controlId and current time as global variables, but this is a cleaner solution.
              // function writeDataToLocation returns 1 if successfull,  0 otherwise
              writingDone = writeDataToLocation(location+1+counter, controlId, currentTime);// code could by smaller by reusing the controlId and current time as global variables, but this is a cleaner solution.
              
              // don't try to write forever, stop after 4 tries, each time try to write to the next location (if the previous ones don't work!)
              counter++;
               
            } while ( !writingDone && counter < 4 );   // if writing fails try to write to card 4 times. The number is chosen as there are 3 locations in a block, so this will always try the next block!


        // usually the writing will succeed in the first try, this is just precoution.
         
         
      return writingDone;
      
}

boolean readOutAllControls() {
        byte location;                      // location on a card where the last data is written. Number cannot be bigger than 255.
    
 
  
       readBlock(4, readbackblock);                        //read block 4 which has all the basic data. This array will be used (and reused) to detect several necessary information 

     
        if ( readbackblock[0] != 1 ) {                       // read the first byte which houses the check if the card is prepared for OTICON. If not, bail out.
            if (useSerial) {
              Serial.println(F("Error: Card not prepared"));
            }
            return false;   // card is not initialized for use in OTICON
         }

        location      = readbackblock[1];   // read location of the last written control
        byte startLocation = location;           // know from where the list started
        
        // if location is 0, then the card is empty, nothing to readback.
        if (location == 0 ) return true;
        
        // now it will go back in memory, reading blocks, and retrieving locations until happens one of the following:
        // 1) comes to location 0
        // 2) finds a start location (control ID  defined by START_CONTROL_ID)
        // 3) finds a finish location (control ID defined by FINISH_CONTROL_ID)
        boolean stillSearching = true;
        
        byte oldBlockNumber = -1;   // set the inital block to -1, as we won't reread block unless necessary.
        
        while ( stillSearching ) {
           
           byte blockNumber = 8+location/3+location/9;  // calculate the block number -> this jumps over the section trailer blocks
          
           // if this block isn't the same as for previous location, then read the new block. Otherwise the block is already in memory.
           if (oldBlockNumber != blockNumber) {
              readBlock(blockNumber, readbackblock);      //read the whole block back

              // if CRC of the block is invalid, then jump over the rest of the loop, as this block is obviuosly compromised
              if (readbackblock[15] != CRC8(readbackblock,15)) 
                continue;

              // read the block, CRC is good => set the oldBlock number to current bloy, to stop rereading of blocks.   
              oldBlockNumber = blockNumber;
           }

           byte locationInBlock = 5*(location%3);       // calculate the location inside one block - there are 3 locations (each is 5 bytes) in one 16 byte block -> bytes 0, 5 or 10. 

           // will print output if the control on current location (which changes inside the loop) isn't a FINISH control or if it is a finish control but it's the last control in the list
           if ( readbackblock[locationInBlock] != FINISH_CONTROL_ID || ( readbackblock[locationInBlock] == FINISH_CONTROL_ID && location == startLocation)) {
              Serial.print( readbackblock[locationInBlock] );  // print the ID of the control
              Serial.print(F(","));
    
    
              // calculate timestamp from the data of the card
              long int timestamp =  (((long int)readbackblock[locationInBlock+4]) << 24) | (((long int) readbackblock[locationInBlock+3]) << 16) | (((long int)readbackblock[locationInBlock+2]) << 8) | (readbackblock[locationInBlock+1]);
              DateTime t(timestamp);
              Serial.print(timestamp);
              Serial.print(F(","));
              serialDateTime(t);
              Serial.println(F("#"));
          }
          else {
              // this means that the control found was a finish control (but not as the last control on the card
              stillSearching = false;
          }
          
          // if the Serial.printed control was a start control or at location 0, then stop searching,
          if ( readbackblock[locationInBlock] == START_CONTROL_ID || location == 0 ) {
             stillSearching = false;
          } 

          // location goes down by 1, the while loop will stop if the variable stillSearching becomes false;
          location--;
        }         

        // if everything was outputted then return to the main loop.
        return true;
}


boolean clearCard() {

   
    //  although it would be easier to write empty data to each location, this is the optimal way:
    //
    //  1. This will create an empty readblock with correct CRC.
    //  2. Then it will save that same block over all the data location
    //  3. Then it will read the same blocks to check that everything is overwritten
    //  4. Will prepare the byte for OTICON, overwrite the first location with 0, and empty data for the first control


    // DOING #1: empty readblock with correct CRC
    // create a readblock which is full of 0xFF
    for (byte i=0; i<15; i++) {
      readbackblock[i]=0xFF;
    }
    byte tempCRC = CRC8(readbackblock,15);
    readbackblock[15]= tempCRC; // set the CRC correctly

    // DOING #2: saving the block
    // go through all the blocks (the last one is 63, but we know that's a sector trailer and it cannot be overwritten
    for (byte blockNumber = 8; blockNumber < 63; blockNumber++) {

        // jump over sector trailers
        if ((blockNumber + 1) %4 == 0) {
          continue;
        }
        
        // writeBlock should return 1 if successfull, and 2-4 if there is an error
        // - and if there is an error, jump out with false!
        if (writeBlock( blockNumber, readbackblock) != 1 ) 
          return false ;

    }

    // DOING #3: checking if everything was written OK
    // go through all the blocks, and check if all the fields are 0xFF and CRC8 is correct
     for (byte blockNumber = 8; blockNumber < 63; blockNumber++) {

        // jump over sector trailers
        if ((blockNumber + 1) %4 == 0) {
          continue;
        }
        
         if (readBlock(blockNumber, readbackblock) == 1) {    //read the whole block back and check the first 15 bytes for 0xFF and the last one for CRC8
            for (byte i=0; i<15; i++) {
              if (readbackblock[i] != 0xFF) 
                return false;
            }
            if (readbackblock[15] != tempCRC) 
              return false;
         } 
         else                      // if reading of the block fails, return false! 
          return false;
    }
    
   // DOING #4: set info block
        
         //read block 4 which has all the basic data. This array will be used (and reused) to detect several necessary information.
         //read should return 1 as sucessfull, if it's not, then return false!
         if (readBlock(4, readbackblock) != 1) 
            return false;                    
     
         readbackblock[0] = 1;                           // prepare card for OTICON. This is a safety in the system so other cards aren't overwritten by mistake on "normal" controls.

         readbackblock[1] = 0;                          // set location of first writing to 0
         
         readbackblock[5] = 0xFF;                       // set last control to 0xFF
         
         // erase the last control time
         readbackblock[6] = 0xFF;
         readbackblock[7] = 0xFF;
         readbackblock[8] = 0xFF;
         readbackblock[9] = 0xFF;

        readbackblock[15] == CRC8(readbackblock,15);

         // write it back to block 4
         if (writeBlock( 4, readbackblock) != 1 ) 
            return false ;
    
     

       // if we came here, then everything is OK, return true!
       return true;
     

    
}

/**
 * write a byte somewhere on the card
 * 
 * THIS FUNCTION IS NOT USED! IT'S NOT EVEN COMPLETE (ON MOST PLACES CRC8 IS NEEDED) - IT'S HERE JUST IF SOMEONE IS INTERESTED HOW TO DO IT
 * @param byteAddress  where to write
 * @param data         a byte to write
 */
void writeByte(int byteAddress, byte data) {
  
  int blockNumber = byteAddress >> 4  ; // byteAddress / 16
  int byteInBlock = byteAddress - (blockNumber << 4);  

// DEBUG DATA
#if DEBUG > 4  
  Serial.print(F("BlockNumber:"));
  Serial.println(blockNumber);
  Serial.print(F("Byte inside:"));
  Serial.println(byteInBlock);
#endif
// END DEBUG DATA
   
  byte dataArray[18];
  byte buffersize = 18;//we need to define a variable with the read buffer size, since the MIFARE_Read method below needs a pointer to the variable that contains the size... 
  readBlock(blockNumber, dataArray);//read the block back
  dataArray[byteInBlock] = data;
 
  writeBlock(blockNumber,dataArray); 
}
