#! /usr/bin/env python3
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

import re
import click
import io
import os.path
import struct
import binascii
import time
from typing import NamedTuple
import base64

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization

from cryptography.hazmat.primitives import hashes

# Add in path to mcuboot.  This is required to pull in keys module from imgtool
import sys
sys.path.append(os.path.join(os.getcwd(), "repos", "mcuboot", "scripts",
                "imgtool"))
import keys as keys


class product_header(NamedTuple):
    ident: bytes
    active_addr: int
    update_addr: int
    cmda_reg: int
    cmdb_reg: int
    flash_ident: bytes
    flash_length: int
    cmd_seq: bytes


class key_revocation_entry(NamedTuple):
    key_type: int
    key_index: int


class revocation_section(NamedTuple):
    ident: bytes
    length: int


class device_administration_section(NamedTuple):
    ident: bytes
    length: int


class signature_section(NamedTuple):
    sig_idx: int
    enc_idx: int
    nonce: bytes
    ident: bytes
    length: int
    signature: bytes


class security_section(NamedTuple):
    ident: bytes
    length: int


class fw_header(NamedTuple):
    ident: bytes
    length: int
    crc: int
    version: bytes
    timestamp: int
    ivt_offset: int


def crc16(data: bytearray, offset, length):
    if data is None or offset < 0 or offset > len(data) - 1 and offset + length > len(data):
        return 0
    crc = 0xFFFF
    for i in range(length):
        crc ^= data[offset + i] << 8

        for j in range(8):
            if (crc & 0x8000) > 0:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
    return crc & 0xFFFF


class da1469x_fw_image(object):
    PROD_ID = b'Pp'
    FW_ID = b'Qq'
    FLASH_SECT_ID = b'\xaa\x11'
    SEC_SECT_ID = b'\xaa\x22'
    SIG_SECT_ID = b'\xaa\x33'
    DEV_ADM_SECT_ID = b'\xaa\x44'
    REV_ENTRY_SECT_ID = b'\xaa\x55'

    def __init__(self, path, encrypt_file, sig_file, sig_slot, decrypt_slot,
                 revoke, version):
        header = product_header(self.PROD_ID, 0x2000, 0x2000, 0xa8a500eb, 0x66,
                                self.FLASH_SECT_ID, 3, b'\x01\x40\07')
        self.img = struct.pack('<2sIIII2sH3s', *header)
        self.img += struct.pack('<H', crc16(self.img, 0, len(self.img)))
        self.img += b'\xff' * (4096*2 - len(self.img))

        self.img_size = os.path.getsize(path)
        self.aes_key = None
        self.sig_key = None
        self.sig_slot = None
        self.decrypt_slot = None
        self.revocation_list = revoke

        if version:
            self.version = version.encode()
        else:
            self.version = b'\x00' * 16

        with open(path, "rb") as binary_file:
            # Read the whole file at once
            self.data = binary_file.read()

        if encrypt_file is not None:
            with open(encrypt_file, "rb") as aes_file:
                # Read the whole file at once
                self.aes_key = aes_file.read()
                self.aes_key = base64.b64decode(self.aes_key)

        if sig_file is not None:
            self.sig_key = keys.load(sig_file)

        if sig_slot is not None:
            self.sig_slot = sig_slot

        if decrypt_slot is not None:
            self.decrypt_slot = decrypt_slot

    def generate_fw_image(self):

        if self.aes_key is not None:
            sec_section = security_section(self.SEC_SECT_ID, 78)
            sec_bytes = struct.pack('<2sH', *sec_section)

            # generate SHA256 to get upper 8 bytes of nonce value
            digest = hashes.Hash(hashes.SHA256(), backend=default_backend())
            digest.update(self.data)
            self.sha = digest.finalize()

            # IVT = nonce + ctr = 0
            self.ivt = self.sha[:8] + b'\0'*8

            encryptor = Cipher(algorithms.AES(self.aes_key),
                               modes.CTR(self.ivt),
                               backend=default_backend()).encryptor()
            self.encrypted_data = encryptor.update(self.data) + encryptor.finalize()
            self.data = self.encrypted_data

            crc32 = binascii.crc32(self.data)
            header = fw_header(self.FW_ID, self.img_size, crc32, self.version,
                               int(time.time()), 0x400)
            fw_hdr_bytes = struct.pack('<2sII16sII', *header)

            sig_section = signature_section(self.sig_slot, self.decrypt_slot,
                                            self.ivt[:8], self.SIG_SECT_ID, 64,
                                            b'\x00'*64)

            sig_bytes = struct.pack('<BB8s2sH64s', *sig_section)

            # signature is computed over device administration, padding,
            # and encrypted image
            if not self.revocation_list:
                admin_section = device_administration_section(self.DEV_ADM_SECT_ID, 0)
                admin_bytes = struct.pack('<2sH', *admin_section)
            else:
                revoke_section = revocation_section(self.REV_SECT_ID,
                                                    len(self.revocation_list * 2))
                revoke_bytes = struct.pack('<2sH', *revoke_section)
                for r in self.revocation_list:
                    revoke_bytes += struct.pack('BB', *r)

                admin_section = device_administration_section(self.DEV_ADM_SECT_ID,
                                                              len(revoke_bytes))
                admin_bytes = struct.pack('<2sH', *admin_section)
                admin_bytes += revoke_bytes

            # pad remainder from header to beginning of binary
            padlen = 1024 - len(sec_bytes) - len(admin_bytes) - len(sig_bytes) - len(fw_hdr_bytes)
            pad_bytes = b'\xff' * padlen

            signature = self.sig_key.sign_digest(admin_bytes + pad_bytes +
                                                 self.data)

            sig_section = signature_section(self.sig_slot, self.decrypt_slot,
                                            self.ivt[:8], self.SIG_SECT_ID, 64,
                                            signature)
            sig_bytes = struct.pack('<BB8s2sH64s', *sig_section)
            self.img += fw_hdr_bytes + sec_bytes + sig_bytes + admin_bytes + pad_bytes + self.data

        else:
            crc32 = binascii.crc32(self.data)
            header = fw_header(self.FW_ID, self.img_size, crc32, self.version,
                               int(time.time()), 0x400)
            fw_hdr_bytes = struct.pack('<2sII16sII', *header)
            sec_section = security_section(self.SEC_SECT_ID, 0)
            sec_bytes = struct.pack('<2sH', *sec_section)
            padlen = 1024 - len(sec_bytes) - len(fw_hdr_bytes)
            pad_bytes = b'\xff' * padlen
            self.img += fw_hdr_bytes + sec_bytes + pad_bytes + self.data

    def dump_to_file(self, path):
        with open(path, "wb") as f:
            f.write(self.img)


