/**
 * NexusVM Extended Page Tables (EPT) Implementation
 */

#include <hypervisor/mmu/ept.h>
#include <hypervisor/hypervisor.h>
#include <utils/log.h>
#include <utils/assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* EPT page table levels */
#define EPT_PML4_INDEX(gpa)   (((gpa) >> 39) & 0x1FF)
#define EPT_PDP_INDEX(gpa)    (((gpa) >> 30) & 0x1FF)
#define EPT_PD_INDEX(gpa)     (((gpa) >> 21) & 0x1FF)
#define EPT_PT_INDEX(gpa)     (((gpa) >> 12) & 0x1FF)

/* EPT entry masks */
#define EPT_PRESENT_MASK       0x1
#define EPT_WRITE_MASK         0x2
#define EPT_EXECUTE_MASK       0x4  /* Note: Inverted in EPT */
#define EPT_MEMORY_TYPE_MASK   0x38
#define EPT_ACCESSED_MASK      0x100
#define EPT_DIRTY_MASK         0x200
#define EPT_LARGE_PAGE_MASK    0x80
#define EPT_PHYS_ADDR_MASK     0xFFFFFFFFFF000

/*============================================================================
 * EPT Initialization
 *============================================================================*/

static bool g_ept_initialized = false;

nexusvm_result_t ept_init(void)
{
    NEXUSVM_LOG_INFO("Initializing EPT subsystem\n");
    
    /* EPT requires VMX and CPU support check */
    hypervisor_t *hv = hypervisor_get_instance();
    if (!hv || !hv->has_ept) {
        NEXUSVM_LOG_WARN("EPT not supported on this CPU\n");
        /* Not a fatal error - EPT is optional */
    }
    
    g_ept_initialized = true;
    return NEXUSVM_OK;
}

void ept_shutdown(void)
{
    g_ept_initialized = false;
    NEXUSVM_LOG_INFO("EPT subsystem shutdown\n");
}

/*============================================================================
 * EPT Page Table Management
 *============================================================================*/

void *ept_alloc_root(void)
{
    phys_addr_t phys;
    void *root = ept_alloc_pages(1, &phys);
    
    if (root) {
        memset(root, 0, PAGE_SIZE);
    }
    
    return root;
}

void ept_free_root(void *root)
{
    if (root) {
        ept_free_pages(root);
    }
}

void ept_clear_root(void *root)
{
    if (root) {
        memset(root, 0, PAGE_SIZE);
    }
}

void *ept_alloc_pages(u64 count, phys_addr_t *physical)
{
    size_t size = count * PAGE_SIZE;
    
    /* Allocate aligned memory */
    void *pages = aligned_alloc(PAGE_SIZE, size);
    if (!pages) {
        return NULL;
    }
    
    memset(pages, 0, size);
    
    /* Get physical address (this is architecture-dependent) */
    /* In a real kernel, you'd use virt_to_phys() */
    if (physical) {
        *physical = (phys_addr_t)pages;
    }
    
    return pages;
}

void ept_free_pages(void *pages)
{
    if (pages) {
        free(pages);
    }
}

/*============================================================================
 * EPT Mapping
 *============================================================================*/

static nexusvm_result_t ept_alloc_pdpt(void *root, guest_addr_t guest_phys, ept_pte_t *pte)
{
    phys_addr_t phys;
    void *pdpt = ept_alloc_pages(1, &phys);
    if (!pdpt) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Setup upper entry pointing to PDPT */
    pte->present = 1;
    pte->write = 1;
    pte->executable = 0;
    pte->memory_type = EPT_MEMORY_TYPE_WRITE_BACK;
    pte->physical_address = (phys >> 12) & 0xFFFFFFFFFF;
    
    return NEXUSVM_OK;
}

