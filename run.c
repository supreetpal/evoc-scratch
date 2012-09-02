#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include "nva.h"
#include "nva3_pdaemon.fuc.h"
#include "nvd9_pdaemon.fuc.h"

typedef uint64_t ptime_t;

typedef enum { false = 0, true = 1} bool;
typedef enum { get = 0, set = 1} resource_op;

#define PDAEMON_DISPATCH_FENCE 0x00000500
#define PDAEMON_DISPATCH_RING 0x00000550
#define PDAEMON_DISPATCH_DATA 0x00000590
#define PDAEMON_DISPATCH_DATA_SIZE 0x00000370
#define RDISPATCH_SIZE 0x00000100

#define NV04_PTIMER_TIME_0                                 0x00009400
#define NV04_PTIMER_TIME_1                                 0x00009410
ptime_t get_time(unsigned int card)
{
	ptime_t low;

	/* From kmmio dumps on nv28 this looks like how the blob does this.
	* It reads the high dword twice, before and after.
	* The only explanation seems to be that the 64-bit timer counter
	* advances between high and low dword reads and may corrupt the
	* result. Not confirmed.
	*/
	ptime_t high2 = nva_rd32(card, NV04_PTIMER_TIME_1);
	ptime_t high1;
	do {
		high1 = high2;
		low = nva_rd32(card, NV04_PTIMER_TIME_0);
		high2 = nva_rd32(card, NV04_PTIMER_TIME_1);
	} while (high1 != high2);
	return ((((ptime_t)high2) << 32) | (ptime_t)low);
}

static bool data_segment_read(unsigned int cnum, uint16_t base, uint16_t length, uint8_t *buf)
{
	uint32_t aligned_base, base_shift;
	uint16_t i = 0;

	aligned_base = base & 0xfffc;
	base_shift = (base - aligned_base) * 8;

	nva_wr32(cnum,0x10a1c8, 0x02000000 | aligned_base);
	while(i < length) {
		uint32_t data = nva_rd32(cnum, 0x10a1cc);

		uint16_t shift;
		do {
			shift = (base_shift + (i * 8)) % 32;
			buf[i] = (data >> shift) & 0xff;
			i++;
		} while (i < length && shift < 24);
	}

	return true;
}

static void data_segment_dump(unsigned int cnum, uint16_t base, uint16_t length)
{
	uint32_t reg, i;

	base &= 0xfffc;

	printf("Data segment dump: base = %x, length = %x", base, length);
	nva_wr32(cnum,0x10a1c8, 0x02000000 | base);
	for (i = 0; i < length / 4; i++) {
		if (i % 4 == 0)
			printf("\n%08x: ",  base + i * 4);
		reg = nva_rd32(cnum, 0x10a1cc);
		printf("%08x ",  reg);
	}
	printf("\n");
}

static void data_segment_upload_u32(unsigned int cnum, uint16_t base,
				uint32_t *data, uint16_t length)
{
	uint32_t i;
	base &= 0xfffc; /* make sure it is 32-bits aligned */

	if (!data)
		return;

	nva_wr32(cnum,0x10a1c8, 0x01000000 | base);
	for (i = 0; i < length; i++)
		nva_wr32(cnum, 0x10a1cc, data[i]);
}

static void data_segment_upload_u8(unsigned int cnum, uint16_t base,
				uint8_t *data, uint16_t length)
{
	uint32_t i, tmp = 0;
	base &= 0xfffc; /* make sure it is 32 bits aligned */

	if (!data)
		return;

	nva_wr32(cnum,0x10a1c8, 0x01000000 | base);
	for (i = 0; i < length; i++) {
		if (i > 0 && i % 4 == 0) {
			nva_wr32(cnum, 0x10a1cc, tmp);
			tmp = 0;
		}
		tmp |= (data[i] << (i % 4) * 8);
	}
	nva_wr32(cnum, 0x10a1cc, tmp);
}

