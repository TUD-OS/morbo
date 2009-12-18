#!/usr/bin/env python
"""Multibooting through firewire with the help of morbo."""

# TODO Support for more than two devices on the bus.

import os, sys, struct, firewire, string, config, getopt, time
from socket import ntohl

CROM_ADDR = 0xfffff0000400

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

def is_morbo(fw=firewire.RemoteFw()):
    MORBO_VENDOR_ID = 0xCAFFEE
    MORBO_MODEL_ID  = 0x000002
    
    # XXX These indices are hardcoded for Morbo's ConfigROM. We should
    # implement proper parsing...
    try:
        vendor = ntohl(struct.unpack("I", fw.read(CROM_ADDR +  6*4, 4))[0]) & 0xFFFFFF
        model  = ntohl(struct.unpack("I", fw.read(CROM_ADDR +  7*4, 4))[0]) & 0xFFFFFF
    except firewire.FirewireException, err:
        vendor = 0
        model  = 0

    if ((vendor == MORBO_VENDOR_ID) and (model == MORBO_MODEL_ID)):
        return (True, vendor, model)
    else:
        return (False, 0, 0)

def boot(files, fw=firewire.RemoteFw()):
    loadaddr = 0x01000000

    ready, vendor, model = is_morbo(fw)
    assert(ready)

    remote_mbi = ntohl(struct.unpack("I", fw.read(CROM_ADDR + 18*4, 4))[0])
    print("MBI: %#x" % remote_mbi)

    # Check if the node is ready to receive something (no modules in
    # MBI).
    assert fw.read_quadlet(remote_mbi + 5*4) == 0, "Already booted."

    state = [config.PATHS["bootdir"]]

    print "read config files"
    for name in files:
        read_pulsar_config(name, state)
    print(state)

    print "push modules"
    mods = []
    for item in state[1:]:
        if type(item) == type(0):
            loadaddr = item
        else:
            name = item.split()[0]
            print "\tmod[%02d] %s [%08x -"%(len(mods), string.ljust(name, 60), loadaddr),
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
                 
    mbi = list(struct.unpack("I"*7, fw.read(remote_mbi, 28)))
    mbi[0] |= 1<<3
    mbi[6]  = loadaddr
    fw.write(remote_mbi, struct.pack("I"*7, *mbi))
    
    print("Boot!")
    # Morbo waits for the module count to change. Update it after the
    # rest of the multiboot info is written.
    fw.write(remote_mbi + 5*4, struct.pack("I", len(mods)))
    if (len(mods) == 1):
        print("XXX Only one module loaded! We might try to DMA to a running node...");

if __name__ == "__main__":
    try:
        opts, args = getopt.getopt(sys.argv[1:], "", ["once"])
        opts = set([ a for (a, b) in opts ]) # Strip parameter
        if not (opts & set(["--once"])):
            print("Waiting for a Morbo node...")
            while not is_morbo()[0]:
                time.sleep(1)
        boot([args[0]])
    except getopt.GetoptError, err:
        # print help information and exit:
        print(str(err)) # will print something like "option -a not recognized"
        print("Options:")
        print("  --once    Don't wait for a node to come up.")
        sys.exit(2)
    except KeyboardInterrupt, err:
        print("Interrupted.");