static nexusvm_result_t ept_alloc_pd(void *pdp, guest_addr_t guest_phys, ept_pte_t *pte)
{
    phys_addr_t phys;
    void *pd = ept_alloc_pages(1, &phys);
    if (!pd) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    pte->present = 1;
    pte->write = 1;
    pte->executable = 0;
    pte->memory_type = EPT_MEMORY_TYPE_WRITE_BACK;
    pte->physical_address = (phys >> 12) & 0xFFFFFFFFFF;
    
    return NEXUSVM_OK;
}

static nexusvm_result_t ept_alloc_pt(void *pdir, guest_addr_t guest_phys, ept_pte_t *pte)
{
    phys_addr_t phys;
    void *pt = ept_alloc_pages(1, &phys);
    if (!pt) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    pte->present = 1;
    pte->write = 1;
    pte->executable = 0;
    pte->memory_type = EPT_MEMORY_TYPE_WRITE_BACK;
    pte->physical_address = (phys >> 12) & 0xFFFFFFFFFF;
    
    return NEXUSVM_OK;
}

nexusvm_result_t ept_map(void *root, guest_addr_t guest_phys, ept_pte_t *pte)
{
    if (!root || !pte) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    ept_pml4e_t *pml4 = (ept_pml4e_t *)root;
    u32 pml4_idx = EPT_PML4_INDEX(guest_phys);
    
    /* Allocate PDPT if needed */
    if (!pml4[pml4_idx].present) {
        ept_alloc_pdpt(root, guest_phys, (ept_pte_t *)&pml4[pml4_idx]);
    }
    
    ept_pdpe_t *pdpt = (ept_pdpe_t *)((pml4[pml4_idx].physical_address << 12) + 
                                        (phys_addr_t)root - (phys_addr_t)root);
    e32 pdpt_idx = EPT_PDP_INDEX(guest_phys);
    pdpt = (ept_pdpe_t *)((pml4[pml4_idx].physical_address << 12));
    
    /* Allocate PD if needed */
    if (!pdpt[pdpt_idx].present) {
        ept_alloc_pd((void *)pdpt, guest_phys, (ept_pte_t *)&pdpt[pdpt_idx]);
    }
    
    ept_pue_t *pd = (ept_pue_t *)(pdpt[pdpt_idx].physical_address << 12);
    u32 pd_idx = EPT_PD_INDEX(guest_phys);
    
    /* Allocate PT if needed (for 4KB pages) */
    if (!pd[pd_idx].present && !pd[pd_idx].big_pagesize) {
        ept_alloc_pt((void *)pd, guest_phys, (ept_pte_t *)&pd[pd_idx]);
    }
    
    /* Get PT and set entry */
    ept_pte_t *pt = (ept_pte_t *)(pd[pd_idx].physical_address << 12);
    u32 pt_idx = EPT_PT_INDEX(guest_phys);
    
    /* Copy PTE */
    pt[pt_idx] = *pte;
    
    return NEXUSVM_OK;
}

nexusvm_result_t ept_unmap(void *root, guest_addr_t guest_phys)
{
    if (!root) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    ept_pte_t pte;
    nexusvm_result_t result = ept_lookup(root, guest_phys, &pte);
    if (result != NEXUSVM_OK) {
        return result;
    }
    
    /* Clear the entry */
    /* In a real implementation, we'd traverse and clear */
    ept_pte_t *pt = (ept_pte_t *)((ept_pte_t *)root + 
                                   EPT_PML4_INDEX(guest_phys));
    /* ... traverse and clear ... */
    
    /* Invalidate TLB */
    ept_invalidate(root, guest_phys);
    
    return NEXUSVM_OK;
}

