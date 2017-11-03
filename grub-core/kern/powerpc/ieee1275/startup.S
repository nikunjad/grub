/* startup.S - Startup code for the PowerPC.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2007,2008  Free Software Foundation, Inc.
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
#include <grub/offsets.h>
#include <ppc-asm.h>

.extern __bss_start
.extern _end

	.text

FUNC_START(_start)

#if defined(_CALL_ELF) && _CALL_ELF == 2  /* LITTLE ENDIAN */
	 tdi  0,0,0x48              /* Reverse endian of b . + 8          */
         b    $+36                  /* Skip trampoline if already LE mode */ 
        .long 0x05009f42            /* bcl 20,31,$+4                      */
        .long 0xa602487d            /* mflr r10                           */
        .long 0x1c004a39            /* addi r10,r10,28                    */
        .long 0xa600607d            /* mfmsr r11                          */
        .long 0x01006b69            /* xori r11,r11,1                     */
        .long 0xa6035a7d            /* mtsrr0 r10                         */
        .long 0xa6037b7d            /* mtsrr1 r11                         */
        .long 0x2400004c            /* rfid                               */
#endif

	li      2, 0
	li      13, 0

	/* Stage1 won't zero BSS for us. In other cases, why not do it again?  */
	lis	6, (__bss_start - 4)@h
	ori	6, 6, (__bss_start - 4)@l

2:	stb     2, 4(6)
	addi    6, 6, 1
	andi.   7, 6, 3
	cmpi    0, 1, 7, 0
	bne	2b
	
	lis	7, (_end - 4)@h
	ori	7, 7, (_end - 4)@l
	subf	7, 6, 7
	subi    8, 7, 1
	andi.   8, 8, 3
	addi    8, 8, 1
	sub	7, 7, 8

	srwi	7, 7, 2 /* We store 4 bytes at a time.  */
	mtctr	7
2:	stwu	2, 4(6) /* We know r2 is already 0 from above.  */
	bdnz	2b

	mtctr   8
2:	stb	2, 4(6) /* We know r2 is already 0 from above.  */
	addi    6, 6, 1
	bdnz	2b

	/* Store r5 in grub_ieee1275_entry_fn.  */
	lis	9, grub_ieee1275_entry_fn@ha
#if defined(_CALL_ELF) && _CALL_ELF == 1  /* BIG ENDIAN */
	std	5, grub_ieee1275_entry_fn@l(9)
#else
	stw	5, grub_ieee1275_entry_fn@l(9)
#endif

#if defined(_CALL_ELF) && _CALL_ELF == 2  /* LITTLE ENDIAN */
	bl	3f
3:	mflr	9
	addis	2,9,.TOC.-3b@ha
	addi	2,2,.TOC.-3b@l
#endif

	bl	grub_main
1:	b	1b
FUNC_END(_start)
