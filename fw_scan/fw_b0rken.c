/* -*- Mode: C -*- */

#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <libraw1394/raw1394.h>

#include "fw_b0rken.h"

/* XXX Evil copy&paste coding. */

/* Better do select/poll, but do we ever wake up? */
#define MAX_RETRIES 5

/** Try again upto MAX_RETRIES if raw1394_read returns EGAIN. */
int
raw1394_read_retry(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		   size_t length, quadlet_t *buffer)
{
  int retries = MAX_RETRIES;
  int ret;

 again:
  ret = raw1394_read(handle, node, addr, length, buffer);
  if (ret != 0) {
    if ((errno == EAGAIN) && (retries-- > 0)) {
      /* Seems to be a bug */
      goto again;
    }
  }

  return ret;
}

/** Try again upto MAX_RETRIES if raw1394_write returns EGAIN. */
int
raw1394_write_retry(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		   size_t length, quadlet_t *buffer)
{
  int retries = MAX_RETRIES;
  int ret;

 again:
  ret = raw1394_write(handle, node, addr, length, buffer);
  if (ret != 0) {
    if ((errno == EAGAIN) && (retries-- > 0))
      /* Seems to be a bug */
      goto again;
  }

  return ret;
}

/* EOF */
