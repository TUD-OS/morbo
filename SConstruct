# -*- Mode: Python -*-

def CheckPKGConfig(context, version):
     context.Message( 'Checking for pkg-config... ' )
     ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
     context.Result( ret )
     return ret

def CheckPKG(context, name):
     context.Message( 'Checking for %s... ' % name )
     ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
     context.Result( ret )
     return ret

fw_env = Environment()

fw_env['CPPPATH'] = ["#include/"]
fw_env['CCFLAGS'] = "-Os -march=native -pipe -g "
fw_env['CXXFLAGS'] = "-std=c++0x"
fw_env['CFLAGS'] = "-std=c99 "

conf = Configure(fw_env, custom_tests = {'CheckPKGConfig' : CheckPKGConfig,
                                       'CheckPKG' : CheckPKG })

if not conf.CheckPKGConfig('0.15.0'):
       print('pkg-config >= 0.15.0 not found.')
       Exit(1)

build_fw_scan = True
build_tools   = True;

if not conf.CheckPKG('libraw1394'):
       print('Could not find libraw1394.')
       build_tools = False
       build_fw_scan = False
else:
       conf.env.ParseConfig('pkg-config --cflags --libs libraw1394')

if not conf.CheckPKG('sdl'):
       print('Could not find libsdl.')
       build_tools = False

if build_fw_scan and not conf.CheckCHeader('slang.h'):
       print('Could not find slang headers.')
       build_fw_scan = False
else:
       conf.env.Append(LIBS = ['slang'])

if build_fw_scan and not conf.CheckFunc('SLsmg_init_smg'):
       print('Could not link to slang.')
       build_fw_scan = False

if not build_fw_scan:
       print('fw_scan will not be built.')

if not build_tools:
       print('Firewire host tools will not be built.')
else:
       conf.env.Append(LIBS = ['SDL'])

fw_env = conf.Finish()

Export('fw_env')

SConscript(["standalone/SConscript"])

if build_tools:
       SConscript(["tools/SConscript"])

if build_fw_scan:
       SConscript(["fw_scan/SConscript"])

# EOF
