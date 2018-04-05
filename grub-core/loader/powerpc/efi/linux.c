/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2010  Free Software Foundation, Inc.
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

#include <grub/loader.h>
#include <grub/file.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/command.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/kernel.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/elf.h>
#include <grub/fdt.h>
#include <grub/efi/fdtload.h>
#include <grub/linux.h>

GRUB_MOD_LICENSE ("GPLv3+");

void *fdt;

static grub_dl_t my_mod;

static int loaded;

/* Kernel base and size.  */
static void *kernel_mem;
static grub_efi_uintn_t kernel_pages;
static grub_efi_uintn_t entry;

/* Initrd base and size.  */
static grub_addr_t initrd_mem;
static grub_efi_uintn_t initrd_pages;
static grub_efi_uintn_t initrd_size;

static inline grub_size_t
page_align (grub_size_t size)
{
  return (size + (1 << 12) - 1) & (~((1 << 12) - 1));
}

static grub_err_t
prepare_fdt(void)
{
  int node, retval;

  fdt = grub_fdt_load (0x400);

  if (!fdt)
    goto failure;

  node = grub_fdt_find_subnode (fdt, 0, "chosen");
  if (node < 0)
    node = grub_fdt_add_subnode (fdt, 0, "chosen");

  if (node < 1)
    goto failure;

  /* Set initrd info */
  if (initrd_mem && initrd_size > 0)
    {
      grub_printf ("Initrd @ %p-0x%lu\n",
                   (void *)(grub_addr_t)initrd_mem, initrd_size);

      retval = grub_fdt_set_prop64 (fdt, node, "linux,initrd-start",
                                    (grub_addr_t) initrd_mem);
      if (retval)
        goto failure;
      retval = grub_fdt_set_prop64 (fdt, node, "linux,initrd-end",
                                    (grub_addr_t) initrd_mem + initrd_size);
      if (retval)
        goto failure;
    }

  if (grub_fdt_install() != GRUB_ERR_NONE)
    goto failure;

  return GRUB_ERR_NONE;

failure:
  grub_fdt_unload();
  return grub_error(GRUB_ERR_BAD_OS, "failed to install/update FDT");

}

/* Find the optimal number of pages for the memory map. Is it better to
   move this code to efi/mm.c?  */
static grub_efi_uintn_t
find_mmap_size (void)
{
  static grub_efi_uintn_t mmap_size = 0;

  if (mmap_size != 0)
    return mmap_size;

  mmap_size = (1 << 12);
  while (1)
    {
      int ret;
      grub_efi_memory_descriptor_t *mmap;
      grub_efi_uintn_t desc_size;

      mmap = grub_malloc (mmap_size);
      if (! mmap)
	return 0;

      ret = grub_efi_get_memory_map (&mmap_size, mmap, 0, &desc_size, 0);
      grub_free (mmap);

      if (ret < 0)
        {
	  grub_error (GRUB_ERR_IO, "cannot get memory map");
	  return 0;
	}
      else if (ret > 0)
	break;

      mmap_size += (1 << 12);
    }

  /* Increase the size a bit for safety, because GRUB allocates more on
     later, and EFI itself may allocate more.  */
  mmap_size += (1 << 12);

  return page_align (mmap_size);
}

static void
free_pages (void)
{
  if (kernel_mem)
    {
      grub_efi_free_pages ((grub_addr_t) kernel_mem, kernel_pages);
      kernel_mem = 0;
    }

  if (initrd_mem)
    {
      grub_efi_free_pages ((grub_addr_t) initrd_mem, initrd_pages);
      initrd_mem = 0;
    }

  grub_fdt_unload ();
}

