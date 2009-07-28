/* -*- Mode: C++ -*- */

#define _BSD_SOURCE

#include <cstdlib>
#include <cstdio>

#include <unistd.h>

#include <brimstone.h>

using namespace std;
using namespace Brimstone;

int
main(int argc, char **argv)
{
  if (argc != 2) {
    cerr << "Usage: fw_provoke fw-dev" << endl;
    return 1;
  }

  Node node(argv[1]);

  unsigned count = 0;
  char buf[0x6CC];
  while (1) {
    node.post_write(0x1000, buf, sizeof(buf));
    node.poll();

    printf("count = %u\n", ++count);
    usleep(15);
  }
  
  return 0;
}
