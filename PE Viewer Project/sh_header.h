#ifndef __Sh_TYPE_H__
#define __Sh_TYPE_H__
#include "sh_type.h"

typedef struct {
	Sh32_Word	sh_name;		/* Section name (string tbl index) */
	Sh32_Word	sh_type;		/* Section type */
	Sh32_Word	sh_flags;		/* Section flags */
	Sh32_Addr	sh_addr;		/* Section virtual addr at exection */
	Sh32_Off	sh_offset;		/* Section file offset */
	Sh32_Word	sh_size;		/* Section size in bytes */
	Sh32_Word	sh_link;		/* Link to anther section */
	Sh32_Word	sh_info;		/* Additional section inforamtion */
	Sh32_Word	sh_addralign;	/* Section alignment */
	Sh32_Word	sh_entsize;		/* Entry size if section holds table */
}Sh32_shdr;

#endif
