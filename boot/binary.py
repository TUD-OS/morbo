import os

class Binary:
    def __init__(self, name):
        self.name = name
        self.symbols = {}
        for a,n in map(lambda x: (x.split()[0], x[:-1].split()[-1]), os.popen("nm %s"%self.name).readlines()):
            try:
                self.symbols[n] = int(a, 16)
            except ValueError:
                pass
        self.regions = []
        for line in os.popen("readelf -l %s"%self.name).readlines():
            line = line[:-1].strip()
            if line.startswith("LOAD"):
                self.regions.append(map(lambda x: int(x, 0), line.split()[1:6]))
            if line.startswith("Entry point"):
                self.entry_point = int(line.split()[-1], 0)
    def get_symbol(self, name):
        "return the symbol"
        return self.symbols[name]
    def get_phys(self, addr):
        "get the physical address of a symbol from the program headers"
        for ofs,virt,phys,fsize,msize in self.regions:
            if addr >= virt and addr < virt + msize:
                return addr - virt + phys
    def get_symbol_phys(self, name):
        "get the physical address of a symbol"
        return self.get_phys(self.get_symbol(name))
    def decode(self, fw):
        "elf decode the binary"
        f = open(self.name)
        for ofs,virt,phys,fsize,msize in self.regions:
            f.seek(ofs)
            fw.write(phys, f.read(fsize))
            fw.write(phys + fsize, "\x00"*(msize - fsize))
        return self.entry_point
