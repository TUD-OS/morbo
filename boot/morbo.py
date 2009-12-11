#!/usr/bin/env python
"""Multibooting through firewire with the help of morbo."""

import os, sys, struct, firewire, config


def read_pulsar_config(name, state):
    """Handle pulsar config files. This is not 100% compatible, as we
    handle "exec" the same as "load" and let morbo do the job of ELF
    extraction."""
    s = open(os.path.join(state[0], name)).read()
    s = s.replace("\\\n", "")
    for line in s.split("\n"):
        line = line.split("#")[0].strip()
        if not line:  continue
        param = line[4:].strip()
        if line.startswith("root"):
            state[0] = param
        elif line.startswith("exec") or line.startswith("load"):
            state.append(os.path.join(state[0], param))
        elif line.startswith("conf"):
            read_pulsar_config(param, state)
        elif line.startswith("addr"):
            state.append(int(param, 16))
        else:
            print "ignored line:", repr(line)

def boot(files, fw=None):
    mbheader = 0x00100000
    loadaddr = 0x01000000

    if not fw:
        fw = firewire.RemoteFw()
    header = fw.read(mbheader, 12)
    assert header, "could not read from firewire"
    header = struct.unpack("III", header)
    assert header[0] == 0x1badb002, "not a multiboot header - is morbo waiting?"

    print "MBI: %#x"%header[2]
    state = [os.path.expanduser("~/boot/")]

    print "read config files"
    for name in files:
        read_pulsar_config(name, state)

    print "push modules"
    mods = []
    for item in state[1:]:
        if type(item) == type(0):
            loadaddr = item
        else:
            name = item.split()[0]
            print "\tmod[%02d] %60s [%08x -"%(len(mods), name, loadaddr),
            data = open(name).read()
            fw.write(loadaddr, data)
            mods.append((loadaddr, loadaddr + len(data), item))
            loadaddr += len(data)
            print "%8x]"%loadaddr
            loadaddr += (0x1000 - (loadaddr & 0xfff)) & 0xfff
    
    # there is no good place for the multiboot modules!
    loadaddr = header[2] + 0x4000

    print "add modules at %#x"%loadaddr
    marray = []
    space = 16*len(mods)
    cmdlines = ""
    for m in mods:
        marray.append(struct.pack("IIII", m[0], m[1], loadaddr + space + len(cmdlines), 0))
        cmdlines += m[2] + "\x00"
    fw.write(loadaddr, "".join(marray) + cmdlines)
                 
    mbi = list(struct.unpack("I"*7, fw.read(header[2], 28)))
    mbi[0]|= 1<<3
    mbi[5] = len(mods)
    mbi[6] = loadaddr
    fw.write(header[2], struct.pack("I"*7, *mbi))
    
    print "boot"
    fw.write(mbheader, "\xff\xff\xff\xff\xff\xff\xff\xff")


if __name__ == "__main__":
    boot_morbo(sys.argv[1:])
