#include <os/mm.h>
#include <assert.h>
#include <os/sched.h>
#include <os/task.h>
#include <os/kernel.h>
#define TOTAL_PAGES ((FREEMEM_PA_END - FREEMEM_KERNEL_PA) / PAGE_SIZE)
#define KERNEL_START 0x50000000
#define KERNEL_END 0x60000000
#define FREEMEM_KERNEL_PA 0x52000000 + PAGE_SIZE
#define FREEMEM_PA_END 0x60000000

#define PAGE_IDX(pa) ((pa - FREEMEM_KERNEL_PA) / PAGE_SIZE)
#define BITMAP(pa) (PAGE_IDX(pa) >> 3)      //each 8 bit for 8 pages
#define BITMAP_OFFSET(pa) (PAGE_IDX(pa) & 0x7) //which bit in that byte

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;
static uint8_t page_bitmap[TOTAL_PAGES / 8];
int used_pages = 0;

static inline int is_valid_pa(uintptr_t pa) {
    return (pa >= FREEMEM_KERNEL_PA && pa < FREEMEM_PA_END && ((pa & (PAGE_SIZE - 1)) == 0));
}


void init_memory_manager() {
    bzero(page_bitmap, sizeof(page_bitmap)); // 清零位图
}

ptr_t allocPage(void)
{
    // align PAGE_SIZE
    // ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    // kernMemCurr = ret + numPage * PAGE_SIZE;
    // return ret;

    for(ptr_t pa = FREEMEM_KERNEL_PA; pa < FREEMEM_PA_END;pa += PAGE_SIZE){
        uint32_t idx = BITMAP(pa);
        uint8_t bit = BITMAP_OFFSET(pa);

        if(!(page_bitmap[idx] & (1 << bit))){
            page_bitmap[idx] |= (1 << bit);
            used_pages++;
            return pa;
        }
    }

    printk("Memory full/n");
    assert(0); // no free page
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