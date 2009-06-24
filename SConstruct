# -*- Mode: Python -*-

def CheckCommand(context, cmd):
       context.Message('Checking for %s command... ' % cmd)
       result = WhereIs(cmd)
       context.Result(result is not None)
       return result

# Construct freestanding environment
fenv = Environment()

conf = Configure(fenv, custom_tests = {'CheckCommand' : CheckCommand})

if not conf.CheckCommand("yasm"):
    print("Please install yasm.")
    Exit(1)

# Barfs with scons 1.2.0
if not (conf.CheckCC() and conf.CheckCXX()):
    print("Your compiler is not usable.")
    Exit(1)

if not (conf.CheckCHeader("stdint.h") and
        conf.CheckCHeader("stdarg.h")):
    print("Standard C headers are missing.")
    Exit(1)

fenv = conf.Finish()

fenv['CPPPATH'] = ["include/"]
fenv['CCFLAGS'] = "-O2 -m32 -march=pentium3 -pipe -g -std=gnu99 -ffreestanding -nostdlib -Wno-multichar"
fenv['LINKFLAGS'] = "-m elf_i386 -gc-sections -N -T morbo.ld" 
fenv['LINK'] = "ld"
fenv['AS'] = "yasm"
fenv['ASFLAGS'] = "-g stabs -O5 -f elf32"

cutil = fenv.StaticLibrary('cutil',
                           [ 'strcmp.c',
                             'strncpy.c',
                             'strtok.c',
                             'strtol.c',
                             'strtoll.c',
                             ])

final = fenv.Program('morbo',
                     ['start.asm',
                      'cpuid.asm',
                      'crc16.c',
                      'util.c',
                      'elf.c',
                      'reboot.c',
                      'pci.c',
                      'pci_db.c',
                      'printf.c',
                      'main.c',
                      'version.c',
                      'serial.c',
                      'ohci.c',
                      'apic.c',
                      ],
                     LIBS=['cutil'],
                     LIBPATH=['.']
                     )
Depends(final, 'morbo.ld')

# EOF
