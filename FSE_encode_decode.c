#include <stdio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#include "FSE.h"

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
	struct FSE_ucode code, *ucode = &code;
	int i,size;
	
	/* create a script */
	FSE_init(ucode);
	FSE_write(ucode, 0x12345678, 0xdeadbeef);
	FSE_write(ucode, 0x12345678, 0xef);
	//FSE_send_msg(ucode, 5, msg);
	FSE_fini(ucode);
	
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
		switch (opcode) {
		  
			case 0x01:
				  printf("FSE_delay(0x%08x)\n",le32(ucode->ptr.u08, &i));
				  
			case 0x02:
				  printf("FSE_delay(0x%08x)\n",le32(ucode->ptr.u08, &i));
				  
			case 0x10:
				printf("FSE_write(0x%08x, 0x%02x);\n",le32(ucode->ptr.u08, &i), le32(ucode->ptr.u08, &i));
				break;
			
			case 0x11:
				printf("FSE_write(0x%08x, 0x%08x);\n",le8(ucode->ptr.u08, &i), le32(ucode->ptr.u08, &i));
				break;
			
			case 0x20:
				size = le16(ucode->ptr.u08, &i);
				printf(" FSE_send_msg(0x%08x,", size);
				while(size)
				{
				      printf("0x%08x ,", le8(ucode->ptr.u08, &i));      
				      size--;
				}
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
