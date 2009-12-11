import os, struct

class RemoteFw:
    def __init__(self, node = 0):
        self.node = node
    def read(self, address, count):
        pipe = os.popen("fw_peek %d 0x%08x0x%08x" % (self.node, address, count), "r")
        return pipe.read()        
    def write(self, address, value):
        pipe = os.popen("fw_poke %d 0x%08x" % (self.node, address), "w")
        pipe.write(value)
    def send_init(self):
        msg = struct.pack("I", 0x500)
        self.write(0xfee00000, msg)
    def send_nmi(self):
        msg = struct.pack("I", 0x0400)
        self.write(0xfee00000, msg)
    def send_extint(self, vec):
        msg = struct.pack("I", vec & 0xff)
        self.write(0xfee00000, msg)
