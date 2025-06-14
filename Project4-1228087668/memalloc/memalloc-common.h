#ifndef MEMALLOC_COMMON_H
#define MEMALLOC_COMMON_H
#include <linux/types.h>
/* Only keep the helper function declarations here */
pud_t *memalloc_pud_alloc(p4d_t *p4d, unsigned long vaddr);
pmd_t *memalloc_pmd_alloc(pud_t *pud, unsigned long vaddr);
void memalloc_pte_alloc(pmd_t *pmd, unsigned long vaddr);
#endif /* MEMALLOC_COMMON_H */
