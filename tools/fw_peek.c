/* -*- Mode: C -*- */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

#include <ohci.h>

/* Constants */

/* Globals */

static char usage[] = "Usage: %s [-p port] node addr\n";
static int port = 0;
static raw1394handle_t fw_handle;

/* Implementation */

int
main(int argc, char **argv)
{
  /* Command line parsing */
  int opt;

  while ((opt = getopt(argc, argv, "p:")) != -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    default:
      fprintf(stderr, usage, argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if ((argc - optind) != 2) {
    fprintf(stderr, usage, argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Connect to raw1394 device. */
  fw_handle = raw1394_new_handle_on_port(port);
  if (fw_handle == NULL) {
    perror("raw1394_new_handle_on_port");
    exit(EXIT_FAILURE);
  }

  unsigned int node_no = strtol(argv[optind], NULL, 0);
  nodeaddr_t   addr    = strtoll(argv[optind+1], NULL, 0);
  quadlet_t    val;

  printf("Reading address %llx on node %x.\n", addr, node_no);

  int res = raw1394_read(fw_handle, LOCAL_BUS | node_no, addr, sizeof(quadlet_t), &val);
  if (res != 0) {
    perror("raw1394_read");
    exit(EXIT_FAILURE);
  }

  printf("%lx\n", val);

  return 0;
}

/* EOF */
