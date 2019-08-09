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


@click.argument('infile')
@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Close out OTP configuration script')
def load(infile, uart):
    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=10,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
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

#    if len(msg) == 0:
#        raise SystemExit("Read timed out, exiting")

    print("Please reset board to enter ROM uart recovery")

    while True:
        msg = ser.read(1)
        print(msg)
        if msg[0] == 0x2:
            print("Detected serial boot protocol")
            msg = bytes([0x1])
            msg += bytes([len(data) & 0xff])
            msg += bytes([len(data) >> 8])
            ser.write(msg)

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
