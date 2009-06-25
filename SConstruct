# -*- Mode: Python -*-

def CheckCommand(context, cmd):
       context.Message('Checking for %s command... ' % cmd)
       result = WhereIs(cmd)
       context.Result(result is not None)
       return result


# Construct freestanding environment
freestanding_env = Environment()

freestanding_env['CCFLAGS'] = "-O2 -m32 -march=pentium3 -pipe -g -std=gnu99 -ffreestanding -nostdlib -Wno-multichar -Werror"
freestanding_env['LINKFLAGS'] = "-m elf_i386 -gc-sections -N"
freestanding_env['LINK'] = "ld"
freestanding_env['AS'] = "yasm"
freestanding_env['ASFLAGS'] = "-g stabs -O5 -f elf32"

conf = Configure(freestanding_env, custom_tests = {'CheckCommand' : CheckCommand})

if not conf.CheckCommand("yasm"):
    print("Please install yasm.")
    Exit(1)

# Barfs with scons 1.2.0
if not conf.CheckCC():
    print("Your compiler is not usable.")
    Exit(1)

if not (conf.CheckCHeader("stdint.h") and
        conf.CheckCHeader("stdarg.h")):
    print("Standard C headers are missing.")
    Exit(1)

freestanding_env = conf.Finish()

Export('freestanding_env')

SConscript(["standalone/SConscript",
            "tools/SConscript",
            ])

# EOF
