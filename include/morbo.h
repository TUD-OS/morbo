/* -*- Mode: C -*- */
/*
 * Morbo constants
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

#include <stdint.h>

/* Leaf types */

#define MORBO_VENDOR_ID 0xCAFFEEU 
#define MORBO_MODEL_ID  0x000002U

#define MORBO_INFO_DIR  ((2 << 6) | 0x38)

/* Flags  */

#define REMOTE_BOO

/* EOF */
