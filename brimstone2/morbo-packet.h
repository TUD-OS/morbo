/* -*- Mode: C -*- */
#pragma once

#include <linux/ioctl.h>

#define MORBO_PACKET_REQ 1
#define MORBO_PACKET_RSP 2

struct packet_header {
  /* Size of buffer */
  uint16_t size;

  /* For received packets this is copied from Xferstatus. For packets
     to be sent this has to be either MORBO_PACKET_REQ or
     MORBO_PACKET_RSP to indicate a request or response. */
  uint16_t event;
};

#define NODE_ID_VALID (1U<<31)
#define NODE_ID_ROOT  (1U<<30)
#define NODE_ID_CPS   (1U<<27)

struct morbo_selfid_buffer {
  /* The contents of the Node identification and status register. The
     lower 16-bit are the node ID. */
  uint32_t node_id;
  uint32_t selfid[504/4];
};

#define MORBO_IOC_MAGIC 0xBF
#define MORBO_IOC_GET_SELFID _IOR(MORBO_IOC_MAGIC, 0, struct morbo_selfid_buffer)
#define MORBO_IOC_BUS_RESET  _IO(MORBO_IOC_MAGIC, 1)

/* EOF */
