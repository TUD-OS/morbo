// -*- Mode: C++ -*-

#include <brimstone.h>

#include <cstdio>

using namespace std;
using namespace Brimstone;

int
main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "Usage: %s /dev/fw??\n", argv[0]);
    return 1;
  }

  try {
    Port p(argv[1], cout);

    char buf[512];
    p.post_block_read(0xFFC0, 0x0, buf, sizeof(buf));

    while (1) {
      p.wait();
      p.read_event();
    }

  } catch (SyscallError *syserr) {
    printf("Syscall error: %s\n", syserr->what());
    return 1;
  };

  return 0;
}

// EOF
