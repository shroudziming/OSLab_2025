#include <os/mm.h>
#include <assert.h>
#include <os/sched.h>
#include <os/task.h>
#include <os/kernel.h>

//test for page swap
#define TEST_FORCE_CAP_PAGES 0  // >0 时生效；设为 0 表示不限制
static unsigned long evict_count = 0;
static unsigned long swap_out_count = 0;
static unsigned long swap_in_count = 0;

#define TOTAL_PAGES ((FREEMEM_PA_END - FREEMEM_KERNEL_PA) / PAGE_SIZE)
#define FREEMEM_KERNEL_PA 0x52000000 + PAGE_SIZE
#define FREEMEM_PA_END 0x60000000
// #define FREEMEM_PA_END 0x52100000   //limit to 256 pages for test

#define PAGE_IDX(pa) ((pa - FREEMEM_KERNEL_PA) / PAGE_SIZE)
#define BITMAP(pa) (PAGE_IDX(pa) >> 3)      //each 8 bit for 8 pages
#define BITMAP_OFFSET(pa) (PAGE_IDX(pa) & 0x7) //which bit in that byte

// NOTE: A/C-core
// static ptr_t kernMemCurr = FREEMEM_KERNEL;
static uint8_t page_bitmap[TOTAL_PAGES / 8];
int used_pages = 0;

extern uint16_t kernel_sectors; // from loader/init
extern uint32_t swap_start_sector;
// swap layout configuration
#define SWAP_SECTORS_PER_SLOT (PAGE_SIZE / SECTOR_SIZE) // 8
#define SWAP_SLOT_NUM 256
#define SWAP_SECTOR_OFFSET  (swap_start_sector - kernel_sectors)//offset after kernel sectors

// frame metadata and FIFO
typedef struct {
    int in_use;
    uintptr_t owner_pgdir; // pgdir owning this frame (0 if none)
    uintptr_t va;          // user VA of this frame (page aligned)
    int swap_slot;         // -1 if not swapped, else slot id
} frame_meta_t;

typedef struct {
    int used;
    uintptr_t owner_pgdir;
    uintptr_t va;
} swap_entry_t;

static frame_meta_t frame_meta[TOTAL_PAGES];
static int fifo_q[TOTAL_PAGES];
static int fifo_head = 0, fifo_tail = 0;
static swap_entry_t swap_table[SWAP_SLOT_NUM];
static uint32_t start_sector = 0;

static inline int is_valid_pa(uintptr_t pa) {
    return (pa >= FREEMEM_KERNEL_PA && pa < FREEMEM_PA_END && ((pa & (PAGE_SIZE - 1)) == 0));
}

/* helper: push frame index to FIFO tail */
static void fifo_push(int idx)
{
    if ((fifo_tail + 1) % TOTAL_PAGES == fifo_head) {
        printk("FIFO queue full\n");
        return;
    }
    fifo_q[fifo_tail] = idx;
    fifo_tail = (fifo_tail + 1) % TOTAL_PAGES;
}

/* helper: pop FIFO head; returns -1 if empty */
static int fifo_pop(void)
{
    if (fifo_head == fifo_tail) return -1;
    int idx = fifo_q[fifo_head];
    fifo_head = (fifo_head + 1) % TOTAL_PAGES;
    return idx;
}

/* find swap slot for given owner_pgdir+va */
int swap_find(uintptr_t owner_pgdir, uintptr_t va)
{
    for (int i = 0; i < SWAP_SLOT_NUM; i++) {
        if (swap_table[i].used && swap_table[i].owner_pgdir == owner_pgdir && swap_table[i].va == va) {
            return i;
        }
    }
    return -1;
}

/* allocate a free swap slot; returns slot or -1 */
int swap_alloc_slot(void)
{
    for (int i = 0; i < SWAP_SLOT_NUM; i++) {
        if (!swap_table[i].used) {
            swap_table[i].used = 1;
            swap_table[i].owner_pgdir = 0;
            swap_table[i].va = 0;
            return i;
        }
    }
    return -1;
}