def validate_slot_index(ctx, param, value):
    if value in range(8):
        return value
    else:
        raise click.BadParameter("Slot value has to be between 0 and 7")


def build_revocation_list(ctx, param, value):
    revocation_list = []
    lookup_table = {'s': 0xa1, 'd': 0xa2, 'u': 0xa3}
    pattern = re.compile("s|u|d:[0-7]$")
    for entry in value:
        if not pattern.match(entry):
            raise click.BadParameter("Revocation must be in s:N, d:N, or u:N")

        t, s = entry.split(':')
        revocation_list.append(key_revocation_entry(lookup_table[t], int(s)))

    return revocation_list


def validate_version(ctx, param, value):
    if value and len(value) >= 16:
        raise click.BadParameter("Version string must be <= to 16")

    return value


@click.argument('infile')
@click.argument('outfile')
@click.option('-E', '--encrypt', metavar='filename', required=True, help='AES key file')
@click.option('-S', '--sign', metavar='filename', required=True, help='ED25519 Signature PEM file')
@click.option('-s', '--signature_slot', required=True, type=int,
              help='OTP Signature slot index', callback=validate_slot_index)
@click.option('-d', '--decrypt_slot', type=int, help='OTP decrypt slot index',
              required=True, callback=validate_slot_index)
@click.option('-r', '--revoke', help='Revocation entry', multiple=True,
              callback=build_revocation_list)
@click.option('-v', '--version', help='Version entry',
              callback=validate_version)
@click.command()
def secure(encrypt, sign, signature_slot, decrypt_slot,
           revoke, infile, outfile, version):

    img = da1469x_fw_image(infile, encrypt, sign, signature_slot,
                           decrypt_slot, revoke, version)
    img.generate_fw_image()
    img.dump_to_file(outfile)


@click.argument('infile')
@click.argument('outfile')
@click.option('-v', '--version', help='Version entry',
              callback=validate_version)
@click.command()
def nonsecure(infile, outfile, version):
    img = da1469x_fw_image(infile, None, None, None,
                           None, None, version)

    img.generate_fw_image()
    img.dump_to_file(outfile)


@click.group()
def cli():
    pass

cli.add_command(nonsecure)
cli.add_command(secure)

if __name__ == '__main__':
    cli()
