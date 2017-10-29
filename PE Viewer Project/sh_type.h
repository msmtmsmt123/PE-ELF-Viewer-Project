#ifndef __SH_TYPE_H__
#define __SH_TYPE_H__

typedef unsigned int Sh32_Addr;
typedef unsigned short Sh32_Half;
typedef unsigned int Sh32_Off;
typedef int Sh32_Sword;
typedef int Sh32_Word;

enum { 
	Null=0,
	PROGBITS,
	SYMTAB,
	STRTAB,
	RELA,
	HASH,
	DYNAMIC,
	NOTE,
	NOBITS,
	REL,
	EYNSYM,
	INIT_ARRAY,
	FINI_ARRAY,
	GNU_verderf = 0x6ffffffd,
	GNU_verneed = 0x6ffffffe,
	GNU_versym = 0x6fffffff
}S_TYPE;

#endif