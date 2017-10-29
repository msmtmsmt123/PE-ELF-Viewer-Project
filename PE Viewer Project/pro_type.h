#ifndef __Phdr_TYPE_H__
#define __Phdr_TYPE_H__

typedef unsigned int Pro32_Addr;
typedef unsigned short Pro32_Half;
typedef unsigned int Pro32_Off;
typedef int Pro32_Sword;
typedef int Pro32_Word;

enum {
	PT_LOAD = 1,		/* Program Segment*/
	PT_DYNAMIC,			/* Dynamic Link infomation*/
	PT_INTERP,			/* Program Interpreter*/
	PT_NOTE,			/*  */			
	PT_TLS,				/* Thread location storage */
	PT_PHDR,			/* Program Header Table */	
	PT_GNU_EH_FRAME = 0x6474e550,/* GNU.eh_frame_hdr Segment */
	PT_GNU_STACK = 0x6474e551,		/*  */
	PT_GNU_RELRO = 0x6474e552
} P_TYPE;

#endif