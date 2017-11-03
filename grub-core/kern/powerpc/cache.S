/* cache.S - Flush the processor cache for a specific region.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2007,2010  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ppc-asm.h>

	.text

	.align 2
FUNC_START(grub_arch_sync_caches)
#include "cache_flush.S"
	blr
FUNC_END(grub_arch_sync_caches)
