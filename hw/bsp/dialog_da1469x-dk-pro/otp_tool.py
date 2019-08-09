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
import struct
import binascii
from typing import NamedTuple
from enum import Enum
import os


class Cmd(object):
    OTP_READ_KEY = 0
    OTP_WRITE_KEY = 1
    FLASH_READ = 2
    FLASH_WRITE = 3
    OTP_READ_CONFIG = 4
    OTP_APPEND_VALUE = 5
    OTP_INIT = 6


class cmd_no_payload(NamedTuple):
    som: int
    cmd: int


class cmd_read_key(NamedTuple):
    som: int
    cmd: int
    segment: int
    slot: int


class cmd_write_key(NamedTuple):
    som: int
    cmd: int
    segment: int
    slot: int


class cmd_append_value(NamedTuple):
    som: int
    cmd: int
    length: int


class cmd_response(NamedTuple):
    som: int
    cmd: int
    status: int
    length: int


class cmd_flash_op(NamedTuple):
    som: int
    cmd: int
    address: int
    length: int


def crc16(data: bytearray, offset, length):
    if data is None or offset < 0:
        return

    if offset > len(data) - 1 and offset + length > len(data):
        return 0

    crc = 0
    for i in range(length):
        crc ^= data[offset + i] << 8

        for j in range(8):
            if (crc & 0x8000) > 0:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
    return crc & 0xFFFF


def validate_slot_index(ctx, param, value):
    if value in range(8):
        return value
    else:
        raise click.BadParameter("Slot value has to be between 0 and 7")


@click.option('-i', '--index', required=True, type=int, help='key slot index',
              callback=validate_slot_index)
@click.option('-u', '--uart', required=True, help='uart port')
@click.option('-s', '--segment', type=click.Choice(['signature', 'data',
              'qspi']), help='OTP segment', required=True)
@click.command(help='Read a single key from OTP key segment')
def otp_read_key(index, segment, uart):
    seg_map = {'signature': 0, 'data': 1, 'qspi': 2}

    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=1,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    cmd = cmd_read_key(0xaa55aa55, Cmd.OTP_READ_KEY, seg_map[segment], index)
    data = struct.pack('IIII', *cmd)

    try:
        ser.write(data)
    except serial.SerialException:
        raise SystemExit("Failed to write to %s" % uart)

    data = ser.read(16)
    if len(data) != 16:
        raise SystemExit("Failed to receive response, exiting")

    response = cmd_response._make(struct.unpack_from('IIII', data))
    if response.status == 0:
        key = ser.read(32)
        print("key:" + key.hex())
    else:
        raise SystemExit("Error reading key with status %s" %
                         hex(response.status))


@click.argument('infile')
@click.option('-u', '--uart', required=True, help='uart port')
@click.option('-i', '--index', required=True, type=int, help='key slot index',
              callback=validate_slot_index)
@click.option('-s', '--segment', type=click.Choice(['signature', 'data',
              'qspi']), help='OTP segment', required=True,)
@click.command(help='Write a single key to OTP key segment')
def otp_write_key(infile, index, segment, uart):

    key = bytearray()
    try:
        with open(infile, "rb") as f:
            buf = f.read(32)
            key = struct.unpack('IIIIIIII', buf)
    except IOError:
        raise SystemExit("Failed to read key from file")

    seg_map = {'signature': 0, 'data': 1, 'qspi': 2}

    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=1,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    cmd = cmd_write_key(0xaa55aa55, Cmd.OTP_WRITE_KEY, seg_map[segment], index)
    # swap endianness of data to little endian

    msg = struct.pack('IIII', *cmd)
    key_bytes = struct.pack('>IIIIIIII', *key)
    msg += (key_bytes)
    msg += struct.pack('H', crc16(key_bytes, 0, 32))

    ser.write(msg)

    data = ser.read(16)
    if len(data) != 16:
        raise SystemExit("Failed to receive response, exiting")

    response = cmd_response._make(struct.unpack_from('IIII', data))
    if response.status == 0:
        print("Key successfully updated")
    else:
        raise SystemExit('Error writing key with status %s ' %
                         hex(response.status))


def generate_payload(data):
    msg = bytearray()

    for entry in data:
        msg += entry.to_bytes(4, 'little')

    return msg


