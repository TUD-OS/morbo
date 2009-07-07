/* -*- Mode: C -*- */

#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <libraw1394/raw1394.h>

int
main(int argc, char **argv)
{
  unsigned port, node;

  if (argc < 3) {
    fprintf(stderr, "Too few arguments\n");
    return 1;
  }

  port = atoi(argv[1]);
  node = atoi(argv[2]);
  printf("port = %u, node = %u\n", port, node);

  raw1394handle_t h = raw1394_new_handle_on_port(port);
  if (h == NULL) {
    perror("raw1394_new_handle_on_port");
    return 1;
  }

  unsigned count = 0;
  char buf[128];
  while (1) {
    int res = raw1394_write(h, 0xFFC0 | node, 0, sizeof(buf),
                            (quadlet_t *)buf);
    printf("res = %d, count = %u\n", res, ++count);
    usleep(100000);
  }
  
  return 0;
}
