#!/usr/bin/env python3
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
import serial
import io
import click
import os
import os.path as path
from datetime import datetime


@click.argument('infile')
@click.option('-u', '--uart', required=True, help='uart port')
@click.option('-r', '--reset_script', required=False, help='Custom reset script to switch to serial load')
@click.command(help='Load the provided file using serial load protocol')
def load(infile, uart, reset_script):
    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=0.010,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    # drain serial port buffer
    try:
        while True:
            data = ser.read(1)
            if len(data) == 0:
                break
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    try:
        f = open(infile, "rb")
    except IOError:
        raise SystemExit("Failed to open input file")

    data = f.read()
    crc = 0
    for i in range(len(data)):
        crc = crc ^ data[i]

    # serial load protocol is the following:
    # - on reset, board sends 0x2 and listens for 100ms or so
    # - host sends 0x1 followed by length of file to transfer
    # - board responds with 0x6
    # - host sends file
    # - board responds with bytewise XOR of file
    # - If XOR matches, host sends 0x6 as final ack
    # - board boots image after receiving ACK from host

    som_detected = False
    reset_triggered = False
    prev = datetime.now()
    reset_delay_us = 250000

    if reset_script is None:
            print("Please reset board to enter ROM uart recovery")

    while True:
        elapsed = datetime.now() - prev
        if elapsed.seconds >= 15:
            raise SystemExit("Failed to receive SOM, aborting")
        if not som_detected and not reset_triggered:
            if reset_script and elapsed.microseconds >= reset_delay_us:
                print("Triggering SWD reset...")
                # Run in background
                os.system(reset_script + " &")
                reset_triggered = True

        msg = ser.read(1)
        if len(msg) > 0 and msg[0] == 0x2:
            print(msg)
            som_detected = True
            print("Detected serial boot protocol")
            msg = bytes([0x1])

            if len(data) > 65535:
                msg += bytes([0x0])
                msg += bytes([0x0])
                msg += bytes([len(data) & 0xff])
                msg += bytes([(len(data) >> 8) & 0xff])
                msg += bytes([len(data) >> 16])
            else:
                msg += bytes([len(data) & 0xff])
                msg += bytes([len(data) >> 8])

            ser.write(msg)
            ser.timeout = 10
            msg = ser.read()
            if len(msg) == 0:
                raise SystemExit("Failed to receive SOH ACK, aborting")

            if msg[0] != 0x6:
                raise SystemExit("Received invalid response, aborting")

            print("Loading application to RAM")
            try:
                ser.write(data)
            except serial.SerialException:
                raise SystemExit("Failed to write executable, aborting")

            msg = ser.read()
            if len(msg) == 0:
                raise SystemExit("Failed to receive XOR of bytes")

            if crc != msg[0]:
                raise SystemExit("Failed CRC check")

            msg = bytes([0x6])

            try:
                ser.write(msg)
            except serial.SerialException:
                raise SystemExit("Failed to write final ACK, aborting")
            print("Successfully loaded RAM, board will now boot executable")
            break
        else:
            continue


@click.group()
def cli():
    pass


cli.add_command(load)


if __name__ == '__main__':
    cli()