/* free swap slot */
void swap_free_slot(int slot)
{
    if (slot < 0 || slot >= SWAP_SLOT_NUM) return;
    swap_table[slot].used = 0;
    swap_table[slot].owner_pgdir = 0;
    swap_table[slot].va = 0;
}

/* write pa-sized page into swap slot (slot -> sector start) */
int swap_write_slot(int slot, uintptr_t pa)
{
    if (slot < 0 || slot >= SWAP_SLOT_NUM) return -1;
    uint32_t sector = start_sector + slot * SWAP_SECTORS_PER_SLOT;
    swap_out_count++;
    printk("SWAP OUT: slot=%d sector=%u pa=0x%lx out_cnt=%lu\n",
           slot, sector, (unsigned long)pa, swap_out_count);
    // bios_sd_write expects physical mem address
    int ret = bios_sd_write((unsigned)pa, SWAP_SECTORS_PER_SLOT, sector);
    return ret;
}

/* read swap slot into pa */
int swap_read_slot(int slot, uintptr_t pa)
{
    if (slot < 0 || slot >= SWAP_SLOT_NUM) return -1;
    uint32_t sector = start_sector + slot * SWAP_SECTORS_PER_SLOT;

    swap_in_count++;
    printk("SWAP IN : slot=%d sector=%u pa=0x%lx in_cnt=%lu\n",
           slot, sector, (unsigned long)pa, swap_in_count);

    int ret = bios_sd_read((unsigned)pa, SWAP_SECTORS_PER_SLOT, sector);
    return ret;
}

/* clear frame_meta for index */
static void clear_frame_meta_index(int idx)
{
    if (idx < 0 || idx >= TOTAL_PAGES) return;
    frame_meta[idx].in_use = 0;
    frame_meta[idx].owner_pgdir = 0;
    frame_meta[idx].va = 0;
    frame_meta[idx].swap_slot = -1;
}

/* record a new user frame mapping (pa must be physical page aligned) */
static void record_frame(uintptr_t pa, uintptr_t owner_pgdir, uintptr_t va)
{
    if (!is_valid_pa(pa)) return;
    int idx = PAGE_IDX(pa);
    frame_meta[idx].in_use = 1;
    frame_meta[idx].owner_pgdir = owner_pgdir;
    frame_meta[idx].va = va & ~(PAGE_SIZE - 1);
    frame_meta[idx].swap_slot = -1;
    fifo_push(idx);
}

/* find pcb by pgdir */
static pcb_t* find_pcb_by_pgdir(uintptr_t pgdir)
{
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pgdir == pgdir && pcb[i].pid != -1) return &pcb[i];
    }
    // also check pid0/s_pid0
    if (pid0_pcb.pgdir == pgdir) return &pid0_pcb;
    if (s_pid0_pcb.pgdir == pgdir) return &s_pid0_pcb;
    return NULL;
}

/* Evict one frame (FIFO). Returns physical address of freed page (pa), or 0 on failure. */
static uintptr_t evict_page(void)
{
    int tries = TOTAL_PAGES; // avoid infinite loop
    while (tries--) {
        int idx = fifo_pop();
        if (idx < 0) break;
        if (!frame_meta[idx].in_use) continue;
        uintptr_t victim_pa = FREEMEM_KERNEL_PA + idx * PAGE_SIZE;
        uintptr_t owner_pgdir = frame_meta[idx].owner_pgdir;
        uintptr_t va = frame_meta[idx].va;
        // find swap slot
        int slot = swap_alloc_slot();
        if (slot < 0) {
            // no swap space
            fifo_push(idx); // keep candidate
            return 0;
        }
        // write page to swap
        if (swap_write_slot(slot, victim_pa) < 0) {
            swap_free_slot(slot);
            fifo_push(idx);
            return 0;
        }

        evict_count++;
        // record swap ownership
        swap_table[slot].owner_pgdir = owner_pgdir;
        swap_table[slot].va = va;

        // unmap PTE in owner pgdir
        if (owner_pgdir != 0) {
            // map_page_helper with pa==0 will clear the pte
            map_page_helper(va, 0, owner_pgdir);
        }

        // clear frame meta and free page bitmap
        clear_frame_meta_index(idx);
        // ensure freePage clears bitmap / bzero; but we already will mark free below.
        // call freePage to be safe (it will clear bitmap and bzero kva)
        freePage(victim_pa);

        // return the freed physical page address (caller will allocate it)
        return victim_pa;
    }

    return 0;
}

