
struct FSE_ucode
{
	u8 data[0x200];
	union {
		u8  *u08;
		u16 *u16;
		u32 *u32;
	} ptr;
	u16 len;	
};

static inline void
FSE_init(struct FSE_ucode *FSE)
{
	FSE->ptr.u08 = FSE->data;	
}

static inline void
FSE_fini(struct FSE_ucode *FSE)
{
	*FSE->ptr.u08++ = 0xff;
	FSE->len =FSE->ptr.u08 - FSE->data;
	FSE->ptr.u08 = FSE->data;
}

static inline void
FSE_delay_ns(struct FSE_ucode *FSE, u64 delay_ns)
{
	if (delay_ns <= 0xffff * 32) {
		*FSE->ptr.u08++ = 0x01;
		*FSE->ptr.u16++ = delay_ns / 32;
		
	} else if (delay_ns <= 0xffff * 1000) {
		/* wait some micro seconds */
		*FSE->ptr.u08++ = 0x2;
		*FSE->ptr.u16++ = delay_ns / 1000;
 
		/* complete the delay with ns */
		if (delay_ns % 1000 >= 32) {
			*FSE->ptr.u08++ = 0x1;
			*FSE->ptr.u16++ = (delay_ns % 1000) / 32;
		}
	} else {
	/* we could try harder to optimize the output, but that should
	* be sufficient since most waits will be under 65ms anyway.
	*
	* If we need to wait longer quite often, then a new opcode
	* could be added.
	*/
		*FSE->ptr.u08++ = 0x0;
		*FSE->ptr.u32++ = (delay_ns >> 32);
		*FSE->ptr.u32++ = (delay_ns & 0xffffffff);
	}
}

static inline void
FSE_write(struct FSE_ucode *FSE, u32 reg, u32 val)
{
	if ((val & 0xff) == val) {
		*FSE->ptr.u08++ = 0x11;
		*FSE->ptr.u32++ = reg;		
		*FSE->ptr.u08++ = val;
	} else {
		*FSE->ptr.u08++ = 0x10;
		*FSE->ptr.u32++ = reg;			
		*FSE->ptr.u32++ = val;
	}

}

static inline void
FSE_mask(struct FSE_ucode *FSE, u32 reg, u32 mask, u32 data)
{
	*FSE->ptr.u08++ = 0x12;
	*FSE->ptr.u32++ = reg;
	*FSE->ptr.u32++ = mask;
	*FSE->ptr.u32++ = data;  
}

static inline void
FSE_wait(struct FSE_ucode *FSE, u32 reg, u32 mask, u32 data)
{
	*FSE->ptr.u08++ = 0x13;
	*FSE->ptr.u32++ = reg;
	*FSE->ptr.u32++ = mask;
	*FSE->ptr.u32++ = data;  
}

static inline void
FSE_send_msg(struct FSE_ucode *FSE, u16 size, u8 *msg)
{
	int i = 0;
	
	*FSE->ptr.u08++ = 0x20;
	*FSE->ptr.u16++ = size;
	
	while(i < size) {
		*FSE->ptr.u08++ = msg[i];
		i++;
	}
}	

  