static void pdaemon_upload(unsigned int cnum) {
	int i;
	uint32_t code_size, max_code_size, max_data_size;
	uint32_t *code;

	/* reboot PDAEMON */
	if (nva_cards[cnum].chipset > 0xc0)
		nva_mask(cnum, 0x200, 0x2000, 0);
	nva_mask(cnum, 0x022210, 0x1, 0x0);
	usleep(1000);
	nva_mask(cnum, 0x022210, 0x1, 0x1);
	if (nva_cards[cnum].chipset > 0xc0)
		nva_mask(cnum, 0x200, 0x2000, 0x2000);
	nva_wr32(cnum, 0x10a014, 0xffffffff); /* disable all interrupts */

	/* data upload */
	if (nva_cards[cnum].chipset < 0xd9) {
		data_segment_upload_u32(cnum, 0, nva3_pdaemon_data,
			  sizeof(nva3_pdaemon_data)/sizeof(*nva3_pdaemon_data));
	} else {
		data_segment_upload_u32(cnum, 0, nvd9_pdaemon_data,
			  sizeof(nvd9_pdaemon_data)/sizeof(*nvd9_pdaemon_data));
	}

	/* Writing test data to 0xd00 */
 	uint8_t buffer[0x100];

	for ( i = 0; i < 0x100; i++)
	    buffer[i] = i;
	data_segment_upload_u8(cnum, 0xd00, buffer, 0x100);

	/* code upload */
	if (nva_cards[cnum].chipset < 0xd9) {
		code_size = sizeof(nva3_pdaemon_code)/sizeof(*nva3_pdaemon_code);
		code = nva3_pdaemon_code;
	} else {
		code_size = sizeof(nvd9_pdaemon_code)/sizeof(*nvd9_pdaemon_code);
		code = nvd9_pdaemon_code;
	}
	nva_wr32(cnum, 0x10a180, 0x01000000);
	for (i = 0; i < code_size; ++i) {
		if (i % 64 == 0)
			nva_wr32(cnum, 0x10a188, i >> 6);
		nva_wr32(cnum, 0x10a184, code[i]);
	}

	/* launch */
	nva_wr32(cnum, 0x10a104, 0x0);
	nva_wr32(cnum, 0x10a10c, 0x0);
	nva_wr32(cnum, 0x10a100, 0x2);

	max_code_size = (nva_rd32(cnum, 0x10a0108) & 0x1ff) << 8;
	max_data_size = (nva_rd32(cnum, 0x10a0108) & 0x1fe00) >> 1;

	if (nva_cards[cnum].chipset < 0xd9) {
		printf("Uploaded pdaemon microcode: data = 0x%lx bytes(%li%%), code = 0x%lx bytes(%li%%)\n",
			sizeof(nva3_pdaemon_data),
		       (sizeof(nva3_pdaemon_data) * 100) / max_data_size,
			sizeof(nva3_pdaemon_code),
		       (sizeof(nva3_pdaemon_code) * 100) / max_code_size);
	} else {
		printf("Uploaded pdaemon microcode: data = 0x%lx bytes(%li%%), code = 0x%lx bytes(%li%%)\n",
			sizeof(nvd9_pdaemon_data),
		        (sizeof(nvd9_pdaemon_data) * 100) / max_data_size,
			sizeof(nvd9_pdaemon_code),
			(sizeof(nvd9_pdaemon_code) * 100) / max_data_size);
	}
}

static void pdaemon_RB_state_dump(unsigned int cnum)
{
	uint32_t fence = 0;
	data_segment_read(cnum, PDAEMON_DISPATCH_FENCE, 4, (uint8_t*)(&fence));

	printf("PDAEMON's FIFO 0 state: Get(%08x) Put(%08x) Fence(%08x)\n",
	       nva_rd32(cnum, 0x10a4b0), nva_rd32(cnum, 0x10a4a0), fence);
	data_segment_dump(cnum, PDAEMON_DISPATCH_RING, PDAEMON_DISPATCH_DATA - PDAEMON_DISPATCH_RING);
	printf("\n");
}

struct pdaemon_resource_command {
	/* in */
	uint8_t pid;
	uint32_t query_header;
	uint8_t *data;
	uint16_t data_length;

