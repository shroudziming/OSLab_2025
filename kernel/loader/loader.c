#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define APP_BASE 0x52000000
#define APP_ADDR_INTERVAL 0x10000
#define TASKNUM_ADDR 0x502001fe
#define KERNEL_SECTORS_ADDR 0x502001fc

/*static void bios_puthex(unsigned char x) {
    const char hex_table[] = "0123456789ABCDEF";
    char buf[3];
    buf[0] = hex_table[(x >> 4) & 0xF];
    buf[1] = hex_table[x & 0xF];
    buf[2] = '\0';
    bios_putstr(buf);
}
*/
uint16_t kernel_sectors;
uint16_t tasknum;
uint32_t table_offset;
static int find_task_index(char *name){
	int i;
	for(i=0;i<tasknum;++i){
		if(strcmp(tasks[i].name,name) == 0)
			return i;
	}
	return -1;
}

uint64_t load_task_img(char *taskname)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
	//[p1-task3] load by id
	/*
	short tasknum = *(short *)TASKNUM_ADDR;
	if(taskid < 0 || taskid >= tasknum){
		bios_putstr("Invalid taskid");
		return 0;
	}
	short kernel_sectors = 15;
	int block_id = 1 + kernel_sectors + taskid * 15;
	uintptr_t mem_addr = APP_BASE + taskid * APP_ADDR_INTERVAL;
	bios_putstr("loading task ");
	bios_putchar('0' + taskid);

	bios_sd_read(mem_addr,15,block_id);
	unsigned char *p = (unsigned char *)mem_addr;
	bios_putstr("First 4 bytes: ");
	bios_puthex(p[0]);
	bios_puthex(p[1]);
	bios_puthex(p[2]);
	bios_puthex(p[3]);
	bios_putstr("\n");
	
	*/
	
	//[p1-task4] load by name
	kernel_sectors = *(short *)KERNEL_SECTORS_ADDR;
	tasknum = *(short *)(TASKNUM_ADDR);
	int idx = find_task_index(taskname);
	if(idx < 0){
		bios_putstr("[Error] Task not found.\n");
		return -1;
	}
	// bios_putstr("loading task ");
	// bios_putstr(tasks[idx].name);
	// bios_putstr("\n");

	//bytes offset
	uintptr_t mem_addr = APP_BASE + idx * APP_ADDR_INTERVAL;
	uint64_t offset = tasks[idx].file_off;
	uint64_t size = tasks[idx].file_size;

	uint64_t start_sector = offset / SECTOR_SIZE;
	uint64_t sector_offset = offset % SECTOR_SIZE;
	uint64_t end_sector = (offset + size + SECTOR_SIZE - 1) / SECTOR_SIZE;
	uint64_t num_sectors = end_sector - start_sector;

	static uint8_t sector_buf[SECTOR_SIZE * 15];

	bios_sd_read((uintptr_t)sector_buf,num_sectors,start_sector);
	
	memcpy((void *)mem_addr,sector_buf + sector_offset,size);

	
	// bios_putstr("done.\n");
	

    return mem_addr;
}