nexusvm_result_t ept_lookup(void *root, guest_addr_t guest_phys, ept_pte_t *pte)
{
    if (!root || !pte) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    ept_pml4e_t *pml4 = (ept_pml4e_t *)root;
    u32 pml4_idx = EPT_PML4_INDEX(guest_phys);
    
    if (!pml4[pml4_idx].present) {
        return NEXUSVM_ERR_NOT_FOUND;
    }
    
    /* Get PDPT */
    ept_pdpe_t *pdpt = (ept_pdpe_t *)(pml4[pml4_idx].physical_address << 12);
    u32 pdpt_idx = EPT_PDP_INDEX(guest_phys);
    
    if (!pdpt[pdpt_idx].present) {
        return NEXUSVM_ERR_NOT_FOUND;
    }
    
    /* Get PD */
    ept_pue_t *pd = (ept_pue_t *)(pdpt[pdpt_idx].physical_address << 12);
    u32 pd_idx = EPT_PD_INDEX(guest_phys);
    
    if (!pd[pd_idx].present) {
        return NEXUSVM_ERR_NOT_FOUND;
    }
    
    /* Check for 2MB page */
    if (pd[pd_idx].big_pagesize) {
        memcpy(pte, &pd[pd_idx], sizeof(ept_pte_t));
        return NEXUSVM_OK;
    }
    
    /* Get PT */
    ept_pte_t *pt = (ept_pte_t *)(pd[pd_idx].physical_address << 12);
    u32 pt_idx = EPT_PT_INDEX(guest_phys);
    
    if (!pt[pt_idx].present) {
        return NEXUSVM_ERR_NOT_FOUND;
    }
    
    memcpy(pte, &pt[pt_idx], sizeof(ept_pte_t));
    
    return NEXUSVM_OK;
}

nexusvm_result_t ept_translate(void *root, guest_addr_t guest_phys, phys_addr_t *host_phys)
{
    if (!root || !host_phys) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    ept_pte_t pte;
    nexusvm_result_t result = ept_lookup(root, guest_phys, &pte);
    if (result != NEXUSVM_OK) {
        return result;
    }
    
    if (!pte.present) {
        return NEXUSVM_ERR_NOT_FOUND;
    }
    
    u64 page_offset = guest_phys & (PAGE_SIZE - 1);
    *host_phys = (pte.physical_address << 12) | page_offset;
    
    return NEXUSVM_OK;
}

/*============================================================================
 * EPT Cache Operations
 *============================================================================*/

void ept_invalidate(void *root, guest_addr_t guest_phys)
{
    /* INVEPT single context */
    __asm__ volatile (
        "mov $1, %%rax\n\t"
        "mov %0, %%rbx\n\t"
        "vmptrst %1\n\t"
        "invept %%rax, (%%rbx)\n\t"
        :
        : "m" (root), "m" (guest_phys)
        : "rax", "rbx", "cc", "memory"
    );
}

void ept_invalidate_all(void)
{
    /* INVEPT all contexts */
    __asm__ volatile (
        "mov $2, %%rax\n\t"
        "xor %%rbx, %%rbx\n\t"
        "invept %%rax, (%%rbx)\n\t"
        :
        :
        : "rax", "rbx", "cc", "memory"
    );
}

void ept_invept(u64 type)
{
    __asm__ volatile (
        "mov %0, %%rax\n\t"
        "xor %%rbx, %%rbx\n\t"
        "invept %%rax, (%%rbx)\n\t"
        :
        : "r" (type)
        : "rax", "rbx", "cc", "memory"
    );
}

/*============================================================================
 * EPT Memory Types
 *============================================================================*/

nexusvm_result_t ept_set_memory_type(void *root, guest_addr_t guest_phys, 
                                      u64 size, u8 memory_type)
{
    if (!root || memory_type > 6) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    u64 pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
    
    for (u64 i = 0; i < pages; i++) {
        guest_addr_t gpa = guest_phys + (i << PAGE_SHIFT);
        ept_pte_t pte;
        
        if (ept_lookup(root, gpa, &pte) == NEXUSVM_OK) {
            pte.memory_type = memory_type;
            /* Update would require re-mapping */
        }
    }
    
    ept_invalidate(root, guest_phys);
    
    return NEXUSVM_OK;
}

/*============================================================================
 * EPT Accessed/Dirty Bits
 *============================================================================*/