	/* out */
	uint32_t fence;
	uint16_t data_addr;
};

static bool pdaemon_send_cmd(unsigned int cnum, struct pdaemon_resource_command *cmd)
{
	static uint16_t data_base[16] = { 0 };
	static uint32_t fence = 0;
	uint32_t fence_get = 0;
	uint16_t dispatch_ring_base_addr = PDAEMON_DISPATCH_RING;
	uint16_t dispatch_data_base_addr = PDAEMON_DISPATCH_DATA;
	uint16_t dispatch_data_size = PDAEMON_DISPATCH_DATA_SIZE;

	uint32_t put = nva_rd32(cnum, 0x10a4a0);
	uint32_t next_put = dispatch_ring_base_addr + ((put - dispatch_ring_base_addr + 4) % 0x40);
	uint32_t get = nva_rd32(cnum, 0x10a4b0);

	uint8_t put_index = (put - dispatch_ring_base_addr) / 4;
	uint8_t next_put_index = (next_put - dispatch_ring_base_addr) / 4;
	uint8_t get_index = (get - dispatch_ring_base_addr) / 4;

	uint32_t header = ((cmd->pid & 0xf) << 28);
	uint32_t length = cmd->data_length;
	uint32_t data_header_length = 0;

	if (cmd->query_header > 0) {
		data_header_length = 4;
		length += data_header_length;
	}

	/* find some available space */
	if (length > dispatch_data_size)
		return false;
	else if ((dispatch_data_size - data_base[put_index]) > length)
		data_base[next_put_index] = data_base[put_index] + length;
	else {
		data_segment_read(cnum, PDAEMON_DISPATCH_FENCE, 4, (uint8_t*)(&fence_get));

		/* there is not enough space available between the current position
		 * and the end of the buffer. We need to rewind to the begining of
		 * the buffer then wait for enough space to be available.
		 */
		printf("pdaemon_send_cmd: running out of data space, "
			"waiting on fifo command 0x%x to finish: "
			"PDAEMON's FIFO 0 state: Get(%08x) Put(%08x) Fence(%08x)\n",
		       get_index, nva_rd32(cnum, 0x10a4b0), nva_rd32(cnum, 0x10a4a0), fence_get);

		do {
			get = nva_rd32(cnum, 0x10a4b0);
			get_index = (get - dispatch_ring_base_addr) / 4;
		} while (data_base[get_index] < length);

		data_base[put_index] = 0;
		data_base[next_put_index] = length;
	}

	/* align data_base[next_put_index] */
	if ((data_base[next_put_index] % 4) != 0)
		data_base[next_put_index] = (data_base[next_put_index] / 4 * 4) + 4;

	/* generate the header */
	header |= ((length & 0xfff) << 16) | (dispatch_data_base_addr & 0xffff);
	header += data_base[put_index];

	/* bump the fence */
	fence++;

	/* fill the command structure */
	cmd->fence = fence;
	cmd->data_addr = dispatch_data_base_addr + data_base[put_index] + data_header_length;

	/* copy the query header to the available space */
	data_segment_upload_u32(cnum,
			    dispatch_data_base_addr + data_base[put_index],
			    &cmd->query_header, 1);

	/* copy the data to the available space */
	if (cmd->data)
		data_segment_upload_u8(cnum, cmd->data_addr, cmd->data, cmd->data_length);

	/* wait for some space in the ring buffer */
	while (next_put == nva_rd32(cnum, 0x10a4b0));

	/* push the commands */
	data_segment_upload_u32(cnum, put, &header, 1);
	nva_wr32(cnum, 0x10a4a0, next_put);

	return true;
}

static struct pdaemon_resource_command pdaemon_resource_get_set(int cnum, uint8_t pid, resource_op op, uint16_t id, uint8_t *buf, uint16_t size)
{
	struct pdaemon_resource_command cmd;
	uint32_t header = 0;

	header |= (op << 31);
	header |= (size & 0x7fff) << 16;
	header |= id;

	cmd.pid = pid;
	cmd.query_header = header;
	cmd.data = buf;
	cmd.data_length = size;

