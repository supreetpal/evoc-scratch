#include <stdio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#include "nouveau_hwsq.h"

u32 le32 (const u8 *buf, int *off) {
	*off = *off + 4;
	return buf[*off - 4] | buf[*off - 3] << 8 | buf[*off - 2] << 16 | buf[*off - 1] << 24;
}

u16 le16 (const u8 *buf, int *off) {
	*off = *off + 2;
	return buf[*off - 2] | buf[*off - 1] << 8;
}

u8 le8 (const u8 *buf, int *off) {
	*off = *off + 1;
	return buf[(*off) - 1];
}

int main(int argc, char **argv)
{
	struct hwsq_ucode code, *ucode = &code;
	int i,j,reg,val, v0, v1, flag;

	/* create a script */
	hwsq_init(ucode);
	hwsq_wr32(ucode, 0x12345678, 0xdeadbeef);
	hwsq_op5f(ucode, 0x12, 0x34);
	hwsq_setf(ucode, 0x01, 1);
	hwsq_fini(ucode);
	
	/* print the generated code */
	printf("encoded program: ucode->len = %i bytes", ucode->len);
	for (i = 0; i < ucode->len; i++) {
		if (i % 16 == 0)
			printf("\n%08x: ", i);
		printf("%01x ", ucode->ptr.u08[i]);
	}
	printf("\n\n");
	
	/* decode */
	printf("decode program:\n");
	i = 0;
	while (i < ucode->len) {
		u8 opcode = le8(ucode->ptr.u08, &i);
		
		printf("opcode = 0x%02x\n", opcode);
		
		if (opcode>0x80 && opcode<0x9f) opcode = 1;
		if (opcode>0xa0 && opcode<0xbf) opcode = 2;
		if (opcode>0xc0 && opcode<0xdf) opcode = 3;
				  
		switch (opcode) {
			//hwsq_wr32
			case 0x42:
				val = (val & 0xffff0000) |  le16(ucode->ptr.u08, &i); 
				break;
			case 0xe2:
				val = le32(ucode->ptr.u08, &i);
				printf(" value = 0x%08x\n", val);
				break;
			case 0x40:
				reg =  (reg & 0xffff0000) |  le16(ucode->ptr.u08, &i); 
				printf("hwsq(0x%x, 0x%x\n", reg, val); 
				break;
			case 0xe0:
				reg = le32(ucode->ptr.u08, &i);
				printf("Reg value=0x%08x\n", reg);
				printf("hwsq_wr32( 0x%08x, 0x%08x)\n", reg, val);
				break;
						
			//op5f
			case 0x5f:
				v0 = le8(ucode->ptr.u08, &i);
				v1 = le8(ucode->ptr.u08, &i);
				printf(" hwsq_op5f( 0x%02x, 0x%02x)\n", v0, v1);
				break;
				
			//setf
			case 1:
			      i--;
			      flag = le8(ucode->ptr.u08, &i);
			      flag -= 0x80;
			      printf("flag = 0x%02x\n", flag);
			      break;
			    			      
			case 2: 
			      i--;
			      flag = le8(ucode->ptr.u08, &i);
			        
			      flag -= 0x80;
			      flag -= 0x20;
			      
			      printf("flag = 0x%02x; val >= 0\n", flag);
			      break;
			      
			case 3:
			      i--;
			      flag = le8(ucode->ptr.u08, &i);
			        
			      flag -= 0x80;
			      flag -= 0x20;
			      flag -= 0x20;
			      
			      printf("flag = 0x%02x; val >=1\n", flag);
			      break;
			      
			case 0x7f:
				printf("exit\n");
				break;
				
			default:
				printf("unknown opcode %1x\n", opcode);
				break;
		}
					
	}
	
	
	return 0;
}
