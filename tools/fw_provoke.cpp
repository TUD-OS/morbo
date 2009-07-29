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
  /* Command line parsing */
  int opt;
  bool got_guid = false;
  uint64_t guid = 0;
  char *dev = NULL;

  while ((opt = getopt(argc, argv, "d:u:")) != -1) {
    switch (opt) {
    case 'd':
      dev = optarg;
      break;
    case 'u':
      guid = strtoll(optarg, NULL, 16);
      got_guid = true;
      break;
    default:
      fprintf(stderr, "Usage: %s [-d fw-dev] [-u guid]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (!got_guid && (dev == NULL)) {
    cerr << "You need to specify either a GUID or a device string." << endl;
    return 1;
  }

  Node *node;
  if (got_guid)
    node = new Node(guid);
  else
    node = new Node(dev);
  
  unsigned count = 0;
  char buf[0x6CC];
  while (1) {
    node->post_write(0x1000, buf, sizeof(buf));
    node->poll();

    printf("count = %u\n", ++count);
    // usleep(15);
  }
  
  delete node;

  return 0;
}
