/* -*- Mode: C -*- */
/*
 * Multiboot definitions.
 *
 * Copyright (C) 2009-2012, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Morbo.
 *
 * Morbo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Morbo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <mbi.h>

bool mbi_find_memory(const struct mbi *multiboot_info, size_t len,
                     void **block_start, size_t *block_len,
                     bool highest);

void *mbi_alloc_protected_memory(struct mbi *multiboot_info, size_t len, unsigned align);

void mbi_relocate_modules(struct mbi *mbi, bool uncompress);


/* EOF */
