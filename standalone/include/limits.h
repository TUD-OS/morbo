/*
 * Limits.
 *
 * Copyright (C) 2012, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#ifdef __SIZEOF_LONG_LONG__
# if __SIZEOF_LONG_LONG__ == 8
#  define ULLONG_MAX 0xFFFFFFFFFFFFFFFFULL
# else
#  error Weird compiler...
# endif
#else
# error Could not detect size of long long.
#endif

/* EOF */
