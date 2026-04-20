/**
 * NexusVM VMX (Intel VT-x) Implementation
 */

#ifndef NEXUSVM_VMX_H
#define NEXUSVM_VMX_H

#include <hypervisor/types.h>
#include <stdbool.h>

#ifdef __x86_64__

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * VMX Operations
 *============================================================================*/

/**
 * Enable VMX operation on current CPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_enable(void);

/**
 * Disable VMX operation on current CPU
 */
void vmx_disable(void);

/**
 * Check if VMX is enabled
 * @return true if VMX is enabled
 */
bool vmx_is_enabled(void);

/**
 * Check if VMX is supported on this CPU
 * @return true if VMX is supported
 */
bool vmx_is_supported(void);

/*============================================================================
 * VMX Region Management
 *============================================================================*/

/**
 * Allocate VMXON region
 * @param region Output for allocated region
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_alloc_vmxon_region(void **region);

/**
 * Free VMXON region
 * @param region Region to free
 */
void vmx_free_vmxon_region(void *region);

/**
 * Allocate VMCS region
 * @param region Output for allocated region
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_alloc_vmcs_region(void **region);

/**
 * Free VMCS region
 * @param region Region to free
 */
void vmx_free_vmcs_region(void *region);

/*============================================================================
 * VMXON/VMCS Operations
 *============================================================================*/

/**
 * Enter VMX operation (VMXON)
 * @param vcpu vCPU context
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_on(nexusvcpu_t *vcpu);

/**
 * Exit VMX operation (VMXOFF)
 * @param vcpu vCPU context
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_off(nexusvcpu_t *vcpu);

/**
 * Initialize VMCS for vCPU
 * @param vcpu vCPU to initialize
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_vmcs_init(nexusvcpu_t *vcpu);

/**
 * Clear VMCS
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_vmcs_clear(nexusvcpu_t *vcpu);

/**
 * Current VMCS pointer
 * @param vcpu vCPU
 * @return Current VMCS physical address
 */
phys_addr_t vmx_vmcs_current(nexusvcpu_t *vcpu);

/*============================================================================
 * VMCS Read/Write
 *============================================================================*/

/**
 * Read VMCS field
 * @param encoding VMCS field encoding
 * @return Field value
 */
u64 vmx_vmread(u32 encoding);

/**
 * Write VMCS field
 * @param encoding VMCS field encoding
 * @param value Value to write
 */
void vmx_vmwrite(u32 encoding, u64 value);

/*============================================================================
 * vCPU VMX Operations
 *============================================================================*/

/**
 * Initialize vCPU for VMX
 * @param vcpu vCPU to initialize
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_vcpu_init(nexusvcpu_t *vcpu);

/**
 * Setup vCPU VMCS
 * @param vcpu vCPU to setup
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_vcpu_setup(nexusvcpu_t *vcpu);

/**
 * Run vCPU in VMX mode
 * @param vcpu vCPU to run
 * @return NEXUSVM_OK on clean exit
 */
nexusvm_result_t vmx_vcpu_run(nexusvcpu_t *vcpu);

/**
 * Cleanup vCPU VMX resources
 * @param vcpu vCPU to cleanup
 */
void vmx_vcpu_cleanup(nexusvcpu_t *vcpu);

/*============================================================================
 * VMX Capability Reading
 *============================================================================*/

/**
 * Read VMX capability MSR
 * @param msr MSR number
 * @return MSR value
 */
u64 vmx_read_capability(u32 msr);

/**
 * Get VMX basic info
 * @return VMX basic MSR value
 */
vmx_basic_msr_t vmx_get_basic_info(void);

/*============================================================================
 * VM-Entry and VM-Exit
 *============================================================================*/

/**
 * Perform VM entry
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_vmentry(nexusvcpu_t *vcpu);

/**
 * Handle VM exit
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vmx_vmexit(nexusvcpu_t *vcpu);

/*============================================================================
 * EPTP Setup
 *============================================================================*/

/**
 * Set EPT pointer in VMCS
 * @param vcpu vCPU
 * @param eptp EPT pointer value
 */
void vmx_set_eptp(nexusvcpu_t *vcpu, eptp_t *eptp);

/*============================================================================
 * MSR Bitmap Management
 *============================================================================*/

/**
 * Enable MSR interception for read
 * @param vcpu vCPU
 * @param msr MSR number
 */
void vmx_enable_msr_read_intercept(nexusvcpu_t *vcpu, u32 msr);

/**
 * Disable MSR interception for read
 * @param vcpu vCPU
 * @param msr MSR number
 */
void vmx_disable_msr_read_intercept(nexusvcpu_t *vcpu, u32 msr);

/**
 * Enable MSR interception for write
 * @param vcpu vCPU
 * @param msr MSR number
 */
void vmx_enable_msr_write_intercept(nexusvcpu_t *vcpu, u32 msr);

/**
 * Disable MSR interception for write
 * @param vcpu vCPU
 * @param msr MSR number
 */
void vmx_disable_msr_write_intercept(nexusvcpu_t *vcpu, u32 msr);

/*============================================================================
 * I/O Bitmap Management
 *============================================================================*/

/**
 * Enable I/O port interception
 * @param vcpu vCPU
 * @param port I/O port
 */
void vmx_enable_io_port_intercept(nexusvcpu_t *vcpu, u16 port);

/**
 * Disable I/O port interception
 * @param vcpu vCPU
 * @param port I/O port
 */
void vmx_disable_io_port_intercept(nexusvcpu_t *vcpu, u16 port);

#ifdef __cplusplus
}
#endif

#endif /* __x86_64__ */

#endif /* NEXUSVM_VMX_H */
