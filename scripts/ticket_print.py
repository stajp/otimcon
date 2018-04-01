
# Uses LPR default printer for printing (presumably OTIMCON tickets).
# LPR is used for printing on linux and macOS.
# For Windows check startCommand below.
#
# Usage: ticket_print -p PORT -s speed
# eg. python ticket_print.py -p /dev/cu.myPort -s 9600
#
# use python ticker_print.py --help for more info
#

import glob
import getopt

# edit this to suit your OS, or use the "command" option when invoking this script
# linux and macOS  startCommand = 'lpr'
# windows          startCommand = 'print D:\\\MACHINE_NAME\PRINTER_NAME'
#                  (possible additional parameters needed, check number of '\')
startCommand = 'lpr'

def usage():
    print "Ticket printing for OTIMCON"
    print ""
    print "Usage:"
    print "  ticket_print -pPORT (-sSPEED) [-i] [-c'COMMAND']"
    print "  ticket_print --port=PORT (--speed=SPEED) [--increment] [--command='COMMAND']"
    print "  ticket_print -h | --help"
    print ""
    print "Options:"
    print "  -h --help       Show this screen"
    print "  -p --port       Serial port for OTIMCON"
    print "  -s --speed      Speed of serial port for OTIMCON, default 38400"
    print "  -i --increment  Save every ticket, with incremental filename"
    print "  -c --command    Command which starts the print, default 'lpr'"
    print ""
    print "Example:"
    print "  ticket_print -p/dev/cu.wchusbserial640 -s9600 -i"
    print "  ticket_print --port=COM4 --speed=38400 --command='print /D:\\\\\MACHINE\PRINTER"
    print ""
    print "Procedure:"
    print "  1. Connect OTIMCON station, look up the serial port."
    print "  2. Set receipt printer as default printer, or set the --command option."
    print "  3. Start this script. It will automatically convert OTIMCON station to PRINT mode!"
    print "  4. Use the script. Exit with CTRL-C."
    print ""
    print "Copyright (c) Stipe Predanic, given under GNU General Public License v3.0"

def inputParse():
    global startCommand
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hp:s:c:i", ["help", "port=", "speed=", "command=", "increment"])
    except getopt.GetoptError as err:
        # print help information and exit:
        print str(err)  # will print something like "option -a not recognized"
        usage()
        sys.exit(2)
    port = ''
    speed = '38400'
    increment = False
    for o, a in opts:
        if o in ("-p", "--port"):
            port = a
        elif o in ("-s", "--speed"):
            speed = a
        elif o in ("-i", "--increment"):
            increment = True
        elif o in ("-h", "--help"):
            usage()
            sys.exit()
        elif o in ("-c", "--command"):
            startCommand = a
        else:
            assert False, "unhandled option"
    return port, speed, increment

def convertToPrintMode(ser):
    print "\n!!! NOTICE: Converting the OTIMCON to PRINT mode !!!"
    print "Press CTRL-C in 3 seconds to stop this."
    print ""
    time.sleep(3)
    readData = ser.readline()
    if readData[0:11] == 'Starting...':
        readData = ser.readline()
        print "Connected..."
        if readData[0] == '>':
            ser.flush()
            ser.write(b"SET MODE PRINT\n")
            time.sleep(0.5)
            print "Setting mode PRINT..."
            readData = ser.readline()
            readData = ser.readline()
            if readData[0:10] == 'Mode:PRINT':
                readData = ser.readline() # catch the last ">"
                return True
            else:
                print "!!! Error: OTIMCON refuses PRINT mode!"
        else:
            print "!!! Error: OTIMCON doesn't behave as expected!"
    else:
        print "!!! Error: Cannot connect to OTIMCON. Check port & speed!"
    return False

if __name__ == '__main__':
    import sys, subprocess, serial, time

    port, speed, incrementFilename = inputParse()
    if port is '':
        print "\n!!! Error: Port must be defined!"
        print "Use ticket_print --help for usage.\n"
        sys.exit(2)
    try:
        ser = serial.Serial(port, speed, timeout=1)
    except Exception as err:
        print "\n!!! Error: Cannot connect to serial port " + port
        print str(err)
        print ""
        sys.exit(2)

    print "\nConnected to port " + port + " at " + speed + " baud."
    print "Printing using '" + startCommand +"' command."

    # convert OTIMCON to PRINT mode
    if convertToPrintMode(ser) is False:
        print "\n!!! Error: Cannot convert OTIMCON to PRINT mode."
        print "OTIMCON state is unknown, check the current OTIMCON mode manually.\n"
        sys.exit(2)

    print ""
    print "Ready, waiting for OTIMCON cards..."

    # everythin OK, loop forever
    ticketNo = 1
    newData = False
    while True:
        time.sleep(0.02)  # sleep added so there is always something is in serial buffer until printing is really needed
        readData = ser.readline()

        # if data is not empty, check if it's the first, if it is - open file, write it, otherwise just append
        if readData is not '':
            if newData is False:
                if incrementFilename is True:
                    print "Incrementing filename number.."
                    fileName = "temp_ticket_" + str(ticketNo)
                else:
                    fileName = "temp_ticket"
                fileOutput = open(fileName, "w")
            newData = True
            sys.stdout.write(readData)
            fileOutput.write(readData)
        else: # no new data in buffer, close the temporaty ticket file and print it
            if newData:
                fileOutput.close()
                print "File closed, printing the " + str(ticketNo) + ". ticket using the '" + startCommand + "' command."
                time.sleep(0.1)
                subprocess.call(startCommand + " " + fileName, shell=True)
                ticketNo += 1
                newData = False
