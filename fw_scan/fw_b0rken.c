/* -*- Mode: C -*- */

#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <gc.h>
#include <libraw1394/raw1394.h>

#include "fw_b0rken.h"

/* XXX Evil copy&paste coding. */

/* Better do select/poll, but do we ever wake up? */
#define MAX_RETRIES 5
const unsigned MAX_REQUEST_SIZE=2048;

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
raw1394_write_large_old(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
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

/** Handle completed events without blocking.
 */
static void process_events(raw1394handle_t handle)
{
  struct pollfd fd = { .fd = raw1394_get_fd(handle),
		       .events = POLLIN | POLLOUT };
  
  /* XXX This should never block, but does under strange
     circumstances. This is very likely a kernel bug. */
  while (poll(&fd, 1, 0) == 1) {
    raw1394_loop_iterate(handle);
  }
  
}

/** Write a large data block in blocks of MAX_REQUEST_SIZE. Ideally,
    this is should be very fast. Failed writes are retried. */
int
raw1394_write_large(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		    size_t length, quadlet_t *buffer)
{
  __label__ failure;

  /* Find out how many requests we need an allocate a req structure
     for each of them. */
  unsigned reqs_no = length/MAX_REQUEST_SIZE;
  bool failed = true;

  if ((length % MAX_REQUEST_SIZE) != 0)
    reqs_no++;

  unsigned completed = 0;
  struct retry {
    struct retry *next;
    unsigned req;
  } *retry_list = 0;
  
  int local_handler(raw1394handle_t handle, unsigned long tag, raw1394_errcode_t err)
  {
    if (raw1394_get_ack(err) == 1) {
      completed++;
    } else {
      struct retry *retry = GC_NEW(struct retry);
      retry->next = retry_list;
      retry->req  = tag;
      retry_list = retry;
    }
    
    return err;
  }

  void start_req(unsigned cur) {
    char *reqbuf  = ((char *)buffer) + cur*MAX_REQUEST_SIZE;
    size_t reqlen = (MAX_REQUEST_SIZE*(cur+1) > length) ? (length % MAX_REQUEST_SIZE) : MAX_REQUEST_SIZE;

    int res = raw1394_start_write(handle, node, addr + cur*MAX_REQUEST_SIZE, reqlen,
				  (quadlet_t *)reqbuf, cur);
    if (res != 0)
      goto failure;
  }

  tag_handler_t old_handler = raw1394_set_tag_handler(handle, local_handler);

  for (unsigned cur = 0; cur < reqs_no; cur++) {
    start_req(cur);
    process_events(handle);
  }

  while (completed < reqs_no) {
    if (retry_list) {
      unsigned cur = retry_list->req;
      retry_list = retry_list->next;
      start_req(cur);
    }
    process_events(handle);
  }

  failed = false;
 failure:
  raw1394_set_tag_handler(handle, old_handler);
  return failed ? -1 : 0;
}


/* EOF */
