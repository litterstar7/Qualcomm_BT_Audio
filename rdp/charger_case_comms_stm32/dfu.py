#
# \copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
#            All Rights Reserved.\n
#            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#\file
#\brief      Charger Case DFU script
#

import serial
import time
import argparse
from enum import Enum

global state
global state_time

class dfu_state(Enum):
    OFF = 0
    START = 1
    WAIT_FOR_READY = 2
    SEND_RECORD = 3
    WAIT_FOR_ACK = 4
    WAIT_FOR_COMPLETE = 5


def change_state(new_state):
    global state
    global state_time

    state = new_state
    state_time = time.time()

def main(argv = []):

    global state
    global state_time

    start_time = time.time()

    parser = argparse.ArgumentParser(description='Charger Case DFU')
    parser.add_argument("port", help = "COM port")
    parser.add_argument("file", help = "Filename")
    parser.add_argument("-n", default=False, action="store_true", help = "Spurious NACKs")
    parser.add_argument("-o", default=False, action="store_true", help = "Override compatibility check")
    parser.add_argument("-t", default=False, action="store_true", help = "Disable timeout")
    parser.add_argument("-s", default=115200, type=int, help = "Speed")    
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.s, timeout=0.005)

    if ser:

        f = open(args.file, "r")

        if f:
            all_lines = f.readlines()

            change_state(dfu_state.START)
            line_no = 0
            s3_count = 0
            nack_count = 0
            nr_count = 0

            while state != dfu_state.OFF:
                ser_line = ser.readline().decode()

                if (str.find(ser_line, 'DFU: ') != -1):
                    if (str.find(ser_line, 'ACK') == -1):
                        print('\r{}'.format(ser_line), end='')
                else:
                    ser_line = ''

                if (str.find(ser_line, 'Error') != -1):
                    change_state(dfu_state.OFF)
                elif (str.find(ser_line, '------') != -1):
                    change_state(dfu_state.OFF)

                if (state==dfu_state.START):
                    opt = 0x01

                    if (args.t):
                        opt += 0x04

                    if (args.n):
                        opt += 0x08

                    if (args.o):
                        opt += 0x10

                    ser.write(("\r\ndfu " + format(opt, 'x') + "\r\n").encode())
                    change_state(dfu_state.WAIT_FOR_READY)

                elif (state==dfu_state.WAIT_FOR_READY):
                    if (str.find(ser_line, 'Ready') != -1):

                        # Edit out the S3 records that we are not using this
                        # time around.

                        rlen = len(all_lines)
                        s3_count = int((rlen - 2) / 2)

                        if (str.find(ser_line, '(A)') != -1):
                            # Running from A, so we don't need the A records.
                            del all_lines[1 : s3_count + 1]
                        else:
                            # Running from B, so we don't need the B records.
                            del all_lines[s3_count + 1 : rlen - 1]

                        change_state(dfu_state.SEND_RECORD)

                elif (state==dfu_state.SEND_RECORD):
                    print('\r{}/{}'.format(line_no, s3_count + 1), end='')
                    ser.write(all_lines[line_no].encode())
                    ser.write(b'\r\n')
                    change_state(dfu_state.WAIT_FOR_ACK)

                elif (state==dfu_state.WAIT_FOR_ACK):
                    if (str.find(ser_line, 'NACK') != -1):
                        nack_count += 1
                        change_state(dfu_state.SEND_RECORD)
                    elif (str.find(ser_line, 'ACK') != -1):
                        if (str.find(all_lines[line_no], 'S7') != -1):
                            print('')
                            change_state(dfu_state.WAIT_FOR_COMPLETE)
                        else:
                            line_no = line_no + 1
                            change_state(dfu_state.SEND_RECORD)
                    elif ((time.time() - state_time) > 1):
                        nr_count += 1
                        change_state(dfu_state.SEND_RECORD)

                elif (state==dfu_state.WAIT_FOR_COMPLETE):
                    if (str.find(ser_line, 'Complete') != -1):
                        change_state(dfu_state.OFF)

            print('NACK: {} NR: {}'.format(nack_count, nr_count))

    print('Time taken = {} seconds'.format(int(time.time() - start_time)))

    return 0


if __name__ == "__main__":
    main()
