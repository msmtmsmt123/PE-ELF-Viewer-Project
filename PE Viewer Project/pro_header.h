#ifndef __Pro_HEADER_H__
#define __Pro_HEADER_H__
#include "pro_type.h"

typedef struct {
	Pro32_Word	p_type;			/* Segment type */
	Pro32_Off	p_offset;		/* Segment file offset */
	Pro32_Addr  p_vaddr;		/* Segment vitual address */
	Pro32_Addr	p_paddr;		/* Segment physical address */
	Pro32_Word	p_filesz;		/* Segment size in file*/
	Pro32_Word	p_memsz;		/* Segment size in memory */
	Pro32_Word	p_flags;		/* Segment flags */
	Pro32_Word	p_align;		/* Segment alignment */
}Pro32_Phdr;

#endif