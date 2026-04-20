/**
 * NexusVM Extended Page Tables (EPT) Implementation
 * 
 * EPT provides hardware-assisted virtualization of guest physical addresses.
 * It enables second-level address translation from guest virtual addresses
 * to guest physical addresses, and then to host physical addresses.
 */

#ifndef NEXUSVM_EPT_H
#define NEXUSVM_EPT_H

#include <hypervisor/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * EPT Initialization
 *============================================================================*/

/**
 * Initialize EPT subsystem
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_init(void);

/**
 * Shutdown EPT subsystem
 */
void ept_shutdown(void);

/*============================================================================
 * EPT Page Table Management
 *============================================================================*/

/**
 * Allocate EPT root page table (PML4)
 * @return Pointer to EPT root, or NULL on failure
 */
void *ept_alloc_root(void);

/**
 * Free EPT root page table
 * @param root EPT root pointer
 */
void ept_free_root(void *root);

/**
 * Clear all EPT entries
 * @param root EPT root pointer
 */
void ept_clear_root(void *root);

/**
 * Allocate pages for EPT
 * @param count Number of pages
 * @param physical Output for physical address
 * @return Virtual address or NULL
 */
void *ept_alloc_pages(u64 count, phys_addr_t *physical);

/**
 * Free pages allocated by EPT
 * @param pages Page pointer
 */
void ept_free_pages(void *pages);

/*============================================================================
 * EPT Mapping
 *============================================================================*/

/**
 * Map a guest physical address to host physical address
 * @param root EPT root
 * @param guest_phys Guest physical address
 * @param pte Pointer to PTE to fill
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_map(void *root, guest_addr_t guest_phys, ept_pte_t *pte);

/**
 * Unmap guest physical address
 * @param root EPT root
 * @param guest_phys Guest physical address
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_unmap(void *root, guest_addr_t guest_phys);

/**
 * Lookup EPT entry for guest physical address
 * @param root EPT root
 * @param guest_phys Guest physical address
 * @param pte Output PTE
 * @return NEXUSVM_OK if found
 */
nexusvm_result_t ept_lookup(void *root, guest_addr_t guest_phys, ept_pte_t *pte);

/**
 * Translate guest physical to host physical address
 * @param root EPT root
 * @param guest_phys Guest physical address
 * @param host_phys Output host physical address
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_translate(void *root, guest_addr_t guest_phys, phys_addr_t *host_phys);

/*============================================================================
 * EPT Cache Operations
 *============================================================================*/

/**
 * Invalidate EPT mapping for a guest physical address
 * @param root EPT root
 * @param guest_phys Guest physical address
 */
void ept_invalidate(void *root, guest_addr_t guest_phys);

/**
 * Invalidate all EPT mappings (global)
 */
void ept_invalidate_all(void);

/**
 * INVEPT instruction wrapper
 * @param type Invalidation type (1=context, 2=global)
 */
void ept_invept(u64 type);

/*============================================================================
 * EPT Memory Types
 *============================================================================*/

/**
 * Set memory type for EPT entries
 * @param root EPT root
 * @param guest_phys Guest physical address
 * @param size Size of region
 * @param memory_type EPT memory type (0-6)
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_set_memory_type(void *root, guest_addr_t guest_phys, 
                                      u64 size, u8 memory_type);

/*============================================================================
 * EPT Accessed/Dirty Bits
 *============================================================================*/

/**
 * Enable accessed/dirty bit tracking in EPT
 * @param root EPT root
 * @param enable True to enable
 */
void ept_enable_accessed_dirty(void *root, bool enable);

/**
 * Get accessed/dirty bit status
 * @param root EPT root
 * @param guest_phys Guest physical address
 * @param accessed Output for accessed bit
 * @param dirty Output for dirty bit
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_get_accessed_dirty(void *root, guest_addr_t guest_phys,
                                          bool *accessed, bool *dirty);

/**
 * Clear accessed/dirty bits for a page
 * @param root EPT root
 * @param guest_phys Guest physical address
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_clear_accessed_dirty(void *root, guest_addr_t guest_phys);

/*============================================================================
 * Large Page Support
 *============================================================================*/

/**
 * Map using 2MB pages
 * @param root EPT root
 * @param guest_phys Guest physical address (must be 2MB aligned)
 * @param host_phys Host physical address
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_map_2mb(void *root, guest_addr_t guest_phys, phys_addr_t host_phys);

/**
 * Map using 1GB pages
 * @param root EPT root
 * @param guest_phys Guest physical address (must be 1GB aligned)
 * @param host_phys Host physical address
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t ept_map_1gb(void *root, guest_addr_t guest_phys, phys_addr_t host_phys);

#ifdef __cplusplus
}
#endif

#endif /* NEXUSVM_EPT_H */
