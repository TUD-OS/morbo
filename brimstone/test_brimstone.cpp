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
    Node p(argv[1]);

    cout << "target[0] = " << p.quadlet_read(0) << endl;

  } catch (SyscallError *syserr) {
    printf("Syscall error: %s\n", syserr->what());
    return 1;
  };

  return 0;
}

// EOF
