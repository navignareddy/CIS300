/* memalloc-helper.c */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/pgtable-types.h> 

#include "memalloc-common.h"


// it is a PUD page

pud_t* memalloc_pud_alloc(p4d_t* p4d, unsigned long vaddr)
{
    gfp_t gfp = GFP_KERNEL_ACCOUNT;
    pud_t *new_pud = (pud_t*)get_zeroed_page(gfp);
    if (!new_pud) {
        printk("memalloc-helper: pud alloc failed\n");
        return NULL;
    }

#ifdef CONFIG_ARM64
    WRITE_ONCE(*p4d, __p4d(P4D_TYPE_TABLE | __pa(new_pud)));
#else
    WRITE_ONCE(*p4d, __p4d(_PAGE_TABLE    | __pa(new_pud)));
#endif

    return new_pud;
}


pmd_t* memalloc_pmd_alloc(pud_t* pud, unsigned long vaddr)
{
    gfp_t gfp = GFP_KERNEL_ACCOUNT;
    pmd_t *new_pmd = (pmd_t*)get_zeroed_page(gfp);
    if (!new_pmd) {
        printk("memalloc-helper: pmd alloc failed\n");
        return NULL;
    }

#ifdef CONFIG_ARM64
    set_pud(pud, __pud(PUD_TYPE_TABLE | __pa(new_pmd)));
#else
    set_pud(pud, __pud(_PAGE_TABLE    | __pa(new_pmd)));
#endif

    return new_pmd;
}


void memalloc_pte_alloc(pmd_t* pmd, unsigned long vaddr)
{
    gfp_t gfp = GFP_KERNEL_ACCOUNT;
    pte_t *new_pte = (pte_t*)get_zeroed_page(gfp);
    if (!new_pte) {
        printk("memalloc-helper: pte alloc failed\n");
        return;
    }

#ifdef CONFIG_ARM64
    set_pmd(pmd, __pmd(PMD_TYPE_TABLE | __pa(new_pte)));
#else
    set_pmd(pmd, __pmd(_PAGE_TABLE    | __pa(new_pte)));
#endif
}