@ click.argument('outfile')
@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Read data from OTP configuration script area')
def otp_read_config(uart, outfile):
    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=1,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    # length is unused, so set to 0
    cmd = cmd_append_value(0xaa55aa55, Cmd.OTP_READ_CONFIG, 0)
    data = struct.pack('III', *cmd)

    try:
        ser.write(data)
    except serial.SerialException:
        raise SystemExit("Failed to write to %s" % uart)

    data = ser.read(16)
    if len(data) != 16:
        raise SystemExit("Failed to receive response, exiting")

    response = cmd_response._make(struct.unpack_from('IIII', data))
    if response.status == 0:
        data = ser.read(response.length)
        if len(data) != response.length:
            raise SystemExit("Failed to receive data, exiting")

        print("Successfully read configuration script, writing to file: " + outfile)
    try:
        with open(outfile, "wb") as f:
            f.write(data)
    except IOError:
        raise SystemExit("Failed to write config data to file")


@click.argument('outfile')
@click.option('-a', '--address', type=str, required=True,
              help='flash address, hexadecimal')
@click.option('-l', '--length', type=int, required=True, help='length to read')
@click.option('-b', '--block-size', type=int, default=1024, help='block size')
@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Read from flash')
def flash_read(uart, length, outfile, address, block_size):
    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=1,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    try:
        f = open(outfile, "wb")
    except IOError:
        raise SystemExit("Failed to open output file")

    bytes_left = length
    offset = int(address, 16)
    while bytes_left > 0:
        if bytes_left > block_size:
            trans_length = block_size
        else:
            trans_length = bytes_left

        cmd = cmd_flash_op(0xaa55aa55, Cmd.FLASH_READ, offset, trans_length)
        data = struct.pack('IIII', *cmd)

        try:
            ser.write(data)
        except serial.SerialException:
            raise SystemExit("Failed to write cmd to %s" % uart)

        data = ser.read(16)
        if len(data) != 16:
            raise SystemExit("Failed to receive response, exiting")

        response = cmd_response._make(struct.unpack_from('IIII', data))
        if response.status == 0:
            data = ser.read(response.length)
            if len(data) != response.length:
                raise SystemExit("Failed to receive response, exiting")
            f.write(data)

        else:
            SystemExit("Error in read response, exiting")
            break

        bytes_left -= trans_length
        offset += trans_length

    print("Successfully read flash, wrote contents to " + outfile)


@click.argument('infile')
@click.option('-a', '--address', type=str, required=True, help='flash address')
@click.option('-b', '--block-size', type=int, default=1024, help='block size')
@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Write to flash')
def flash_write(uart, infile, address, block_size):
    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=1,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    try:
        f = open(infile, "rb")
    except IOError:
        raise SystemExit("Failed to open input file")

    length = os.path.getsize(infile)

    bytes_left = length
    offset = int(address, 16)
    while bytes_left > 0:
        if bytes_left > block_size:
            trans_length = block_size
        else:
            trans_length = bytes_left

        cmd = cmd_flash_op(0xaa55aa55, Cmd.FLASH_WRITE, offset,
                           trans_length + 2)
        data = struct.pack('IIII', *cmd)

        f_bytes = f.read(trans_length)
        data += f_bytes
        data += struct.pack('H', crc16(f_bytes, 0, trans_length))

        try:
            ser.write(data)
        except serial.SerialException:
            raise SystemExit("Failed to write cmd to %s" % uart)

        data = ser.read(16)
        if len(data) != 16:
            raise SystemExit("Failed to receive response, exiting")

        response = cmd_response._make(struct.unpack_from('IIII', data))
        if response.status == 0:
            data = ser.read(response.length)
            if len(data) != response.length:
                raise SystemExit("Failed to receive response, exiting")
            f.write(data)

        else:
            SystemExit("Error in read response, exiting")
            break

        bytes_left -= trans_length
        offset += trans_length

    print("Successfully wrote flash")


def send_otp_config_payload(uart, data):
    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=1,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    data_bytes = generate_payload(data)

    # length is unused, so set to 0
    cmd = cmd_append_value(0xaa55aa55, Cmd.OTP_APPEND_VALUE,
                           len(data_bytes) + 2)
    msg = struct.pack('III', *cmd)

    msg += data_bytes
    msg += struct.pack('H', crc16(data_bytes, 0, len(data_bytes)))

    try:
        ser.write(msg)
    except serial.SerialException:
        raise SystemExit("Failed to write to %s" % uart)

    data = ser.read(16)
    if len(data) != 16:
        raise SystemExit("Failed to receive response, exiting")

    response = cmd_response._make(struct.unpack_from('IIII', data))
    if response.status == 0:
        data = ser.read(response.length)
        if len(data) != response.length:
            raise SystemExit("Failed to receive data, exiting")


