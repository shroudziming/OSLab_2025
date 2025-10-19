#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define APP_BASE 0x52000000
#define APP_ADDR_INTERVAL 0x10000
#define TASKNUM_ADDR 0x502001fe

/*static void bios_puthex(unsigned char x) {
    const char hex_table[] = "0123456789ABCDEF";
    char buf[3];
    buf[0] = hex_table[(x >> 4) & 0xF];
    buf[1] = hex_table[x & 0xF];
    buf[2] = '\0';
    bios_putstr(buf);
}
*/

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
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
	/*unsigned char *p = (unsigned char *)mem_addr;
	bios_putstr("First 4 bytes: ");
	bios_puthex(p[0]);
	bios_puthex(p[1]);
	bios_puthex(p[2]);
	bios_puthex(p[3]);
	bios_putstr("\n");
	*/
	bios_putstr("done.\n");

    return 0;
}
