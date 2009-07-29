#!/usr/bin/env python
# -*- Mode: Python -*-

import os
import sys
import array

def fsize(f):
    old = f.tell()
    f.seek(0, os.SEEK_END)
    size = f.tell()
    f.seek(old, os.SEEK_SET)
    return size

infile = open(sys.argv[1], "rb")
outfile = open(sys.argv[2], "wb")

rom = array.array('B')          # unsigned byte
rom.fromfile(infile, fsize(infile))

# Extend size to multiple of 512 (high-performance version *g*)
while ((len(rom) % 512) != 0):
    rom.append(0)

# Set size field in ROM header.
rom[2] = len(rom) / 512

print(rom[3])

# Clear checksum in ROM image
rom[6] = 0

# Compute checksum
checksum = 0
for byte in rom:
    checksum = (checksum + byte) & 0xFF

rom[6] = 256 - checksum

rom.tofile(outfile)


# EOF