@click.command(help='Append register value to OTP configuration script')
@click.option('-a', '--address', type=str, required=True,
              help='register address in hexadecimal')
@click.option('-v', '--value', type=str, required=True,
              help='register value in hexadecimal')
@click.option('-u', '--uart', required=True, help='uart port')
def otp_append_register(uart, address, value):

    send_otp_config_payload(uart, [int(address, 16), int(value, 16)])


@click.option('-t', '--trim', type=str, required=True,
              multiple=True, help='trim value')
@click.option('-i', '--index', type=int, required=True,
              help='Trim value id')
@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Append trim value to OTP configuration script')
def otp_append_trim(uart, trim, index):

    data = []
    data.append(0x90000000 + (len(trim) << 8) + index)
    for entry in trim:
        data.append(int(entry, 16))

    send_otp_config_payload(uart, data)


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Disable development mode')
def disable_development_mode(uart):

    send_otp_config_payload(uart, [0x70000000])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Enable secure boot')
def enable_secure_boot(uart):

    send_otp_config_payload(uart, [0x500000cc, 0x1])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Write lock OTP QSPI key area')
def disable_qspi_key_write(uart):

    send_otp_config_payload(uart, [0x500000cc, 0x40])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Read lock OTP QSPI key area')
def disable_qspi_key_read(uart):

    send_otp_config_payload(uart, [0x500000cc, 0x80])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Write lock OTP user key area')
def disable_user_key_write(uart):

    send_otp_config_payload(uart, [0x500000cc, 0x10])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Read lock OTP user key area')
def disable_user_key_read(uart):

    send_otp_config_payload(uart, [0x500000cc, 0x20])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Write lock OTP signature key area')
def disable_signature_key_write(uart):

    send_otp_config_payload(uart, [0x500000cc, 0x8])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Disable CMAC debugger')
def disable_cmac_debugger(uart):

    send_otp_config_payload(uart, [0x500000cc, 0x4])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Disable SWD debugger')
def disable_swd_debugger(uart):

    send_otp_config_payload(uart, [0x500000cc, 0x2])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Close out OTP configuration script')
def close_config_script(uart):

    send_otp_config_payload(uart, [0x0])


@click.option('-u', '--uart', required=True, help='uart port')
@click.command(help='Initialize blank OTP Config script')
def init_config_script(uart):
    try:
        ser = serial.Serial(port=uart, baudrate=115200, timeout=1,
                            bytesize=8, stopbits=serial.STOPBITS_ONE)
    except serial.SerialException:
        raise SystemExit("Failed to open serial port")

    # length is unused, so set to 0
    cmd = cmd_append_value(0xaa55aa55, Cmd.OTP_INIT, 0)
    msg = struct.pack('III', *cmd)

    try:
        ser.write(msg)
    except serial.SerialException:
        raise SystemExit("Failed to write to %s" % uart)

    data = ser.read(16)
    if len(data) != 16:
        raise SystemExit("Failed to receive response, exiting")

    response = cmd_response._make(struct.unpack_from('IIII', data))
    if response.status != 0:
        raise SystemExit('Failed to initialize OTP with status %d'
                         % response.status)
    else:
        SystemExit("Successfully initialized blank OTP")

    print(response)

@click.group()
def cli():
    pass


cli.add_command(otp_read_config)
cli.add_command(otp_read_key)
cli.add_command(otp_write_key)
cli.add_command(flash_read)
cli.add_command(flash_write)
cli.add_command(otp_append_register)
cli.add_command(otp_append_trim)
cli.add_command(disable_development_mode)
cli.add_command(enable_secure_boot)
cli.add_command(disable_qspi_key_write)
cli.add_command(disable_qspi_key_read)
cli.add_command(disable_user_key_write)
cli.add_command(disable_user_key_read)
cli.add_command(disable_signature_key_write)
cli.add_command(disable_cmac_debugger)
cli.add_command(disable_swd_debugger)
cli.add_command(close_config_script)
cli.add_command(init_config_script)


if __name__ == '__main__':
    cli()