void ept_enable_accessed_dirty(void *root, bool enable)
{
    /* This is configured in the EPTP, not per-entry */
    /* The CPU automatically sets accessed/dirty bits */
}

nexusvm_result_t ept_get_accessed_dirty(void *root, guest_addr_t guest_phys,
                                          bool *accessed, bool *dirty)
{
    if (!root) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    ept_pte_t pte;
    nexusvm_result_t result = ept_lookup(root, guest_phys, &pte);
    if (result != NEXUSVM_OK) {
        return result;
    }
    
    if (accessed) *accessed = pte.accessed;
    if (dirty) *dirty = pte.dirty;
    
    return NEXUSVM_OK;
}

nexusvm_result_t ept_clear_accessed_dirty(void *root, guest_addr_t guest_phys)
{
    /* Note: In a real hypervisor, you'd need to track and clear these */
    /* The CPU sets them, but clearing requires more complex logic */
    return NEXUSVM_OK;
}

/*============================================================================
 * Large Page Support
 *============================================================================*/

nexusvm_result_t ept_map_2mb(void *root, guest_addr_t guest_phys, phys_addr_t host_phys)
{
    if (!IS_ALIGNED(guest_phys, HUGE_PAGE_SIZE) ||
        !IS_ALIGNED(host_phys, HUGE_PAGE_SIZE)) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    ept_pml4e_t *pml4 = (ept_pml4e_t *)root;
    u32 pml4_idx = EPT_PML4_INDEX(guest_phys);
    
    if (!pml4[pml4_idx].present) {
        return NEXUSVM_ERR_NOT_FOUND;
    }
    
    ept_pdpe_t *pdpt = (ept_pdpe_t *)(pml4[pml4_idx].physical_address << 12);
    u32 pdpt_idx = EPT_PDP_INDEX(guest_phys);
    
    if (!pdpt[pdpt_idx].present) {
        return NEXUSVM_ERR_NOT_FOUND;
    }
    
    ept_pue_t *pd = (ept_pue_t *)(pdpt[pdpt_idx].physical_address << 12);
    u32 pd_idx = EPT_PD_INDEX(guest_phys);
    
    /* Setup 2MB page entry */
    pd[pd_idx].present = 1;
    pd[pd_idx].write = 1;
    pd[pd_idx].executable = 0;
    pd[pd_idx].memory_type = EPT_MEMORY_TYPE_WRITE_BACK;
    pd[pd_idx].big_pagesize = 1;
    pd[pd_idx].physical_address = (host_phys >> 21) & 0xFFFFFFFFFF;
    
    return NEXUSVM_OK;
}

nexusvm_result_t ept_map_1gb(void *root, guest_addr_t guest_phys, phys_addr_t host_phys)
{
    if (!IS_ALIGNED(guest_phys, GIG_PAGE_SIZE) ||
        !IS_ALIGNED(host_phys, GIG_PAGE_SIZE)) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    ept_pml4e_t *pml4 = (ept_pml4e_t *)root;
    u32 pml4_idx = EPT_PML4_INDEX(guest_phys);
    
    if (!pml4[pml4_idx].present) {
        return NEXUSVM_ERR_NOT_FOUND;
    }
    
    ept_pdpe_t *pdpt = (ept_pdpe_t *)(pml4[pml4_idx].physical_address << 12);
    u32 pdpt_idx = EPT_PDP_INDEX(guest_phys);
    
    /* Setup 1GB page entry */
    pdpt[pdpt_idx].present = 1;
    pdpt[pdpt_idx].write = 1;
    pdpt[pdpt_idx].executable = 0;
    pdpt[pdpt_idx].memory_type = EPT_MEMORY_TYPE_WRITE_BACK;
    pdpt[pdpt_idx].big_pagesize = 1;
    pdpt[pdpt_idx].physical_address = (host_phys >> 30) & 0xFFFFFFFFFF;
    
    return NEXUSVM_OK;
}
