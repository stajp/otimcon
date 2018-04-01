
# Command line setup for OTIMCON

# Note: Does _NOT_ support all OTIMCON options!

import glob
import getopt

def usage():
    print "Command line setup for OTIMCON"
    print ""
    print "Usage:"
    print "  otimcon_setup -pPORT (-sSPEED) [-g] [-t]"
    print "  otimcon_setup --port=PORT (--speed=SPEED) [--get] [--settime] [--setOPTIONS=setValue]"
    print "  otimcon_setup -h | --help"
    print ""
    print "Options:"
    print "  -h --help       Show this screen"
    print "  -p --port       Serial port for OTIMCON"
    print "  -s --speed      Speed of serial port for OTIMCON, default 38400"
    print "  -g --get        Get all data about OTIMCON box"
    print "  -t --settime    Sets the current time taken from the PC"
    print "     --setmode    Sets the OTIMCON to mode (CLEAR|CONTROL|PRINT|READOUT)"
    print "     --setctrl    Sets the number of control (1-250|START|FINISH)"
    print "     --writeinfo  Sets the OTIMCON to WRITER mode and writes info to card"
    print ""
    print "Example:"
    print "  otimcon_setup -p/dev/cu.wchusbserial640 -s9600 -t --setmode=CONTROL --setctrl=START"
    print "  otimcon_setup --port=COM4 --speed=38400 --setmode=CLEAR"
    print "  otimcon_setup --port=/dev/dev/ttyUSB0 -s 9600 --settime --setmode=CONTROL --setctrl=45"
    print "  otimcon_setup -p /dev/dev/ttyUSB0 --speed=38400 --writeinfo='John Smith RED'"
    print "  otimcon_setup -p COM3 -s 38400 -g"
    print ""
    print "Procedure:"
    print "  1. Connect OTIMCON station, look up the serial port."
    print "  2. Start command line setup"
    print ""
    print "Notes:"
    print "  1. Modes CONTROL WITH PRINTOUT, FULL READOUT, FROZEN CONTROL are not supported through"
    print "     this software. Also WRITER mode where a control is written. Those can still be used"
    print "     through serial connection to OTIMCON."
    print "  2. Setting time is not necessary for every option, but is suggested to set time every"
    print "     time as possible."
    print "  3. Writing info leaves OTIMCON in WRITER mode."
    print "  4. Option 'get' will give every information, even those which are maybe not supported"
    print "     by hardware. It is not allowed to use get and set at the same time."
    print ""
    print "Copyright (c) Stipe Predanic, given under GNU General Public License v3.0"

def inputParse():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hgtp:s:", ["help", "port=", "speed=", "get", "settime", "setmode=", "setctrl=", "writeinfo="])
    except getopt.GetoptError as err:
        # print help information and exit:
        print str(err)  # will print something like "option -a not recognized"
        usage()
        sys.exit(2)
    port = ''
    speed = '38400'
    get = False
    setTime = False
    mode = ''
    ctrl = ''
    write = False
    writeInfo = ''
    for o, a in opts:
        if o in ("-p", "--port"):
            port = a
        elif o in ("-s", "--speed"):
            speed = a
        elif o in ("-g", "--get"):
            get = True
        elif o in ("-h", "--help"):
            usage()
            sys.exit()
        elif o in ("-t", "--settime"):
            setTime = True
        elif o in ("--setmode"):
            mode = a
        elif o in ("--setctrl"):
            ctrl = a
        elif o in ("--writeinfo"):
            write = True
            writeInfo = a
        else:
            assert False, "unhandled option"
    return port, speed, get, setTime, mode, ctrl, write, writeInfo

def wakeUp(ser):
    ser.write(b"\n")
    readData = ser.readline()
    counter = 0
    while counter < 6:
        readData = ser.readline()
        if readData[0:1] == '>':
            break;
        else:
            counter +=1;
        if counter == 3:
            ser.write(b"\n")
    counter = 0
    ser.write(b"\n")

    while counter < 6:
        readData = ser.readline()
        if readData[0:1] == '>':
            break;
        else:
            counter +=1;
        if counter == 3:
            ser.write(b"\n")

    time.sleep(1)
    ser.write(b"PING\n")
    time.sleep(0.5)
    readData = ser.readline()
    readData = ser.readline()

    if readData[0:4] == 'PONG':
        return True
    return False

def getMode(ser):
    print "Detecting current OTIMCON mode..."
    if wakeUp(ser) is True:
        ser.write(b"GET MODE\n")
        time.sleep(0.1)
        readData = ser.readline()
        readData = ser.readline()
        if readData[0:5] == 'Mode:':
            return readData[5:].rstrip()

def convertToMode(ser, mode):
    print "\n!!! NOTICE: Converting OTIMCON to " + mode + " mode !!!"
    print "Press CTRL-C in 3 seconds to stop this."
    print ""
    time.sleep(3)
    if wakeUp(ser) is True:
        readData = ser.readline()
        print "OTIMCON awake, connected..."
        if readData[0] == '>':
            ser.flush()
            ser.write(b"SET MODE " + mode + "\n")
            time.sleep(0.5)
            print "Setting mode " + mode + "..."
            readData = ser.readline()
            readData = ser.readline()
            expectedReponse='Mode:'+mode
            if readData[0:len(expectedReponse)] == expectedReponse:
                readData = ser.readline() # catch the last ">"
                return True
            else:
                print "!!! Error: OTIMCON refuses " + mode + " mode!"
        else:
            print "!!! Error: OTIMCON doesn't behave as expected!"
    else:
        print "!!! Error: Cannot connect to OTIMCON. Check port & speed!"

    return False

