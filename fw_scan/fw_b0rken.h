/* -*- Mode: C -*- */

#pragma once

#include <libraw1394/raw1394.h>

int raw1394_read_retry(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		       size_t length, quadlet_t *buffer);

int raw1394_read_large(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		       size_t length, quadlet_t *buffer);


int raw1394_write_retry(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		       size_t length, quadlet_t *buffer);

int raw1394_write_large(raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
			size_t length, quadlet_t *buffer);


/* EOF */
