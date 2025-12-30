#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    uintptr_t va_start = io_base;
    while(size > 0){
        kernel_map_page_helper(io_base,phys_addr,pa2kva(PGDIR_PA));
        io_base += PAGE_SIZE;
        phys_addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    local_flush_tlb_all();
    return (void *)va_start;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}

//map va & pa into kernel page table(return 1 if success, 0 if fail)
int kernel_map_page_helper(uintptr_t va,uintptr_t pa,uintptr_t pgdir){
    va &= VA_MASK;
    uint64_t vpn2 = 
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    PTE *pgd = (PTE*)pgdir;
    if(pgd[vpn2]==0){
        //新的三级页目录
        set_pfn(&pgd[vpn2], kva2pa(allocPage()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    PTE *pmd = (uintptr_t *)pa2kva((get_pa(pgd[vpn2])));
    if(pa==0){
        pmd[vpn1]=0;
        return 1;
    }else if(pmd[vpn1]==0){
        set_pfn(&pmd[vpn1], pa >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                         _PAGE_ACCESSED | _PAGE_DIRTY);
        return 1;
    }
    return 0;
}