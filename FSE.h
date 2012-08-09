#ifndef __PDAEMON_ISA_H__
#define __PDAEMON_ISA_H__

struct FSE_ucode
{
	u8 data[0x200];
	union {
		u8  *u08;
		u16 *u16;
		u32 *u32;
	} ptr;
	u16 len;
	
	u32 reg;
	u32 val;
};

static inline void
FSE_init(struct FSE_ucode *FSE)
{
	FSE->ptr.u08 = FSE->data;
	FSE->reg = 0xffffffff;
	FSE->val = 0xffffffff;
}

static inline void
FSE_fini(struct FSE_ucode *FSE)
{
	do {
		*FSE->ptr.u08++ = 0x7f;
		FSE->len = FSE->ptr.u08 - hwsq->data;
	} while (FSE->len & 3);
	FSE->ptr.u08 = FSE->data;
}

FSE_delay()
{


}

FSE_write(struct FSE_ucode *FSE, u32 reg, u32 val)
{
	if (val != FSE->val) {
		if ((val & 0xffff0000) != 0x00000000)) {
			*FSE->ptr.u08++ = 0x11;
			*FSE->ptr.u08++ = val;
			*FSE->ptr.u32++ = reg;
		  
		} else {
			*FSE->ptr.u08++ = 0x10;
			*FSE->ptr.u32++ = val;
			*FSE->ptr.u32++ = reg;
		}

		FSE->val = val;
		FSE->reg = reg;
	}

	
}

FSE_mask()
{

}

FSE_wait()
{

}

FSE_send_msg(struct FSE_ucode *FSE, u16 size, u8 *msg)
{
	int i = 0;
	
	*FSE->ptr.u08++ = 0x20;
	*FSE->ptr.u16++ = size;
	
	while(size != 0){
	    *FSE->ptr.u08++ = msg[i];
	    i++;
	    size--;
	    }
}	    