/* -*- Mode: C -*- */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <getopt.h>
#include <unistd.h>

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

#include <ohci-constants.h>

/* Constants */

/* Globals */

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
      fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  /* Connect to raw1394 device. */
  fw_handle = raw1394_new_handle_on_port(port);
  if (fw_handle == NULL) {
    perror("raw1394_new_handle_on_port");
    exit(EXIT_FAILURE);
  }

  unsigned int nodes = raw1394_get_nodecount(fw_handle);
  
  for (unsigned int node = 0; node < nodes; node++) {
    printf("Node %u:\n", node);

    unsigned int offset = 0;
    while (offset < 0x100) {
      printf("%8x |", offset*sizeof(quadlet_t));

      for (unsigned int line_off = 0; line_off < 4; line_off++) {
	quadlet_t buf;
	int ret = raw1394_read(fw_handle, LOCAL_BUS | node,
			       CSR_REGISTER_BASE + CSR_CONFIG_ROM + (offset++)*sizeof(quadlet_t),
			       sizeof(quadlet_t), &buf);

	if (ret == 0) {
	  printf(" %08x", buf);
	} else {
	  printf(" %8s", strerror(errno));
	}
      }
      printf("\n");
    }

    printf("\n");
  }

  return 0;
}

/* EOF */
