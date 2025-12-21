#include <os/mm.h>
#include <assert.h>
#include <os/sched.h>
#include <os/task.h>
#include <os/kernel.h>

#define KERNEL_START 0xffffffc050000000
#define TOTAL_PAGES ((FREEMEM_END - FREEMEM_START) / PAGE_SIZE)
#define FREEMEM_START 0xffffffc050200000 + PAGE_SIZE
#define FREEMEM_END 0xffffffc060000000

#define PAGE_IDX(va) ((va - FREEMEM_START) / PAGE_SIZE)
#define BITMAP(va) (PAGE_IDX(va) >> 3)      //each 8 bit for 8 pages
#define BITMAP_OFFSET(va) (PAGE_IDX(va) % 8) //which bit in that byte

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;
static uint8_t page_bitmap[TOTAL_PAGES / 8];
int used_pages = 0;

extern uint16_t kernel_sectors; // from loader/init
extern uint32_t swap_start_sector;
// swap layout configuration
#define SWAP_SECTOR_OFFSET  (swap_start_sector - kernel_sectors)//offset after kernel sectors

static uint32_t start_sector = 0;

int pgdir_id;
int page_cnt;  //已经使用的物理页数


void init_memory_manager() {
    bzero(page_bitmap, sizeof(page_bitmap)); // 清零位图

    // compute swap start sector
    start_sector = (uint32_t)(kernel_sectors + SWAP_SECTOR_OFFSET);
}

bool is_memory_full()
{
    for (int i = 0; i < TOTAL_PAGES / 8; i++) {
        if (page_bitmap[i] != 0xff) {
            return false;
        }
    }
    return true;
}