void init_memory_manager() {
    bzero(page_bitmap, sizeof(page_bitmap)); // 清零位图

    for (int i = 0; i < TOTAL_PAGES; i++) clear_frame_meta_index(i);
    fifo_head = fifo_tail = 0;
    for (int i = 0; i < SWAP_SLOT_NUM; i++) swap_free_slot(i);
    // compute swap start sector
    start_sector = (uint32_t)(kernel_sectors + SWAP_SECTOR_OFFSET);
}

ptr_t allocPage(void)
{
    // align PAGE_SIZE
    // ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    // kernMemCurr = ret + numPage * PAGE_SIZE;
    // return ret;

    ptr_t limit_pa = FREEMEM_PA_END;
    if(TEST_FORCE_CAP_PAGES > 0){
        limit_pa = FREEMEM_KERNEL_PA + TEST_FORCE_CAP_PAGES * PAGE_SIZE;
    }

    for(ptr_t pa = FREEMEM_KERNEL_PA; pa < limit_pa;pa += PAGE_SIZE){
        uint32_t idx = BITMAP(pa);
        uint8_t bit = BITMAP_OFFSET(pa);

        if(!(page_bitmap[idx] & (1 << bit))){
            page_bitmap[idx] |= (1 << bit);
            used_pages++;
            return pa;
        }
    }

    uintptr_t freed_pa = evict_page();
    if (freed_pa) {
        // mark it as allocated now
        uint32_t idx = BITMAP(freed_pa);
        uint8_t bit = BITMAP_OFFSET(freed_pa);
        if (!(page_bitmap[idx] & (1 << bit))) {
            page_bitmap[idx] |= (1 << bit);
            used_pages++;
            return freed_pa;
        }
    }

    printk("Memory full\n");
    return 0; // out of memory
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

void freePage(ptr_t pa)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    // if(pa < FREEMEM_KERNEL_PA || pa >= FREEMEM_PA_END){
    //     return;
    // }
    // uint32_t idx = BITMAP(pa);
    // uint8_t bit = BITMAP_OFFSET(pa);

    // if(page_bitmap[idx] & (1 << bit)){
    //     page_bitmap[idx] &= ~(1 << bit);
    //     used_pages--;
    // }
    if (pa >= KERNEL_BASE) {
        pa = kva2pa(pa);
    }

    // 校验物理地址合法性（范围 + 对齐）
    if (!is_valid_pa(pa)) {
        return;
    }

    uint32_t idx = BITMAP(pa);
    uint8_t bit = BITMAP_OFFSET(pa);

    // 如果此页确实已被分配，则清除位并减计数
    if (page_bitmap[idx] & (1 << bit)) {
        page_bitmap[idx] &= ~(1 << bit);
        if (used_pages > 0) used_pages--;
        // 可选：清零页面内容（防止残留）
        bzero((void *)pa2kva(pa), PAGE_SIZE);
    }
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
        set_pfn(&pgd[vpn2], allocPage() >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    if(pmd[vpn1] == 0){
        //新的二级页目录
        set_pfn(&pmd[vpn1], allocPage() >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    if(pte[vpn0] == 0){
        //新的页表项
        ptr_t pa = allocPage();
        set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    }
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);

    uintptr_t phys = get_pa(pte[vpn0]);
    if (phys) {
        record_frame(phys, pgdir, va);
    }

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
        set_pfn(&pgd[vpn2], allocPage() >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    PTE *pmd = (uintptr_t *)pa2kva((get_pa(pgd[vpn2])));
    if(pmd[vpn1] == 0){
        // 分配一个新的二级页目录
        set_pfn(&pmd[vpn1], allocPage() >> NORMAL_PAGE_SHIFT);
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
        record_frame(pa, pgdir, va);
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