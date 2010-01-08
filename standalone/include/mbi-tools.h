/* -*- Mode: C -*- */

#pragma once

#include <stddef.h>
#include <mbi.h>

void *mbi_alloc_protected_memory(struct mbi *multiboot_info, size_t len, unsigned align);


/* EOF */