static grub_err_t
grub_linux_boot (void)
{
  typedef  void (*LINUX_ENTRY)(void *Fdt, grub_efi_uintn_t r4, grub_efi_uintn_t r5, grub_efi_uintn_t r6, grub_efi_uintn_t r7);
  grub_efi_uintn_t mmap_size;
  grub_efi_uintn_t map_key;
  grub_efi_uintn_t desc_size;
  grub_efi_uint32_t desc_version;
  grub_efi_memory_descriptor_t *mmap_buf;
  grub_err_t err;
  LINUX_ENTRY Entry = (LINUX_ENTRY) entry;

  grub_dprintf ("linux", "Jump to 0x%lx\n", entry);

  /* MDT.
     Must be done after grub_machine_fini because map_key is used by
     exit_boot_services.  */
  mmap_size = find_mmap_size ();
  if (! mmap_size)
    return grub_errno;
  mmap_buf = grub_efi_allocate_any_pages (page_align (mmap_size) >> 12);
  if (! mmap_buf)
    return grub_error (GRUB_ERR_IO, "cannot allocate memory map");
  err = grub_efi_finish_boot_services (&mmap_size, mmap_buf, &map_key,
				       &desc_size, &desc_version);
  if (err)
    return err;

  asm volatile("mtmsrd %0,%1" : : "r"(0x8000000000000000UL >> (62)), "i"(1) : "memory");
  /* See you next boot.  */

  Entry(fdt, 0, 0, 0x65504150, 0x30000000);

  /* Never reach here.  */
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_unload (void)
{
  free_pages ();
  grub_dl_unref (my_mod);
  loaded = 0;
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_load_elf64 (grub_addr_t LinuxImage)
{
  Elf64_Ehdr *kh = (Elf64_Ehdr *) LinuxImage;
  Elf64_Phdr *ph;
  int i;

  if (kh->e_ident[EI_MAG0] != ELFMAG0
      || kh->e_ident[EI_MAG1] != ELFMAG1
      || kh->e_ident[EI_MAG2] != ELFMAG2
      || kh->e_ident[EI_MAG3] != ELFMAG3
      || kh->e_ident[EI_DATA] != ELFDATA2LSB) {
    grub_printf ("elf error 0\n ");
    return grub_error(GRUB_ERR_UNKNOWN_OS,
                      N_("invalid arch-independent ELF magic"));
  }
  if (kh->e_ident[EI_CLASS] != ELFCLASS64
      || kh->e_version != EV_CURRENT
      || kh->e_machine != EM_PPC64) {
    grub_printf ("elf error 1\n ");
    return grub_error (GRUB_ERR_UNKNOWN_OS,
                       N_("invalid arch-dependent ELF magic"));
  }

  if (kh->e_type != ET_EXEC) {
    grub_printf ("elf error 2\n ");
    return grub_error (GRUB_ERR_UNKNOWN_OS,
                       N_("this ELF file is not of the right type"));
  }

  entry = 0;
  /* Compute low, high and align addresses.  */
  for (i = 0; i < kh->e_phnum; i++)
    {
      ph = (Elf64_Phdr *) (LinuxImage + kh->e_phoff
                             + i * kh->e_phentsize);
      if (ph->p_type != PT_LOAD)
          continue;

      if ((ph->p_vaddr > kh->e_entry) ||
          (ph->p_vaddr + ph->p_memsz) < kh->e_entry)
                  continue;

      entry = LinuxImage + kh->e_entry - ph->p_vaddr + ph->p_offset;
    }

  grub_printf ("linux loaded at  0x%lx\n ", (long unsigned int) LinuxImage);
  grub_printf ("linux entry point at 0x%lx\n ", entry);

  return 0;
}


static grub_err_t
grub_cmd_linux (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  grub_file_t file = 0;
  grub_addr_t kimage;
  grub_ssize_t len;
  int ksize;
  int ksize_pages;

  grub_dl_ref (my_mod);

  grub_loader_unset ();

  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }

  file = grub_file_open (argv[0]);
  if (! file)
    goto fail;

  ksize = grub_file_size (file);
  ksize_pages = page_align (ksize) >> 12;
  kimage = (grub_addr_t) grub_efi_allocate_any_pages (ksize_pages);

  len = grub_file_read (file, (void *) kimage, ksize);
  if (len < (grub_ssize_t) sizeof (Elf64_Ehdr))
    {
      if (!grub_errno)
	grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),
		    argv[0]);
      goto fail;
    }

  grub_dprintf ("linux", "Loading linux: %s\n", argv[0]);
  if (grub_load_elf64 (kimage))
    goto fail;

  grub_errno = GRUB_ERR_NONE;

  loaded = 1;

  grub_loader_set (grub_linux_boot, grub_linux_unload, 0);

 fail:
  if (file)
    grub_file_close (file);

  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_dl_unref (my_mod);
    }
  return grub_errno;
}

static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  struct grub_linux_initrd_context initrd_ctx = { 0, 0, 0 };

  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }

  if (! loaded)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("you need to load the kernel first"));
      goto fail;
    }

  if (grub_initrd_init (argc, argv, &initrd_ctx))
    goto fail;

  initrd_size = grub_get_initrd_size (&initrd_ctx);
  grub_dprintf ("linux", "Loading initrd\n");

  initrd_pages = (page_align (initrd_size) >> 12);
  initrd_mem = (grub_addr_t) grub_efi_allocate_any_pages (initrd_pages);
  if (! initrd_mem)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "cannot allocate pages");
      goto fail;
    }

  grub_dprintf ("linux", "[addr=0x%lx, size=0x%lx]\n",
		(grub_uint64_t) initrd_mem, initrd_size);

  if (grub_initrd_load (&initrd_ctx, argv, (void *)initrd_mem))
    goto fail;

 prepare_fdt();
 fail:
  grub_initrd_close (&initrd_ctx);
  return grub_errno;
}

static grub_command_t cmd_linux, cmd_initrd;

GRUB_MOD_INIT(linux)
{
  cmd_linux = grub_register_command ("linux", grub_cmd_linux,
				     N_("FILE [ARGS...]"), N_("Load Linux."));

  cmd_initrd = grub_register_command ("initrd", grub_cmd_initrd,
				      N_("FILE"), N_("Load initrd."));
  my_mod = mod;
}

GRUB_MOD_FINI(linux)
{
  grub_unregister_command (cmd_linux);
  grub_unregister_command (cmd_initrd);
}
