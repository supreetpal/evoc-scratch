
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
	do {
		*FSE->ptr.u08++ = 0x7f;
		FSE->len = FSE->ptr.u08 - FSE->data;
	} while (FSE->len & 3);
	FSE->ptr.u08 = FSE->data;
}

FSE_delay_ns(struct FSE_ucode *FSE, u64 val)
{
	
	if( val > 0xffff) {
		    *FSE->ptr.u08++ = 0x0;
		    *FSE->ptr.u32++ = (val >> 32);
		    *FSE->ptr.u32++ = (val & 0xffffffff);	  
	}    		
	
	else {
	  
	 val = val & 0xffff;
	   
	   if( val % 1000 != 0 ) {
	     
		    *FSE->ptr.u08++ = 0x01;
		    *FSE->ptr.u16++ = val % 1000;
		}
	   if( val > 999 ){
	 
		    *FSE->ptr.u08++ = 0x02;
		    *FSE->ptr.u16++ = val/1000;
		}
		    
	}
	  
}

FSE_write(struct FSE_ucode *FSE, u32 reg, u32 val)
{
	if (val & 0xff == val) {
		*FSE->ptr.u08++ = 0x11;
		*FSE->ptr.u08++ = val;
		*FSE->ptr.u32++ = reg;
		
	} else {
		*FSE->ptr.u08++ = 0x10;
		*FSE->ptr.u32++ = val;
		*FSE->ptr.u32++ = reg;
			
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
	
	while(i < size) {
	    *FSE->ptr.u08++ = msg[i];
	    i++;
	    }
}	    