#!/usr/bin/env python
"""reboot the remote system"""

import struct, os, sys, binary

def reboot_nova(kernel, fw):
    """rebooting a running nova is hard: we modify the NMI vector in
    the IDT to point to the ACPI-reset routine and send an NMI."""
    b = binary.Binary(kernel)
    idt = b.get_symbol_phys("_ZN3Idt3idtE")
    acpi_reset = b.get_symbol("_ZN4Acpi5resetEv")
    desc= [acpi_reset & 0xffff, 0x0008, 0x8e00, (acpi_reset >> 16) & 0xffff]
    fw.write(idt + 2*0x8, struct.pack("H"*4,*desc))
    fw.send_nmi()

def reboot(kernel, fw=None):
    if not fw:
        import firewire    
        fw = firewire.RemoteFw()
    fw.send_init()
    reboot_nova(kernel, fw)
    
if __name__ == "__main__":
    reboot(sys.argv[1])
