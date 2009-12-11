import os, struct

class RemoteFw:
    def __init__(self):
        self.node = 0
        self.bs = 2048
    def read(self, address, count):
        bs = self.bs < count and self.bs or count
        pipe = os.popen("fwcat -n %d -r -a %#x -s %#x -c %#x"%(self.node, address, bs, count/bs), "r")
        return pipe.read()        
    def write(self, address, value):
        bs = self.bs < len(value) and self.bs or len(value)
        pipe = os.popen("fwcat -n %d -w -a %#x -s %#x -c %#x"%(self.node, address, bs, (len(value)+bs-1)/bs), "w")
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