	pdaemon_send_cmd(cnum, &cmd);

	return cmd;
}

static bool pdaemon_sync_fence(int cnum, uint32_t waited_fence)
{
	uint32_t fence = 0;

	/* wait for the command to be executed */
	do {
		data_segment_read(cnum, PDAEMON_DISPATCH_FENCE, 4, (uint8_t*)(&fence));
	} while(fence < waited_fence);

	return true;
}

static bool pdaemon_read_resource(int cnum, struct pdaemon_resource_command *cmd, uint8_t *buf)
{
	/* wait for the command to be executed */
	pdaemon_sync_fence(cnum, cmd->fence);

	/* read the data back */
	data_segment_read(cnum, cmd->data_addr, cmd->data_length, buf);

	return true;
}

static uint32_t ring_wrap_around(int cur_pos, int bump, uint32_t ring_base, uint32_t ring_size)
{
	return ((cur_pos + bump) % ring_size) + ring_base;  
}

void data_segment_read_ring(unsigned int cnum, uint32_t ring_base, 
			    uint32_t ring_size, uint32_t offset, uint32_t length, uint8_t *buf)
{
	uint16_t copied_bytes = 0;
	uint16_t cur_offset = offset;
	
	do {
		uint32_t max_bytes_to_copy = ring_base + ring_size - cur_offset;
		uint32_t bytes_to_copy = (length - copied_bytes);
		
		if (bytes_to_copy > max_bytes_to_copy)
			bytes_to_copy = max_bytes_to_copy;

		data_segment_read(cnum, cur_offset, bytes_to_copy, buf + copied_bytes);
		cur_offset = ring_wrap_around(cur_offset, bytes_to_copy, 0xa00, RDISPATCH_SIZE);
		
		copied_bytes += bytes_to_copy;
	} while (copied_bytes < length);
}

struct rdispatch_msg {
  
	uint8_t pid;
	uint8_t msg_id;
	uint8_t payload_size;
	uint8_t payload[RDISPATCH_SIZE];
};

int rdispatch_read_msg(int cnum, struct rdispatch_msg *msg){
		
	uint32_t RFIFO_GET;
	uint32_t RFIFO_PUT;
	uint8_t header_buf[0x4];

	RFIFO_GET = nva_rd32(cnum, 0x10a4cc);
	RFIFO_PUT = nva_rd32(cnum, 0x10a4c8);

	if ( RFIFO_GET == RFIFO_PUT ){
		return 1;
	} else {
		//data_segment_read(cnum, RFIFO_GET, 0x4, header_buf);
		data_segment_read_ring(cnum, 0xa00, RDISPATCH_SIZE, RFIFO_GET, 4, header_buf);
		RFIFO_GET = ring_wrap_around( RFIFO_GET, 3, 0xa00, RDISPATCH_SIZE);

		msg->pid = header_buf[0];
		msg->msg_id = header_buf[1];
		msg->payload_size = header_buf[2];

		data_segment_read_ring(cnum, 0xa00, RDISPATCH_SIZE, RFIFO_GET, header_buf[2], msg->payload);

		RFIFO_GET = nva_rd32(cnum, 0x10a4cc);
		nva_wr32(cnum, 0x10a4cc, ring_wrap_around( RFIFO_GET, 3 + header_buf[2], 0xa00, RDISPATCH_SIZE));
	}
    
	return 0;
}


int main(int argc, char **argv)
{
		
	if (nva_init()) {
		fprintf (stderr, "PCI init failure!\n");
		return 1;
	}
	int c;
	int cnum =0;
	while ((c = getopt (argc, argv, "c:")) != -1)
		switch (c) {
			case 'c':
				sscanf(optarg, "%d", &cnum);
				break;
		}
	if (cnum >= nva_cardsnum) {
		if (nva_cardsnum)
			fprintf (stderr, "No such card.\n");
		else
			fprintf (stderr, "No cards found.\n");
		return 1;
	}

	pdaemon_upload(cnum);
	usleep(1000);

	data_segment_dump(cnum, 0xc00, 0x10);
	usleep(5000);

	return 0;
}
