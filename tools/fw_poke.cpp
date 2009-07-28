/* -*- Mode: C++ -*- */

#include <cstdio>
#include <cstdlib>

#include <getopt.h>
#include <unistd.h>

#include <brimstone.h>

using namespace Brimstone;

static char usage[] = "Usage: %s node-dev addr data\n";

int
main(int argc, char **argv)
{
  /* Command line parsing */
  int opt;

  while ((opt = getopt(argc, argv, "")) != -1) {
    switch (opt) {
    default:
      fprintf(stderr, usage, argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if ((argc - optind) != 3) {
    fprintf(stderr, usage, argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Connect to Firewire device. */
  Node node(argv[optind]);

  uint64_t addr = strtoll(argv[optind+1], NULL, 0);
  uint32_t val  = strtol(argv[optind+2], NULL, 0);

  node.quadlet_write(addr, val);

  return 0;
}

/* EOF */