ptr_t allocPage(void)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    ptr_t initial_search_addr = ret;

    // 寻找一个空闲页
    while (page_bitmap[BITMAP(ret)] & (1 << (BITMAP_OFFSET(ret)))) {
        kernMemCurr = ret + PAGE_SIZE;
        ret = ROUND(kernMemCurr, PAGE_SIZE);

        // 如果回绕了
        if (ret >= FREEMEM_END) {
            kernMemCurr = FREEMEM_KERNEL;
            ret = ROUND(kernMemCurr, PAGE_SIZE);
        }

        // 如果搜索回到了起点，说明没有空闲页了
        if (ret == initial_search_addr) {
            printk("PANIC: Out of memory!\n");
            while(1); // 或者返回 NULL，让调用者处理
            return 0;
        }
    }

    // 标记页面为已使用
    page_bitmap[BITMAP(ret)] |= (1 << (BITMAP_OFFSET(ret)));
    kernMemCurr = ret + PAGE_SIZE;
    if (kernMemCurr >= pa2kva(FREEMEM_END)) {
        kernMemCurr = FREEMEM_KERNEL;
    }

    used_pages++;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void freePage(ptr_t va)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    // if(pa < FREEMEM_START || pa >= FREEMEM_END){
    //     return;
    // }
    // uint32_t idx = BITMAP(pa);
    // uint8_t bit = BITMAP_OFFSET(pa);

    // if(page_bitmap[idx] & (1 << bit)){
    //     page_bitmap[idx] &= ~(1 << bit);
    //     used_pages--;
    // }
    if(va < KERNEL_START || va >= FREEMEM_END){
        return;
    }

    page_bitmap[BITMAP(va)] &= ~(1 << (BITMAP_OFFSET(va)));
    used_pages--;
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    memcpy((void *)dest_pgdir,(const void*)src_pgdir,PAGE_SIZE);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^ (va >> NORMAL_PAGE_SHIFT);

    PTE *pgd = (PTE *)pgdir;
    if(pgd[vpn2] == 0){
        //新的三级页目录
        set_pfn(&pgd[vpn2], kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    if(pmd[vpn1] == 0){
        //新的二级页目录
        set_pfn(&pmd[vpn1], kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    if(pte[vpn0] == 0){
        //新的页表项
        ptr_t pa = kva2pa(allocPage());
        set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    }
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);


    return pa2kva(get_pa(pte[vpn0]));
}

//store mapping relation in page table
int map_page_helper(uintptr_t va,uintptr_t pa,uintptr_t pgdir){
    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> NORMAL_PAGE_SHIFT);
    PTE *pgd = (PTE*)pgdir;
    if (pgd[vpn2] == 0) {
        set_pfn(&pgd[vpn2], kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    PTE *pmd = (uintptr_t *)pa2kva((get_pa(pgd[vpn2])));
    if(pmd[vpn1] == 0){
        //分配一个新的二级页目录
        set_pfn(&pmd[vpn1], kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    // 若pa等于0，即取消映射操作
    if(pa==0){
        pte[vpn0] = 0;
        local_flush_tlb_page(va);
        return 1;
    }
    // 将对应实地址置为pa
    else if(pte[vpn0]==0){
        set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
        set_attribute(
            &pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                            _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER);

        return 1;
    }
    return 0;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}


void free_all_pages(pcb_t *pcb)
{
    PTE *pgd = (PTE*)pcb->pgdir;
    for(int i=0;i<512;i++)
    {
        int kernel = 0;
        if(pgd[i] == 0)
        {
            continue;
        }
        PTE *pmd = (uintptr_t *)pa2kva((get_pa(pgd[i])));
        for(int j=0;j<512;j++)
        {
            if(pmd[j] == 0)
            {
                continue;
            }
            if((pmd[j] & (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)) != 0)
            {
                kernel = 1;
                continue;
            }
            PTE *pte = (uintptr_t *)pa2kva((get_pa(pmd[j])));
            for(int k=0;k<512;k++)
            {
                if(pte[k] == 0)
                {
                    continue;
                }
                freePage(get_pa(pte[k]));
            }
            freePage(get_pa(pmd[j]));
        }
        if(kernel==1)
            continue;
        freePage(get_pa(pgd[i]));
    }
    freePage(kva2pa(pcb->pgdir));
}

LIST_HEAD(in_mem_list);
LIST_HEAD(swap_out_list);
LIST_HEAD(free_list);

alloc_info_t alloc_info[USER_PAGE_MAX_NUM];

static inline int get_pgdir_id(uintptr_t pgdir){
    for(int i=0; i<NUM_MAX_TASK; i++){
        if(pgdir==pcb[i].pgdir)
            return i;
    }
}
static inline alloc_info_t* lnode2info(list_node_t* lnode){
    return (alloc_info_t*)lnode;    //lnode是info的第一个成员变量，lnode的地址即info的地址
}

void init_alloc_info(){
    // 计数器复位
    page_cnt = 0;
    for(int i=0; i<USER_PAGE_MAX_NUM; i++){
        // 将所有node加入free list
        list_add_tail(&alloc_info[i].lnode, &free_list);
        alloc_info[i].uva = 0;
        alloc_info[i].pa = 0;
        alloc_info[i].on_disk_sec = 0;
        alloc_info[i].pgdir_id = 0;
    }
}
// 换出页面并返回info结点
alloc_info_t* swapPage(){
    list_node_t* swap_lnode = in_mem_list.next; // 最早被分配的node
    assert(swap_lnode!=&in_mem_list);
    //从in_mem_list到swap_out_list
    list_del(swap_lnode);
    list_add_tail(swap_lnode, &swap_out_list);
    // printk("Swap out page uva=%lx pa=%lx\n", lnode2info(swap_lnode)->uva, lnode2info(swap_lnode)->pa);
    //转移到磁盘
    alloc_info_t* info = lnode2info(swap_lnode);
    bios_sd_write(info->pa, PAGE_SIZE/SECTOR_SIZE, swap_start_sector);
    info->on_disk_sec = swap_start_sector;
    swap_start_sector += PAGE_SIZE/SECTOR_SIZE;
    //清除页表映射
    map_page_helper(info->uva, 0, pcb[info->pgdir_id].pgdir);
    clear_pgdir(pa2kva(info->pa));
    local_flush_tlb_all();
    return info;
}
// 因为换页时需要知道对应用户虚地址，才可将表项置零，故需要额外加参数uva
ptr_t uva_allocPage(int numPage, uintptr_t uva)
{
    // 检查是否是被换出的页面
    for(list_node_t* lnode=swap_out_list.next; lnode!=&swap_out_list; lnode=lnode->next){
        alloc_info_t* in_info = lnode2info(lnode);  
        if(in_info->uva==uva && in_info->pgdir_id == pgdir_id){
            // 获取待换出的页、待换入的页对应info指针
            alloc_info_t* out_info = swapPage();
            in_info->pa = out_info->pa;
            //从swap_out_list移到in_mem_list
            list_del(&in_info->lnode);
            list_add_tail(&in_info->lnode, &in_mem_list);
            // 建立新映射
            map_page_helper(in_info->uva, in_info->pa, current_running[cpu_id]->pgdir);
            local_flush_tlb_all();
            // 重新读入内存
            bios_sd_read(in_info->pa, PAGE_SIZE/SECTOR_SIZE, in_info->on_disk_sec);
            in_info->on_disk_sec = 0;
            return pa2kva(in_info->pa);
        }
    }
    //分配新的node
    list_node_t* new_lnode = free_list.next;
    // assert(new_lnode!=&free_list);
    if (new_lnode == &free_list) {
        // free_list已耗尽，从swap_out_list中回收一个
        new_lnode = swap_out_list.next;
        assert(new_lnode != &swap_out_list); // 如果swap_out_list也为空，则有问题
    }
    alloc_info_t* in_info = lnode2info(new_lnode);
    list_del(new_lnode);
    list_add_tail(new_lnode, &in_mem_list);
    in_info->uva = uva;
    in_info->pgdir_id = pgdir_id;

    //检查是否需要换出页
    if(page_cnt>=KERN_PAGE_MAX_NUM){
        page_cnt++;
        alloc_info_t* out_info = swapPage();
        in_info->pa = out_info->pa;
        // 将pa作为待分配的页表的地址
        return pa2kva(in_info->pa);
    }
    else{
        page_cnt++;
        in_info->pa = kva2pa(allocPage());
        return pa2kva(in_info->pa);
    }
}

uintptr_t alloc_limit_page_helper(uintptr_t va, uintptr_t pgdir)
{
    pgdir_id = get_pgdir_id(current_running[cpu_id]->pgdir);
    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> NORMAL_PAGE_SHIFT);
    PTE *pgd = (PTE*)pgdir;
    if (pgd[vpn2] == 0) {
        //三级页目录
        set_pfn(&pgd[vpn2], kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    PTE *pmd = (uintptr_t *)pa2kva((get_pa(pgd[vpn2])));
    if(pmd[vpn1] == 0){
        //二级页目录
        set_pfn(&pmd[vpn1], kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1])); 
    if(pte[vpn0] == 0){
        //分配一个新的页或该虚地址对应的物理页被换出
        ptr_t pa = kva2pa(uva_allocPage(1, (va>>NORMAL_PAGE_SHIFT)<<NORMAL_PAGE_SHIFT));
        set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    }
    set_attribute(
            &pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                            _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER);
    return pa2kva(get_pa(pte[vpn0]));
}