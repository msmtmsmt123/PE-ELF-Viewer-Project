#ifndef __ELF_TYPE_H__
#define __ELF_TYPE_H__

typedef unsigned int Elf32_Addr;
typedef unsigned short Elf32_Half;
typedef unsigned int Elf32_Off;
typedef int Elf32_Sword;
typedef int Elf32_Word;

enum {
	ET_NONE = 0,
	ET_REL,
	ET_EXEC,
	ET_DYN,
	ET_CORE,
	ET_LOPROC = 0xff00,
	ET_HIPROC = 0xffff
} E_TYPE;

enum {
	EM_NONE = 0,
	EM_M32,
	EM_SPARC,
	EM_386,
	EM_68K,
	EM_88K,
	EM_860,
	EM_MIPS
} E_MACHINE;

enum {
	ELFCLASSNONE = 0,
	ELFCLASS32,
	ELFCLASS64
} EI_CLASS;

enum
{
	ELFDATANONE = 0,
	ELFDATA2LSB,
	ELFDATA2MSB
} EI_DATA;

enum {
	EV_NONE = 0,
	EV_CURRENT
} E_VERSION;

#endif /* __ELF_TYPE_H__ */