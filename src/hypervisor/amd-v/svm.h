/**
 * NexusVM AMD-V (SVM) Implementation
 */

#ifndef NEXUSVM_SVM_H
#define NEXUSVM_SVM_H

#include <hypervisor/types.h>
#include <stdbool.h>

#ifdef __amd64__

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * SVM Operations
 *============================================================================*/

/**
 * Enable SVM (AMD-V) operation on current CPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t svm_enable(void);

/**
 * Disable SVM operation on current CPU
 */
void svm_disable(void);

/**
 * Check if SVM is enabled
 * @return true if SVM is enabled
 */
bool svm_is_enabled(void);

/**
 * Check if SVM is supported on this CPU
 * @return true if SVM is supported
 */
bool svm_is_supported(void);

/*============================================================================
 * VMCB Management
 *============================================================================*/

/**
 * Allocate VMCB (Virtual Machine Control Block)
 * @param vmcb Output for allocated VMCB
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t svm_alloc_vmcb(void **vmcb);

/**
 * Free VMCB
 * @param vmcb VMCB to free
 */
void svm_free_vmcb(void *vmcb);

/**
 * Allocate HSAVE area for nested virtualization
 * @param region Output for allocated region
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t svm_alloc_hsave(void **region);

/**
 * Free HSAVE area
 * @param region Region to free
 */
void svm_free_hsave(void *region);

/*============================================================================
 * VMCB Operations
 *============================================================================*/

/**
 * Run guest using VMCB
 * @param vmcb VMCB to run
 * @return NEXUSVM_OK on clean exit
 */
nexusvm_result_t svm_vmrun(void *vmcb);

/**
 * VMLOAD - Load state from VMCB
 * @param vmcb Source VMCB
 */
void svm_vmload(void *vmcb);

/**
 * VMSAVE - Save state to VMCB
 * @param vmcb Destination VMCB
 */
void svm_vmsave(void *vmcb);

/**
 * CLGI - Clear global interrupt flag
 */
void svm_clgi(void);

/**
 * STGI - Set global interrupt flag
 */
void svm_stgi(void);

/**
 * VMMCALL - VM call to hypervisor
 */
void svm_vmmcall(void);

/*============================================================================
 * Intercept Control
 *============================================================================*/

/**
 * Set intercept bit in VMCB
 * @param vmcb VMCB
 * @param offset Offset in intercept area
 * @param bit Bit number
 * @param enable Enable or disable
 */
void svm_set_intercept(void *vmcb, u32 offset, u32 bit, bool enable);

/**
 * Set I/O bitmap
 * @param vmcb VMCB
 * @param bitmap_pa Physical address of I/O bitmap
 */
void svm_set_io_bitmap(void *vmcb, phys_addr_t bitmap_pa);

/**
 * Set MSR bitmap
 * @param vmcb VMCB
 * @param bitmap_pa Physical address of MSR bitmap
 */
void svm_set_msr_bitmap(void *vmcb, phys_addr_t bitmap_pa);

/*============================================================================
 * Nested Page Table (NPT) Operations
 *============================================================================*/

/**
 * Set NPT pointer in VMCB
 * @param vmcb VMCB
 * @param ncr3 NPT CR3 value
 */
void svm_set_npt(void *vmcb, u64 ncr3);

/*============================================================================
 * vCPU SVM Operations
 *============================================================================*/

/**
 * Initialize vCPU for SVM
 * @param vcpu vCPU to initialize
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t svm_vcpu_init(nexusvcpu_t *vcpu);

/**
 * Setup vCPU VMCB
 * @param vcpu vCPU to setup
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t svm_vcpu_setup(nexusvcpu_t *vcpu);

/**
 * Run vCPU in SVM mode
 * @param vcpu vCPU to run
 * @return NEXUSVM_OK on clean exit
 */
nexusvm_result_t svm_vcpu_run(nexusvcpu_t *vcpu);

/**
 * Cleanup vCPU SVM resources
 * @param vcpu vCPU to cleanup
 */
void svm_vcpu_cleanup(nexusvcpu_t *vcpu);

/*============================================================================
 * SVM Exit Handling
 *============================================================================*/

/**
 * Handle SVM exit
 * @param vmcb VMCB with exit info
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t svm_handle_exit(void *vmcb);

/**
 * Get exit code from VMCB
 * @param vmcb VMCB
 * @return Exit code
 */
u64 svm_get_exit_code(void *vmcb);

/**
 * Get exit info 1
 * @param vmcb VMCB
 * @return Exit info 1
 */
u64 svm_get_exit_info1(void *vmcb);

/**
 * Get exit info 2
 * @param vmcb VMCB
 * @return Exit info 2
 */
u64 svm_get_exit_info2(void *vmcb);

#ifdef __cplusplus
}
#endif

#endif /* __amd64__ */

#endif /* NEXUSVM_SVM_H */
