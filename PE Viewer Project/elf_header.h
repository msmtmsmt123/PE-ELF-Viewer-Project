#ifndef __ELF_HEADER_H__
#define __ELF_HEADER_H__

#include "elf_type.h"
#include "pro_type.h"
#include "sh_type.h"
#define EI_NIDENT 16

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'


typedef struct {
	unsigned char EI_MAG0;
	unsigned char EI_MAG1;
	unsigned char EI_MAG2;
	unsigned char EI_MAG3;
	unsigned char EI_CLASS;
	unsigned char EI_DATA;
	unsigned char EI_VERSION;
	unsigned char EI_PAD[EI_NIDENT - 7];
} E_IDENT;

typedef struct {
	E_IDENT       e_ident;
	Elf32_Half    e_type;
	Elf32_Half    e_machine;
	Elf32_Word    e_version;
	Elf32_Addr    e_entry;
	Elf32_Off     e_phoff;
	Elf32_Off     e_shoff;
	Elf32_Word    e_flags;
	Elf32_Half    e_ehsize;
	Elf32_Half    e_phentsize;
	Elf32_Half    e_phnum;
	Elf32_Half    e_shentsize;
	Elf32_Half    e_shnum;
	Elf32_Half    e_shstrndx;
} Elf32_Ehdr;

typedef struct {
	Elf32_Ehdr    header;
} Elf32;

#endif /* __ELF_HEADER_H__ */

