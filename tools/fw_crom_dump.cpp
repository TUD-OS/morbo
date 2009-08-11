/* -*- Mode: C++ -*- */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <errno.h>
#include <getopt.h>
#include <unistd.h>

#include <libraw1394/csr.h>

#include <fstream>
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
    fprintf(stderr, "Usage: %s [-d fw-dev] [-u guid]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (isatty(STDOUT_FILENO)) {
    cerr << "Refusing to write binary data to your terminal." << endl;
    cerr << "Piping into hexdump is a way better idea. 8-)" << endl;
    return 1;
  }

  /* Connect to Firewire device. */
  std::ofstream devnull;
  devnull.open("/dev/null");
  try {
    Node *node;

    // XXX delete
    if (got_guid)
      node = new Node(guid, devnull);
    else
      node = new Node(dev, devnull);

    unsigned int offset = 0;
    while (offset < 0x100) {
      uint32_t data = node->quadlet_read(CSR_REGISTER_BASE + CSR_CONFIG_ROM + (offset++)*sizeof(uint32_t));
      
      cout.write((const char *)&data, sizeof(data));
    }

    return 0;
  } catch (exception *e) {
    cerr << "Caught exception: '" << e->what()
	 << "'. Bye. :-/" << endl;
    return 1;
  }
}

/* EOF */
