/* entry.S - open firmware call entry and return  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2007,2010,2014  Free Software Foundation, Inc.
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

#define STACK_FRAME_SIZE	(48 + 64 + 16 + 144)
#define MSR_OFFSET		(48 + 64)
#define R13_OFFSET		(MSR_OFFSET + 8)
#define NVREG_OFFSET(i)		(STACK_FRAME_SIZE - (32-(i))*8)
#define SAVE_NVGPR(A)	std	(A),NVREG_OFFSET(A)(r1)
#define REST_NVGPR(A)	ld	(A),NVREG_OFFSET(A)(r1)
#define SPRN_SRR0	0x01A
#define SPRN_SRR1	0x01B
#define r1	sp


#if defined(_CALL_ELF) && _CALL_ELF == 1  /* BIG ENDIAN */
#define TOC_OFFSET		40
#define r2	toc
#endif

/*
 * OF runs in 32 bit mode so it clobbers the high 32 bits of all registers
 * it saves. We save and restore all the non volatile registers to avoid
 * this issue.
 *
 * unsigned int ieee1275_call_entry_fn(void *args, unsigned long entry);
 */
FUNC_START(ieee1275_call_entry_fn)
	mflr	r0
	std	r0,16(r1)
	stdu	r1,-STACK_FRAME_SIZE(r1)

#if defined(_CALL_ELF) && _CALL_ELF == 1  /* BIG ENDIAN */
	std	r2,TOC_OFFSET(r1)
#endif
	std	r13,R13_OFFSET(r1)

	SAVE_NVGPR(r14)
	SAVE_NVGPR(r15)
	SAVE_NVGPR(r16)
	SAVE_NVGPR(r17)
	SAVE_NVGPR(r18)
	SAVE_NVGPR(r19)
	SAVE_NVGPR(r20)
	SAVE_NVGPR(r21)
	SAVE_NVGPR(r22)
	SAVE_NVGPR(r23)
	SAVE_NVGPR(r24)
	SAVE_NVGPR(r25)
	SAVE_NVGPR(r26)
	SAVE_NVGPR(r27)
	SAVE_NVGPR(r28)
	SAVE_NVGPR(r29)
	SAVE_NVGPR(r30)
	SAVE_NVGPR(r31)

	mfmsr	r31
	std	r31, MSR_OFFSET(r1)

	/* Clear 64bit mode */
	rldicl r31,r31,0,1

#if defined(_CALL_ELF) && _CALL_ELF == 2  /* LITTLE ENDIAN */
	/* Clear LE mode */
	rldicr r31,r31,0,62
#endif

	mtspr	SPRN_SRR0,r4
	mtspr	SPRN_SRR1,r31

	/* Point the LR at our return code */
	bl	1f
1:	mflr	r30
	addi	r30,r30,(2f - 1b)
	mtlr	r30

	/* Call OF */
	rfid

#if defined(_CALL_ELF) && _CALL_ELF == 2  /* LITTLE ENDIAN */
2:     .long 0x05009f42        /* bcl     20,31,$+4 */
       .long 0xa602487d        /* mflr    r10 */
       .long 0x1c004a39        /* addi    r10,r10,28 */
       .long 0xa6035a7d        /* mtsrr0  r10 */
       .long 0xa600407d        /* mfmsr   r10 */
       .long 0x01004a69        /* xori    r10,r10,1 */
       .long 0xa6035b7d        /* mtsrr1  r10 */
       .long 0x2400004c        /* rfid */
#endif

	/* Clear the top 32 bits of r1, just in case */
2:	rldicl	r1,r1,0,32

	/* Reset our MSR */
	ld	r31,MSR_OFFSET(r1)
	mtspr	SPRN_SRR1,r31

	bl	3f
3:	mflr	r30
	addi	r30,r30,(4f - 3b)
	mtspr	SPRN_SRR0,r30

	rfid

#if defined(_CALL_ELF) && _CALL_ELF == 1  /* BIG ENDIAN */
4:	ld	r2,TOC_OFFSET(r1)
#endif
4:	ld	r13,R13_OFFSET(r1)

	REST_NVGPR(r14)
	REST_NVGPR(r15)
	REST_NVGPR(r16)
	REST_NVGPR(r17)
	REST_NVGPR(r18)
	REST_NVGPR(r19)
	REST_NVGPR(r20)
	REST_NVGPR(r21)
	REST_NVGPR(r22)
	REST_NVGPR(r23)
	REST_NVGPR(r24)
	REST_NVGPR(r25)
	REST_NVGPR(r26)
	REST_NVGPR(r27)
	REST_NVGPR(r28)
	REST_NVGPR(r29)
	REST_NVGPR(r30)
	REST_NVGPR(r31)

	addi	r1,r1,STACK_FRAME_SIZE
	ld	r0,16(r1)
	mtlr	r0
	blr
FUNC_END(ieee1275_call_entry_fn)
