
# Uses LPR default printer for printing (presumably OTIMCON tickets).
# LPR is used for printing on linux and macOS.
# For Windows check subprocess.call() on line 55
#
# Usage: ticket_print -p PORT -s speed
# eg. python ticket_print.py -p /dev/cu.myPort -s 9600
#


import glob

def getopts(argv):
    opts = {}  # Empty dictionary to store key-value pairs.
    while argv:  # While there are arguments left to parse...
        if argv[0][0] == '-':  # Found a "-name value" pair.
            opts[argv[0]] = argv[1]  # Add key and value to the dictionary.
        argv = argv[1:]  # Reduce the argument list by copying it starting from index 1.
    return opts

if __name__ == '__main__':
    from sys import argv
    from subprocess import Popen
    import subprocess
    import serial
    import time

    myargs = getopts(argv)
    if '-p' in myargs:
        port = (myargs['-p'])
    else:
        port = '/dev/ttyS1'
    if '-s' in myargs:
        speed = (myargs['-s'])
    else:
        speed = '9600'

    print port
    print speed
    ser = serial.Serial(port, speed, timeout=1)

    file_object = open("temp_ticket", "w")

    newData = False
    while True:
        time.sleep(0.05)  # sleep added so there is always something is in serial buffer until printing is really needed
        readData = ser.readline()
        if readData is not '':
            newData = True
            print readData
            file_object.write(readData)
        else:
            if newData:
                file_object.close()
                print "File closed, printing..."
                time.sleep(0.5)
                subprocess.call("lpr temp_ticket", shell=True)
                file_object = open("temp_ticket", "w")
                newData = False
