/* dl.c - arch-dependent part of loadable module support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2004,2005,2007,2009  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/elf.h>
#include <grub/misc.h>
#include <grub/err.h>
#include <grub/i18n.h>
#include <grub/powerpc/reloc.h>

#if defined( __powerpc64__ ) || defined( __powerpc64le__ )
#define ELFCLASSXX ELFCLASS64
#define ELFMACHINEXX EM_PPC64
#else
#define ELFCLASSXX ELFCLASS32
#define ELFMACHINEXX EM_PPC
#endif

#if defined( __powerpc64le__ )
#define ELFDATA2XSB ELFDATA2LSB
#else
#define ELFDATA2XSB ELFDATA2MSB
#endif

/* Check if EHDR is a valid ELF header.  */
grub_err_t
grub_arch_dl_check_header (void *ehdr)
{
  Elf_Ehdr *e = ehdr;

  /* Check the magic numbers.  */
  if (e->e_ident[EI_CLASS] != ELFCLASSXX
      || e->e_ident[EI_DATA] != ELFDATA2XSB
      || e->e_machine != ELFMACHINEXX)
    return grub_error (GRUB_ERR_BAD_OS, N_("invalid arch-dependent ELF magic"));

  return GRUB_ERR_NONE;
}




#if defined( __powerpc64le__ )
struct trampoline
{
  grub_uint32_t std;
  grub_uint32_t addis;
  grub_uint32_t addi;
  grub_uint32_t clrldi;
  grub_uint32_t mtctr;
  grub_uint32_t bctr;
};

static const struct trampoline trampoline_template =
  {
       0xf8410018,       /* std     r2,24(r1) */
       0x3d800000,       /* addis   r12,0,0 */
       0x398c0000,       /* addi    r12,r12,0 */
       0x798c0020,       /* clrldi  r12,r12,32 */
       0x7d8903a6,       /* mtctr   r12 */
       0x4e800420,       /* bctr */
  };

#define PPC_NOP 0x60000000
#define	RESTORE_TOC 0xe8410018       /* ld      r2,24(r1) */

#define STO_PPC64_LOCAL_BIT             5
#define STO_PPC64_LOCAL_MASK            (7 << STO_PPC64_LOCAL_BIT)

static unsigned long grub_arch_dl_get_toc (grub_dl_t mod, void *ehdr)
{
  unsigned long i = (unsigned long)grub_dl_find_section_addr(mod, ehdr, ".toc");
  if (!i)
    return 0;

  return i;
}

static inline unsigned int
ppc64_decode_local_entry(unsigned int other)
{
  return ((1 << other) >> 2) << 2;
}

#define PPC64_LOCAL_ENTRY_OFFSET(other)                         \
  ppc64_decode_local_entry (((other) & STO_PPC64_LOCAL_MASK)    \
                            >> STO_PPC64_LOCAL_BIT)



#elif defined( __powerpc64__ )

#error "NOT IMPLEMENTED YET"

static int grub_arch_dl_is_in_opd (grub_dl_t mod, void *ehdr, unsigned long addr)
{
  unsigned long start, end;
  Elf_Shdr *s = grub_dl_find_section(ehdr, ".opd");

  if (!s)
	return 0;

  start = (unsigned long)grub_dl_find_section_addr(mod, ehdr, ".opd");
  end = start + s->sh_size;

  if ((start <= addr) && (addr < end))
    return 1;
  else
    return 0;
}

#else

/* For low-endian reverse lis and addr_high as well as ori and addr_low. */
struct trampoline
{
  grub_uint32_t lis;
  grub_uint32_t ori;
  grub_uint32_t mtctr;
  grub_uint32_t bctr;
};

static const struct trampoline trampoline_template =
  {
    0x3d800000,
    0x618c0000,
    0x7d8903a6,
    0x4e800420,
  };

#endif

#pragma GCC diagnostic ignored "-Wcast-align"

grub_err_t
grub_arch_dl_get_tramp_got_size (const void *ehdr, grub_size_t *tramp,
				 grub_size_t *got)
{
  const Elf_Ehdr *e = ehdr;
  const Elf_Shdr *s;
  unsigned i;

  *tramp = 0;
  *got = 0;

  for (i = 0, s = (const Elf_Shdr *) ((const char *) e + e->e_shoff);
       i < e->e_shnum;
       i++, s = (const Elf_Shdr *) ((const char *) s + e->e_shentsize))
    if (s->sh_type == SHT_RELA)
      {
	const Elf_Rela *rel, *max;

	for (rel = (const Elf_Rela *) ((const char *) e + s->sh_offset),
	       max = rel + s->sh_size / s->sh_entsize;
	     rel < max;
	     rel++)
	  if (ELF_R_TYPE (rel->r_info) == GRUB_ELF_R_PPC_REL24)
	    (*tramp)++;
      }

  *tramp *= sizeof (struct trampoline);

  return GRUB_ERR_NONE;
}

