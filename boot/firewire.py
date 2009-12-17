import os, struct, config
import subprocess

class FirewireException(Exception):
    def __init__(self, msg):
        self.msg = msg
    def __str__(self):
        return "Remote DMA failed: %s" % self.msg

class RemoteFw:
    def __init__(self, node = 0):
        self.node = node

    def read(self, address, count):
        peek = subprocess.Popen(config.PROGS["fwread"]%{"node" : self.node, "address": address, "count": count},
                                shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        data, err = peek.communicate(None)
        if peek.returncode != 0:
            raise FirewireException(err)
        assert len(data) == count, "fw_peek seems confused"
        return data

    def read_quadlet(self, address):
        data = self.read(address, 4)
        return struct.unpack("I", data)[0]

    def write(self, address, data):
        poke = subprocess.Popen("fw_poke %d 0x%08x" % (self.node, address),
                                shell=True, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        err = poke.communicate(data)[1]
        if poke.returncode != 0:
            raise FirewireException(err)

    def write_quadlet(self, address, value):
        data = struct.pack("I", value)
        self.write(address, data)

    def send_init(self):
        msg = struct.pack("I", 0x500)
        self.write(0xfee00000, msg)
    def send_nmi(self):
        msg = struct.pack("I", 0x0400)
        self.write(0xfee00000, msg)
    def send_extint(self, vec):
        msg = struct.pack("I", vec & 0xff)
        self.write(0xfee00000, msg)
