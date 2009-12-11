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
                x = line.split()
                self.regions.append((int(x[2], 16),int(x[3], 16), int(x[5], 16)))

    def get_symbol(self, name):
        "return the symbol"
        return self.symbols[name]
    def get_phys(self, addr):
        for virt,phys,length in self.regions:
            if addr >= virt and addr < virt+length:
                return addr - virt + phys
        

    def get_symbol_phys(self, name):
        "get the physical address of a symbol"
        return self.get_phys(self.get_symbol(name))

