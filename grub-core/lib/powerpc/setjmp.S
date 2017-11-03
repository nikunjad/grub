/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2007  Free Software Foundation, Inc.
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

#include <grub/symbol.h>
#include <grub/dl.h>
#include <ppc-asm.h>

	.file	"setjmp.S"

GRUB_MOD_LICENSE "GPLv3+"

	.text

#if defined( __powerpc64__ ) || defined( __powerpc64le__ )
#define LOAD	ld
#define STORE	std
#define SZ_LONG	8
#else
#define LOAD	lwz
#define STORE	stw
#define SZ_LONG	4
#endif

/*
 * int grub_setjmp (grub_jmp_buf env)
 */
FUNC_START(grub_setjmp)
	STORE	1, 0(3)
	STORE	14, 1*SZ_LONG(3)
	STORE	15, 2*SZ_LONG(3)
	STORE	16, 3*SZ_LONG(3)
	STORE	17, 4*SZ_LONG(3)
	STORE	18, 5*SZ_LONG(3)
	STORE	19, 6*SZ_LONG(3)
	STORE	20, 7*SZ_LONG(3)
	STORE	21, 8*SZ_LONG(3)
	STORE	22, 9*SZ_LONG(3)
	STORE	23, 10*SZ_LONG(3)
	STORE	24, 11*SZ_LONG(3)
	STORE	25, 12*SZ_LONG(3)
	STORE	26, 13*SZ_LONG(3)
	STORE	27, 14*SZ_LONG(3)
	STORE	28, 15*SZ_LONG(3)
	STORE	29, 16*SZ_LONG(3)
	STORE	30, 17*SZ_LONG(3)
	STORE	31, 18*SZ_LONG(3)
	mflr	4
	STORE	4, 19*SZ_LONG(3)
	mfcr	4
	STORE	4, 20*SZ_LONG(3)
	li	3, 0
	blr
FUNC_END(grub_setjmp)

/*
 * int grub_longjmp (grub_jmp_buf env, int val)
 */
FUNC_START(grub_longjmp)
	LOAD	1, 0(3)
	LOAD	14, 1*SZ_LONG(3)
	LOAD	15, 2*SZ_LONG(3)
	LOAD	16, 3*SZ_LONG(3)
	LOAD	17, 4*SZ_LONG(3)
	LOAD	18, 5*SZ_LONG(3)
	LOAD	19, 6*SZ_LONG(3)
	LOAD	20, 7*SZ_LONG(3)
	LOAD	21, 8*SZ_LONG(3)
	LOAD	22, 9*SZ_LONG(3)
	LOAD	23, 10*SZ_LONG(3)
	LOAD	24, 11*SZ_LONG(3)
	LOAD	25, 12*SZ_LONG(3)
	LOAD	26, 13*SZ_LONG(3)
	LOAD	27, 14*SZ_LONG(3)
	LOAD	28, 15*SZ_LONG(3)
	LOAD	29, 16*SZ_LONG(3)
	LOAD	30, 17*SZ_LONG(3)
	LOAD	31, 18*SZ_LONG(3)
	LOAD	5, 19*SZ_LONG(3)
	mtlr	5
	LOAD	5, 20*SZ_LONG(3)
	mtcr	5
	mr.	3, 4
	bne	1f
	li	3, 1
1:	blr
FUNC_END(grub_longjmp)
