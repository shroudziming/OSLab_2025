#include <os/task.h>
#include <os/string.h>
#include <printk.h>
#include <os/kernel.h>
#include <type.h>
#include <os/mm.h>

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

	static uint8_t sector_buf[SECTOR_SIZE];		//single sector buffer

	uint64_t bytes_remaining = size;
	uint64_t current_mem_addr = mem_addr;
	uint64_t current_sector = start_sector;

	while(bytes_remaining > 0 && current_sector < start_sector + num_sectors){
		bios_sd_read((uintptr_t)sector_buf, 1,current_sector);

		uint64_t bytes_to_copy;
		uint64_t copy_offset;

		if(current_sector == start_sector){
			copy_offset = sector_offset;
			bytes_to_copy = (SECTOR_SIZE - sector_offset) < bytes_remaining ? (SECTOR_SIZE - sector_offset) : bytes_remaining;
		}else{
			copy_offset = 0;
			bytes_to_copy = SECTOR_SIZE < bytes_remaining ? SECTOR_SIZE : bytes_remaining;
		}

		memcpy((void *)current_mem_addr,sector_buf + copy_offset,bytes_to_copy);

		current_mem_addr += bytes_to_copy;
		current_sector++;
		bytes_remaining -= bytes_to_copy;
	}

    return mem_addr;
}


uint64_t map_task(char *taskname, uintptr_t pgdir)
{
    static uint8_t sector_buf[SECTOR_SIZE];

    for (int i = 0; i < TASK_MAXNUM; i++) {
        if (strcmp(taskname, tasks[i].name) != 0)
            continue;

        uint64_t file_off  = tasks[i].file_off;
        uint64_t file_size = tasks[i].file_size;
        uint64_t mem_size  = tasks[i].p_memsz;

        uint64_t cur_file_off = 0;
        uint64_t user_va_base = USER_ENTRYPOINT;

        while (cur_file_off < file_size) {
            uint64_t abs_off = file_off + cur_file_off;
            uint64_t sector  = abs_off / SECTOR_SIZE;
            uint64_t sec_off = abs_off % SECTOR_SIZE;

            bios_sd_read(kva2pa((uintptr_t)sector_buf), 1, sector);

            uint64_t copy_bytes =
                (SECTOR_SIZE - sec_off < file_size - cur_file_off)
                    ? (SECTOR_SIZE - sec_off)
                    : (file_size - cur_file_off);

            uint64_t user_va = user_va_base + cur_file_off;
            uint64_t page_va = user_va & ~(PAGE_SIZE - 1);

            uintptr_t page_kva = alloc_page_helper(page_va, pgdir);

            memcpy(
                (void *)(page_kva + (user_va & (PAGE_SIZE - 1))),
                sector_buf + sec_off,
                copy_bytes
            );

            cur_file_off += copy_bytes;
        }

        //bss section zeroed
        if (mem_size > file_size) {
            uint64_t bss_start_va = user_va_base + file_size;
            uint64_t bss_end_va   = user_va_base + mem_size;

            uint64_t va = bss_start_va & ~(PAGE_SIZE - 1);

            while (va < bss_end_va) {
                uintptr_t page_kva = alloc_page_helper(va, pgdir);

                uint64_t start = (va < bss_start_va)
                                     ? (bss_start_va & (PAGE_SIZE - 1))
                                     : 0;

                uint64_t end = PAGE_SIZE;
                if (va + PAGE_SIZE > bss_end_va)
                    end = bss_end_va & (PAGE_SIZE - 1);

                if (end > start)
                    bzero((void *)(page_kva + start), end - start);

                va += PAGE_SIZE;
            }
        }

        return USER_ENTRYPOINT;
    }

    return -1;
}
