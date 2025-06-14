/* memalloc-main.c */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>

#include "../common.h"        
#include "memalloc-common.h"    

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Navigna Reddy Gangumalla");
MODULE_DESCRIPTION("Project 4, CSE 330 Spring 2025");
MODULE_VERSION("0.01");

// page perms

#define PAGE_PERMS_RW  PAGE_SHARED
#define PAGE_PERMS_R   PAGE_READONLY

#define MAX_PAGES       4096
#define MAX_ALLOCATIONS 100

static int             major;
static struct class   *memalloc_class;
static struct device  *memalloc_device;
static int             total_pages_allocated = 0;
static int             total_allocations     = 0;

static long memalloc_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static bool memalloc_ioctl_init(void);
static void memalloc_ioctl_teardown(void);

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = memalloc_ioctl,
};

// it returns 1 if any of the PTE in the start_vddr, start_vaddre and num_pages*PAGE_SIZE is present or else it gives 0.

static int check_vaddr_mapped(unsigned long start_vaddr, int num_pages)
{
    struct mm_struct *mm = current->mm;
    unsigned long     addr;
    pgd_t            *pgd;
    p4d_t            *p4d;
    pud_t            *pud;
    pmd_t            *pmd;
    pte_t            *pte;

    for (addr = start_vaddr;
         addr < start_vaddr + num_pages * PAGE_SIZE;
         addr += PAGE_SIZE) {

        pgd = pgd_offset(mm, addr);
        if (pgd_none(*pgd) || pgd_bad(*pgd)) continue;

        p4d = p4d_offset(pgd, addr);
        if (p4d_none(*p4d) || p4d_bad(*p4d)) continue;

        pud = pud_offset(p4d, addr);
        if (pud_none(*pud) || pud_bad(*pud)) continue;

        pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd) || pmd_bad(*pmd)) continue;

        pte = pte_offset_map(pmd, addr);
        if (!pte) continue;

        if (!pte_none(*pte)) {
            pte_unmap(pte);
            return 1;
        }
        pte_unmap(pte);
    }
    return 0;
}

//  IOCTL handler
 
static long memalloc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct mm_struct  *mm;
    long               ret;
    int                i;
    unsigned long      addr;
    struct alloc_info  alloc_req;
    pgd_t             *pgd;
    p4d_t             *p4d;
    pud_t             *pud;
    pmd_t             *pmd;
    pte_t             *pte;
    void              *vaddr_page;
    unsigned long      paddr;

    mm  = current->mm;
    ret = 0;

    switch (cmd) {
    case ALLOCATE:
        if (copy_from_user(&alloc_req, (void __user *)arg, sizeof(alloc_req)))
            return -EFAULT;

        if (total_pages_allocated + alloc_req.num_pages > MAX_PAGES)
            return -2;
        if (total_allocations >= MAX_ALLOCATIONS)
            return -3;
        if (check_vaddr_mapped(alloc_req.vaddr, alloc_req.num_pages))
            return -1;

        for (i = 0; i < alloc_req.num_pages; i++) {
            addr = alloc_req.vaddr + i * PAGE_SIZE;

            pgd = pgd_offset(mm, addr);
            if (pgd_none(*pgd)) return -EFAULT;

            p4d = p4d_offset(pgd, addr);
            if (p4d_none(*p4d)) memalloc_pud_alloc(p4d, addr);

            pud = pud_offset(p4d, addr);
            if (pud_none(*pud)) memalloc_pmd_alloc(pud, addr);

            pmd = pmd_offset(pud, addr);
            if (pmd_none(*pmd)) memalloc_pte_alloc(pmd, addr);

            pte = pte_offset_map(pmd, addr);
            if (!pte) return -EFAULT;

            vaddr_page = (void *)get_zeroed_page(GFP_KERNEL_ACCOUNT);
            if (!vaddr_page) {
                pte_unmap(pte);
                return -ENOMEM;
            }
            paddr = __pa(vaddr_page);

            if (alloc_req.write)
                set_pte_at(mm, addr, pte,
                           pfn_pte(paddr >> PAGE_SHIFT, PAGE_PERMS_RW));
            else
                set_pte_at(mm, addr, pte,
                           pfn_pte(paddr >> PAGE_SHIFT, PAGE_PERMS_R));

            pte_unmap(pte);
        }
// for new mappings to be live

        flush_tlb_mm(mm);

        total_pages_allocated += alloc_req.num_pages;
        total_allocations++;
        break;

    case FREE:
        ret = 0;
        break;

    default:
        ret = -EINVAL;
    }

    return ret;
}

// To create /dev/memalloc
 
static bool memalloc_ioctl_init(void)
{
    major = register_chrdev(0, "memalloc", &fops);
    if (major < 0)
        return false;

    memalloc_class = class_create("memalloc");
    if (IS_ERR(memalloc_class)) {
        unregister_chrdev(major, "memalloc");
        return false;
    }

    memalloc_device = device_create(memalloc_class,
                                    NULL,
                                    MKDEV(major, 0),
                                    NULL,
                                    "memalloc");
    if (IS_ERR(memalloc_device)) {
        class_destroy(memalloc_class);
        unregister_chrdev(major, "memalloc");
        return false;
    }

    return true;
}

// To remove /dev/memalloc
 
static void memalloc_ioctl_teardown(void)
{
    device_destroy(memalloc_class, MKDEV(major, 0));
    class_destroy(memalloc_class);
    unregister_chrdev(major, "memalloc");
}

static int __init memalloc_module_init(void)
{
    if (!memalloc_ioctl_init()) {
        printk(KERN_ERR "memalloc: init failed\n");
        return -1;
    }
    printk(KERN_INFO "memalloc: module loaded\n");
    return 0;
}

static void __exit memalloc_module_exit(void)
{
    memalloc_ioctl_teardown();
    printk(KERN_INFO "memalloc: module unloaded\n");
}

module_init(memalloc_module_init);
module_exit(memalloc_module_exit);

void __attribute__((weak))
mte_sync_tags(pte_t old_pte, pte_t new_pte)
{

}
