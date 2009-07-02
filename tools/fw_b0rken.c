/* -*- Mode: C -*- */

#include <errno.h>
#include <libraw1394/raw1394.h>

#include "fw_b0rken.h"

/* XXX Evil copy&paste coding. */

/* Better do select/poll, but do we ever wake up? */
#define MAX_RETRIES 5
#define MAX_REQUEST_SIZE 512

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

int
raw1394_read_large(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		   size_t length, quadlet_t *buffer)
{
  char *rbuf = (char *)buffer;

  while (length > 0) {
    size_t req_size = (length > MAX_REQUEST_SIZE) ? MAX_REQUEST_SIZE : length;

    int res = raw1394_read_retry(handle, node, addr, req_size, (quadlet_t *)rbuf);

    if (res != 0)
      return res;

    addr += req_size;
    length -= req_size;
    rbuf += req_size;
  }

  return 0;
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

int
raw1394_write_large(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		    size_t length, quadlet_t *buffer)
{
  char *wbuf = (char *)buffer;

  while (length > 0) {
    size_t req_size = (length > MAX_REQUEST_SIZE) ? MAX_REQUEST_SIZE : length;

    int res = raw1394_write_retry(handle, node, addr, req_size, (quadlet_t *)wbuf);

    if (res != 0)
      return res;

    addr += req_size;
    length -= req_size;
    wbuf += req_size;
  }

  return 0;
}

/* EOF */
