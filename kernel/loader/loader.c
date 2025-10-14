#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
	if(taskid < 0 || taskid >= tasknum){
		bios_putstr("Invalid taskid");
		return 0;
	}
	short kernel_sectors = 15;
	short 
	int block_id = 1 + kernel_sectors + taskid * 15;
	uintptr_t mem_addr = 0x52000000;
	bios_putstr("loading task ");
	bios_putstr('0' + taskid);

	bios_id_read(mem_addr,15,blockid);

	bios_putstr("done.");

    return 0;
}
