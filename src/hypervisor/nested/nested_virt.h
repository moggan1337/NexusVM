/**
 * NexusVM Nested Virtualization Implementation
 * 
 * Supports running VMs inside VMs (L1 hypervisor running L2 guests)
 */

#ifndef NEXUSVM_NESTED_H
#define NEXUSVM_NESTED_H

#include <hypervisor/types.h>
#include <hypervisor/hypervisor.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Nested Virtualization Initialization
 *============================================================================*/

/**
 * Initialize nested virtualization support
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_virt_init(void);

/**
 * Shutdown nested virtualization
 */
void nested_virt_shutdown(void);

/*============================================================================
 * L2 VM Management
 *============================================================================*/

/**
 * Create an L2 VM for nested virtualization
 * @param l1_vm L1 VM (our hypervisor's VM)
 * @param l2_config L2 VM configuration
 * @param l2_vm Output L2 VM pointer
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_create_l2_vm(nexusvm_t *l1_vm, vm_config_t *l2_config,
                                      nexusvm_t **l2_vm);

/**
 * Start L2 VM
 * @param l2_vm L2 VM to start
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_start_l2_vm(nexusvm_t *l2_vm);

/**
 * Stop L2 VM
 * @param l2_vm L2 VM to stop
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_stop_l2_vm(nexusvm_t *l2_vm);

/*============================================================================
 * VMX-in-VMX (Nested VMX)
 *============================================================================*/

/**
 * Enable nested VMX on a vCPU
 * @param vcpu vCPU to enable nested VMX on
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_enable(nexusvcpu_t *vcpu);

/**
 * Disable nested VMX on a vCPU
 * @param vcpu vCPU
 */
void nested_vmx_disable(nexusvcpu_t *vcpu);

/**
 * Setup L1 VM for running L2 guests
 * @param l1_vm L1 VM
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_setup_l1(nexusvm_t *l1_vm);

/**
 * Handle VMX VMCS-read by L1
 * @param vcpu vCPU
 * @param encoding VMCS field encoding
 * @param value Output value
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_handle_vmread(nexusvcpu_t *vcpu, u32 encoding, u64 *value);

/**
 * Handle VMX VMCS-write by L1
 * @param vcpu vCPU
 * @param encoding VMCS field encoding
 * @param value Value to write
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_handle_vmwrite(nexusvcpu_t *vcpu, u32 encoding, u64 value);

/**
 * Handle VMXON by L1
 * @param vcpu vCPU
 * @param vmxon_region L1's VMXON region
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_handle_vmxon(nexusvcpu_t *vcpu, void *vmxon_region);

/**
 * Handle VMOFF by L1
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_handle_vmxoff(nexusvcpu_t *vcpu);

/**
 * Handle VMLAUNCH by L1
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_handle_vmlaunch(nexusvcpu_t *vcpu);

/**
 * Handle VMRESUME by L1
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_handle_vmresume(nexusvcpu_t *vcpu);

/**
 * Handle VMX exit from L2 guest (to L1 hypervisor)
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_vmx_handle_l2_vmexit(nexusvcpu_t *vcpu);

/*============================================================================
 * SVM-in-SVM (Nested SVM)
 *============================================================================*/

/**
 * Enable nested SVM on a vCPU
 * @param vcpu vCPU to enable nested SVM on
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_svm_enable(nexusvcpu_t *vcpu);

/**
 * Disable nested SVM on a vCPU
 * @param vcpu vCPU
 */
void nested_svm_disable(nexusvcpu_t *vcpu);

/**
 * Handle VMRUN by L1
 * @param vcpu vCPU
 * @param vmcb_pa L1's VMCB physical address
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_svm_handle_vmrun(nexusvcpu_t *vcpu, phys_addr_t vmcb_pa);

/**
 * Handle VMMCALL by L1
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_svm_handle_vmmcall(nexusvcpu_t *vcpu);

/*============================================================================
 * L2 State Management
 *============================================================================*/

/**
 * Save L2 state on VM exit
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_save_l2_state(nexusvcpu_t *vcpu);

/**
 * Restore L2 state on VM entry
 * @param vcpu vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_restore_l2_state(nexusvcpu_t *vcpu);

/**
 * Get nested virtualization status
 * @param vcpu vCPU
 * @param is_nested Output: is nested VMX/SVM active
 * @param is_guest Output: is in guest (L2) mode
 */
void nested_get_status(nexusvcpu_t *vcpu, bool *is_nested, bool *is_guest);

/*============================================================================
 * VMFUNC Support
 *============================================================================*/

/**
 * Handle VMFUNC instruction
 * @param vcpu vCPU
 * @param function VMFUNC function number
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t nested_handle_vmfunc(nexusvcpu_t *vcpu, u32 function);

#ifdef __cplusplus
}
#endif

#endif /* NEXUSVM_NESTED_H */