def convertToControlMode(ser, ctrl):
    currentMode = getMode(ser)
    if currentMode[0:len(mode)] != mode:
        result = convertToMode(ser,'CONTROL')
        if result is True:
            print "Success: OTIMCON in CONTROL mode"
    else:
        print "Success: OTIMCON was in CONTROL mode"
        result = True

    if ctrl == '':  # control number isn't necessary
        return True

    validCtrl = False
    if (ctrl == 'START' or ctrl == 'FINISH'):
        validCtrl = True
    elif (int(ctrl) > 0 and int(ctrl) < 256):
        validCtrl = True
    if (result is True and validCtrl is True):
        ser.write(b"SET CTRL " + ctrl + "\n")
        time.sleep(0.5)
        print "Setting control to " + ctrl + "..."
        readData = ser.readline()
        readData = ser.readline()
        expectedReponse='Control:'+ctrl
        print expectedReponse
        print readData
        if readData[0:len(expectedReponse)] == expectedReponse:
            readData = ser.readline() # catch the last ">"
            return True
        else:
            print "!!! Error: OTIMCON refuses to set control to " + ctrl + "!"
    else:
        print "!!! Error: OTIMCON isn't in CONTROL mode or invalid control designation"
    return False


def writeInfoToCard(ser, info):
    if (convertToMode(ser, 'WRITER') is True):
        ser.write(b"WRITE INFO " + info + "\n")
        time.sleep(0.1)
        readData = ser.readline()
        if readData[0:30] == "Waiting for card to write info":
           print "Put chip at the OTIMCON station"
           counter = 0
           while counter < 5:
               time.sleep(1)
               readData = ser.readline()
               if readData[0:24] == "WRITING INFO TO CARD: OK":
                   print "Succes: Info written to card."
                   return True
               else:
                   counter += 1
           print "!!! Error: info not written"
        else:
            print "!!! Error: OTIMCON refuses to write info."
    else:
        print "!!! Error: OTIMCON not set to WRITER mode."
    return False

def getAllData(ser):
    print "Trying to connect to OTIMCON..."
    if wakeUp(ser) is True:
        print "OTIMCON awake, connected..."
        print "\nOTIMCON DATA:\n---------------"
        ser.write(b"GET VERSION\n")
        time.sleep(0.1)
        readData = ser.readline()
        readData = ser.readline()
        print readData.rstrip()

        ser.write(b"GET TIME\n")
        time.sleep(0.1)
        readData = ser.readline()
        readData = ser.readline()
        print readData.rstrip()

        ser.write(b"GET MODE\n")
        time.sleep(0.1)
        readData = ser.readline()
        readData = ser.readline()
        print readData.rstrip()

        if readData[5:].rstrip() == 'CONTROL':
            ser.write(b"GET CTRL\n")
            time.sleep(0.1)
            readData = ser.readline()
            readData = ser.readline()
            print readData.rstrip()

        ser.write(b"GET VOLTAGE\n")
        time.sleep(0.1)
        readData = ser.readline()
        readData = ser.readline()
        print readData.rstrip()




if __name__ == '__main__':
    import sys, subprocess, serial, time, datetime

    port, speed, get, setTime, mode, ctrl, write, writeInfo = inputParse()
    if port is '':
        print "\n!!! Error: Port must be defined!"
        print "Use otimcon_setup --help for usage.\n"
        sys.exit(2)
    try:
        ser = serial.Serial(port, speed, timeout=1)
    except Exception as err:
        print "\n!!! Error: Cannot connect to serial port " + port
        print str(err)
        print ""
        sys.exit(2)

    print "\nConnected to port " + port + " at " + speed + " baud."

    if get is True:
        print "GET DATA" #TODO: remove
        getAllData(ser)
    else:
        if write is True:
            if writeInfoToCard(ser, writeInfo) is False:
                print "\n!!! Error: Cannot convert OTIMCON to WRITE mode and write info."
                print "OTIMCON state is unknown, check the current OTIMCON mode manually.\n"
                sys.exit(2)
            else:
                print "Success: OTIMCON wrote info on card."
        elif (mode[0:5] == 'PRINT') or (mode[0:5] == 'CLEAR') or (mode[0:7] == 'READOUT'):
            currentMode = getMode(ser)
            if currentMode[0:len(mode)] == mode:
                print "Success: OTIMCON was already in " + mode + " mode."
            else:
                print "Current mode is " + currentMode + ", will be changed into " + mode + "."
                if convertToMode(ser,mode) is False:
                    print "!!! Error: Cannot convert OTIMCON to " + mode + " mode."
                    print "OTIMCON state is unknown, check the current OTIMCON mode manually.\n"
                    sys.exit(2)
                else:
                    print "Success: OTIMCON is in " + mode + " mode."
        elif mode == 'CONTROL':
            print "SET CONTROL" #TODO: remove
            if convertToControlMode(ser, ctrl) is False:
                print "!!! Error: Error while setting OTIMCON to CONTROL mode or the control designation."
                print "OTIMCON state is unknown, check the current OTIMCON mode manually.\n"
                sys.exit(2)
            else:
                if ctrl != '':
                    print "Success: OTIMCON in CONTROL mode with designation " + ctrl + "."
                else:
                    print "Success: OTIMCON in CONTROL mode, use GET option for desigation."
        if setTime is True:
            print 'Setting time..'
            if wakeUp(ser) is True:
                print "OTIMCON awake, trying to set time..."
                now = datetime.datetime.now()
                ser.write(b"SET TIME " + now.strftime("%Y%m%d%H%M%S")+"\n")
                time.sleep(0.1)
                readData = ser.readline()
                readData = ser.readline()
                if readData[0:5] == 'Time:':
                    print "Time set, OTIMCON "+ readData
                else:
                    print "!!! Error: Time not set!"
            else:
                print "!!! Error: Not connected to set time."
    print ""
