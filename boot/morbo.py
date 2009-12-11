#!/usr/bin/env python
"""Multibooting through firewire with the help of morbo."""

import os, sys, struct, firewire

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
    cromaddr = 0xfffff0000400
    loadaddr = 0x01000000

    MORBO_VENDOR_ID = 0xCAFFEE
    MORBO_MODEL_ID  = 0x000002

    if not fw:
        fw = firewire.RemoteFw()
    
    # XXX These indices are hardcoded for Morbo's ConfigROM. We should
    # implement proper parsing...
    remote_vendor = struct.unpack("I", fw.read(cromaddr +  6*4, 4)) & 0xFFFFFF
    remote_model  = struct.unpack("I", fw.read(cromaddr +  7*4, 4)) & 0xFFFFFF
    remote_mbi    = struct.unpack("I", fw.read(cromaddr + 18*4, 4))
    assert (remote_vendor == MORBO_VENDOR_ID) and (remote_model == MORBO_MODEL_ID), "Not a Morbo node."
    print "MBI: %#x" % remote_mbi

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
    loadaddr = remote_mbi + 0x4000

    print "add modules at %#x" % loadaddr
    marray = []
    space = 16*len(mods)
    cmdlines = ""
    for m in mods:
        marray.append(struct.pack("IIII", m[0], m[1], loadaddr + space + len(cmdlines), 0))
        cmdlines += m[2] + "\x00"
    fw.write(loadaddr, "".join(marray) + cmdlines)
                 
    mbi = list(struct.unpack("I"*7, fw.read(mbi, 28)))
    mbi[0] |= 1<<3
    #mbi[5]  = len(mods)
    mbi[6]  = loadaddr
    fw.write(remote_mbi, struct.pack("I"*7, *mbi))
    
    print("Boot!")
    # Morbo waits for the module count to change. Update it after the
    # rest of the multiboot info is written.
    fw.write(remote_mbi + 5*4, struct.pack("I", len(mods)))

if __name__ == "__main__":
    boot_morbo(sys.argv[1:])