/* Relocate symbols.  */
grub_err_t
grub_arch_dl_relocate_symbols (grub_dl_t mod, void *ehdr,
			       Elf_Shdr *s, grub_dl_segment_t seg)
{
  Elf_Rela *rel, *max;

  for (rel = (Elf_Rela *) ((char *) ehdr + s->sh_offset),
	 max = (Elf_Rela *) ((char *) rel + s->sh_size);
       rel < max;
       rel = (Elf_Rela *) ((char *) rel + s->sh_entsize))
    {
      Elf_Word *addr;
      Elf_Sym *sym;
      Elf_Addr value;

      if (seg->size < rel->r_offset)
	return grub_error (GRUB_ERR_BAD_MODULE,
			   "reloc offset is out of the segment");

      addr = (Elf_Word *) ((char *) seg->addr + rel->r_offset);
      sym = (Elf_Sym *) ((char *) mod->symtab
			 + mod->symsize * ELF_R_SYM (rel->r_info));

      /* On the PPC the value does not have an explicit
	 addend, add it.  */
      value = sym->st_value + rel->r_addend;
      switch (ELF_R_TYPE (rel->r_info))
	{
#ifdef __powerpc64le__
	case GRUB_ELF_R_PPC_REL24:
	  {
	    struct trampoline *tptr = mod->trampptr;
	    Elf_Sword delta;
	    if (sym->st_shndx == SHN_UNDEF)
	    {
                grub_memcpy (tptr, &trampoline_template, sizeof (*tptr));

		tptr->addis |= PPC_HA(value);
		tptr->addi |= PPC_LO(value);

	        mod->trampptr = tptr + 1;
	        delta = (grub_uint8_t *) tptr - (grub_uint8_t *) addr;

	        if (*(addr+1) != PPC_NOP)
	               return grub_error (GRUB_ERR_BAD_MODULE,
			     "Missing NOP after PPC_REL24 got %x", *(addr+1));
	        *(addr+1) = RESTORE_TOC;
	    } else
	        delta = (grub_uint8_t *)value - (grub_uint8_t *) addr +
                     PPC64_LOCAL_ENTRY_OFFSET(sym->st_other);


            if (delta << 6 >> 6 != delta)
		      return grub_error (GRUB_ERR_BAD_MODULE,
					 "relocation overflow");

	    *(Elf_Word *) (addr) = (*addr & ~0x03fffffc) | (delta & 0x03fffffc);
	  }
	  break;

	case GRUB_ELF_R_PPC64_ADDR64:
	  *(Elf_Xword *) addr = value;
	  break;

	case GRUB_ELF_R_PPC64_TOC:
	  *(Elf_Xword *) addr = grub_arch_dl_get_toc(mod, ehdr);
	  break;

	case GRUB_ELF_R_PPC64_TOC16_HA:
          value -= grub_arch_dl_get_toc(mod, ehdr);
          *(Elf_Half *) addr = PPC_HA(value);
	  break;

	case GRUB_ELF_R_PPC64_TOC16_LO:
          value -= grub_arch_dl_get_toc(mod, ehdr);
	  *(Elf_Half *) addr = PPC_LO(value);
	  break;

	case GRUB_ELF_R_PPC64_TOC16_LO_DS:
	   value -= grub_arch_dl_get_toc(mod, ehdr);
	   if (value & 3)
	    return grub_error (GRUB_ERR_BAD_MODULE,
			       "bad TOC16_LO_DS relocation");

	  *(Elf_Half *) addr = ((*(Elf_Half *) addr) & ~0xfffc) | (value & 0xfffc);
	  break;

	case GRUB_ELF_R_PPC64_REL16_HA:
	  value -=  (unsigned long) addr;
	  *(Elf_Half *) addr = PPC_HA(value);
	  break;

	case GRUB_ELF_R_PPC64_REL16_LO:
	  value -=  (unsigned long) addr;
	  *(Elf_Half *) addr = PPC_LO(value);
	  break;
#else

	case GRUB_ELF_R_PPC_ADDR16_LO:
	  *(Elf_Half *) addr = value;
	  break;

	case GRUB_ELF_R_PPC_REL24:
	  {
	    Elf_Sword delta = value - (Elf_Word) addr;

	    if (delta << 6 >> 6 != delta)
	      {
		struct trampoline *tptr = mod->trampptr;
		grub_memcpy (tptr, &trampoline_template,
			     sizeof (*tptr));
		delta = (grub_uint8_t *) tptr - (grub_uint8_t *) addr;
		tptr->lis |= (((value) >> 16) & 0xffff);
		tptr->ori |= ((value) & 0xffff);
		mod->trampptr = tptr + 1;
	      }

	    if (delta << 6 >> 6 != delta)
	      return grub_error (GRUB_ERR_BAD_MODULE,
				 "relocation overflow");
	    *addr = (*addr & 0xfc000003) | (delta & 0x3fffffc);
	    break;
	  }

	case GRUB_ELF_R_PPC_ADDR16_HA:
	  *(Elf_Half *) addr = (value + 0x8000) >> 16;
	  break;

	case GRUB_ELF_R_PPC_ADDR32:
	  *addr = value;
	  break;

	case GRUB_ELF_R_PPC_REL32:
	  *addr = value - (Elf_Word) addr;
	  break;
#endif

	default:
	  return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
			     N_("relocation 0x%x is not implemented yet"),
			     ELF_R_TYPE (rel->r_info));
	}
    }

  return GRUB_ERR_NONE;
}
