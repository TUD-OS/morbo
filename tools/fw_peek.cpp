/* -*- Mode: C++ -*- */

#include <cstdio>
#include <cstdlib>

#include <getopt.h>
#include <unistd.h>

#include <brimstone.h>

using namespace Brimstone;

static char usage[] = "Usage: %s node-dev addr\n";

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

  if ((argc - optind) != 2) {
    fprintf(stderr, usage, argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Connect to Firewire device. */
  Node node(argv[optind]);

  printf("%x\n", node.quadlet_read(strtoll(argv[optind+1], NULL, 0)));

  return 0;
}

/* EOF */
