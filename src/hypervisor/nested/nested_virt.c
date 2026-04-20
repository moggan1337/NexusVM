/**
 * NexusVM Nested Virtualization Implementation
 */

#include <hypervisor/nested/nested_virt.h>
#include <hypervisor/hypervisor.h>
#include <hypervisor/vmx/vmx.h>
#include <hypervisor/amd-v/svm.h>
#include <utils/log.h>
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * Nested Virtualization Initialization
 *============================================================================*/

static bool g_nested_initialized = false;

nexusvm_result_t nested_virt_init(void)
{
    NEXUSVM_LOG_INFO("Initializing nested virtualization\n");
    
    hypervisor_t *hv = hypervisor_get_instance();
    if (!hv) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Check if CPU supports nested virtualization */
    if (hv->cpu_vendor == CPU_VENDOR_INTEL) {
        /* Check IA32_MISC_ENABLE MSR for VMX support */
        u64 misc_enable;
        __asm__ volatile (
            "mov $0x1A0, %%ecx\n\t"
            "rdmsr\n\t"
            : "=a" (misc_enable), "=d" (misc_enable >> 32)
        );
        
        if (!(misc_enable & (1 << 5))) {
            NEXUSVM_LOG_WARN("VMX appears disabled in firmware\n");
        }
    }
    
    g_nested_initialized = true;
    NEXUSVM_LOG_INFO("Nested virtualization initialized\n");
    
    return NEXUSVM_OK;
}

void nested_virt_shutdown(void)
{
    g_nested_initialized = false;
    NEXUSVM_LOG_INFO("Nested virtualization shutdown\n");
}

/*============================================================================
 * L2 VM Management
 *============================================================================*/

nexusvm_result_t nested_create_l2_vm(nexusvm_t *l1_vm, vm_config_t *l2_config,
                                      nexusvm_t **l2_vm)
{
    if (!l1_vm || !l2_config || !l2_vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Creating L2 VM for nested virtualization\n");
    
    /* Mark as nested VM */
    l2_config->nested_virtualization = true;
    
    nexusvm_result_t result = vm_create(l2_config, l2_vm);
    if (result != NEXUSVM_OK) {
        return result;
    }
    
    (*l2_vm)->is_nested_guest = true;
    
    return NEXUSVM_OK;
}

nexusvm_result_t nested_start_l2_vm(nexusvm_t *l2_vm)
{
    if (!l2_vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Starting L2 VM\n");
    
    return vm_start(l2_vm);
}

nexusvm_result_t nested_stop_l2_vm(nexusvm_t *l2_vm)
{
    if (!l2_vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Stopping L2 VM\n");
    
    return vm_stop(l2_vm);
}

/*============================================================================
 * VMX-in-VMX (Nested VMX)
 *============================================================================*/

nexusvm_result_t nested_vmx_enable(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    hypervisor_t *hv = hypervisor_get_instance();
    if (hv->cpu_vendor != CPU_VENDOR_INTEL) {
        return NEXUSVM_ERR_NOT_SUPPORTED;
    }
    
    /* Allocate nested VMX structures */
    nested_vmx_t *nested = (nested_vmx_t *)calloc(1, sizeof(nested_vmx_t));
    if (!nested) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Allocate L2 VMCS */
    nexusvm_result_t result = vmx_alloc_vmcs_region(&nested->vmcs_link_pointer);
    if (result != NEXUSVM_OK) {
        free(nested);
        return result;
    }
    
    vcpu->vm->nested_vmx = nested;
    
    NEXUSVM_LOG_INFO("Nested VMX enabled on vCPU %u\n", vcpu->vcpu_id);
    
    return NEXUSVM_OK;
}

void nested_vmx_disable(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vm->nested_vmx) {
        return;
    }
    
    nested_vmx_t *nested = vcpu->vm->nested_vmx;
    
    if (nested->vmcs_link_pointer) {
        vmx_free_vmcs_region(nested->vmcs_link_pointer);
    }
    
    free(nested);
    vcpu->vm->nested_vmx = NULL;
    
    NEXUSVM_LOG_INFO("Nested VMX disabled on vCPU %u\n", vcpu->vcpu_id);
}

nexusvm_result_t nested_vmx_setup_l1(nexusvm_t *l1_vm)
{
    if (!l1_vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Setting up L1 VM for nested VMX\n");
    
    /* Enable VMXON in secondary controls */
    /* L1 needs to be able to execute VMXON/VMXOFF */
    
    return NEXUSVM_OK;
}

nexusvm_result_t nested_vmx_handle_vmread(nexusvcpu_t *vcpu, u32 encoding, u64 *value)
{
    if (!vcpu || !value) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Check if we're in L2 guest mode */
    if (!vcpu->vm->nested_vmx || !vcpu->vm->nested_vmx->l1_guest_mode) {
        /* Normal VMREAD */
        *value = vmx_vmread(encoding);
        return NEXUSVM_OK;
    }
    
    /* L1 is reading from L2's VMCS - use shadow VMCS */
    /* ... shadow VMCS handling ... */
    
    return NEXUSVM_OK;
}

nexusvm_result_t nested_vmx_handle_vmwrite(nexusvcpu_t *vcpu, u32 encoding, u64 value)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Check if we're in L2 guest mode */
    if (!vcpu->vm->nested_vmx || !vcpu->vm->nested_vmx->l1_guest_mode) {
        /* Normal VMWRITE */
        vmx_vmwrite(encoding, value);
        return NEXUSVM_OK;
    }
    
    /* L1 is writing to L2's VMCS */
    /* ... shadow VMCS handling ... */
    
    return NEXUSVM_OK;
}

nexusvm_result_t nested_vmx_handle_vmxon(nexusvcpu_t *vcpu, void *vmxon_region)
{
    if (!vcpu || !vmxon_region) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("L1 executing VMXON\n");
    
    /* L1 is trying to enable VMX - we need to track this */
    if (vcpu->vm->nested_vmx) {
        vcpu->vm->nested_vmx->vmxon_pointer = (phys_addr_t)vmxon_region;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t nested_vmx_handle_vmxoff(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("L1 executing VMXOFF\n");
    
    if (vcpu->vm->nested_vmx) {
        vcpu->vm->nested_vmx->vmxon_pointer = 0;
        vcpu->vm->nested_vmx->l1_guest_mode = false;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t nested_vmx_handle_vmlaunch(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("L1 executing VMLAUNCH\n");
    
    if (vcpu->vm->nested_vmx) {
        vcpu->vm->nested_vmx->l1_guest_mode = true;
        
        /* L1 is launching L2 guest - enter guest mode */
        /* The actual VM entry to L2 will happen on next VM entry */
    }
    
    /* Continue with normal VM entry, but set L2 mode */
    return vmx_vmentry(vcpu);
}

nexusvm_result_t nested_vmx_handle_vmresume(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("L1 executing VMRESUME\n");
    
    /* Similar to VMLAUNCH but for resuming L2 */
    return vmx_vmentry(vcpu);
}

nexusvm_result_t nested_vmx_handle_l2_vmexit(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* L2 guest caused a VM exit - we need to deliver to L1 */
    u64 exit_reason = vmx_vmread(VMCS_VM_EXIT_REASON);
    u64 exit_qual = vmx_vmread(VMCS_VM_EXIT_QUALIFICATION);
    
    NEXUSVM_LOG_DEBUG("L2 VM exit: reason=0x%lx, qual=0x%lx\n",
                      exit_reason, exit_qual);
    
    /* Save L2 state */
    nested_save_l2_state(vcpu);
    
    /* Inject VM exit into L1 as appropriate */
    /* ... L1 VM exit injection ... */
    
    return NEXUSVM_OK;
}

/*============================================================================
 * SVM-in-SVM (Nested SVM)
 *============================================================================*/

nexusvm_result_t nested_svm_enable(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    hypervisor_t *hv = hypervisor_get_instance();
    if (hv->cpu_vendor != CPU_VENDOR_AMD) {
        return NEXUSVM_ERR_NOT_SUPPORTED;
    }
    
    /* Allocate nested SVM structures */
    nested_svm_t *nested = (nested_svm_t *)calloc(1, sizeof(nested_svm_t));
    if (!nested) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    vcpu->vm->nested_svm = nested;
    
    NEXUSVM_LOG_INFO("Nested SVM enabled on vCPU %u\n", vcpu->vcpu_id);
    
    return NEXUSVM_OK;
}

void nested_svm_disable(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vm->nested_svm) {
        return;
    }
    
    free(vcpu->vm->nested_svm);
    vcpu->vm->nested_svm = NULL;
    
    NEXUSVM_LOG_INFO("Nested SVM disabled on vCPU %u\n", vcpu->vcpu_id);
}

nexusvm_result_t nested_svm_handle_vmrun(nexusvcpu_t *vcpu, phys_addr_t vmcb_pa)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("L1 executing VMRUN with VMCB PA: 0x%lx\n", vmcb_pa);
    
    if (vcpu->vm->nested_svm) {
        vcpu->vm->nested_svm->vmcb = (void *)vmcb_pa;
        vcpu->vm->nested_svm->nested_running = true;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t nested_svm_handle_vmmcall(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("L1 executing VMMCALL\n");
    
    /* L1 hypercall - handle in L1 context */
    /* ... */
    
    return NEXUSVM_OK;
}

/*============================================================================
 * L2 State Management
 *============================================================================*/

nexusvm_result_t nested_save_l2_state(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    hypervisor_t *hv = hypervisor_get_instance();
    
    if (hv->cpu_vendor == CPU_VENDOR_INTEL && vcpu->vm->nested_vmx) {
        /* Save L2 guest state from VMCS to our L2 state structure */
        l2_state_t *l2 = (l2_state_t *)calloc(1, sizeof(l2_state_t));
        if (!l2) {
            return NEXUSVM_ERR_NO_MEMORY;
        }
        
        l2->guest_state.gprs.rax = vmx_vmread(VMCS_GUEST_RAX);
        l2->guest_state.gprs.rbx = vmx_vmread(VMCS_GUEST_RBX);
        l2->guest_state.gprs.rcx = vmx_vmread(VMCS_GUEST_RCX);
        l2->guest_state.gprs.rdx = vmx_vmread(VMCS_GUEST_RDX);
        l2->guest_state.gprs.rsi = vmx_vmread(VMCS_GUEST_RSI);
        l2->guest_state.gprs.rdi = vmx_vmread(VMCS_GUEST_RDI);
        l2->guest_state.gprs.rbp = vmx_vmread(VMCS_GUEST_RBP);
        l2->guest_state.gprs.rsp = vmx_vmread(VMCS_GUEST_RSP);
        l2->guest_state.gprs.rip = vmx_vmread(VMCS_GUEST_RIP);
        l2->guest_state.gprs.rflags = vmx_vmread(VMCS_GUEST_RFLAGS);
        
        l2->guest_state.cr.cr0 = vmx_vmread(VMCS_GUEST_CR0);
        l2->guest_state.cr.cr2 = vmx_vmread(VMCS_GUEST_CR2);
        l2->guest_state.cr.cr3 = vmx_vmread(VMCS_GUEST_CR3);
        l2->guest_state.cr.cr4 = vmx_vmread(VMCS_GUEST_CR4);
        
        l2->vmexit_reason = vmx_vmread(VMCS_VM_EXIT_REASON);
        l2->vmexit_qualification = vmx_vmread(VMCS_VM_EXIT_QUALIFICATION);
        l2->vmexit_instruction_len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
        
        /* Store in vcpu for later restoration */
        vcpu->cpu_state.gprs.r12 = (u64)l2; /* Save pointer in unused register */
        
        NEXUSVM_LOG_DEBUG("L2 state saved\n");
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t nested_restore_l2_state(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    hypervisor_t *hv = hypervisor_get_instance();
    
    if (hv->cpu_vendor == CPU_VENDOR_INTEL && vcpu->vm->nested_vmx) {
        /* Retrieve saved L2 state */
        l2_state_t *l2 = (l2_state_t *)vcpu->cpu_state.gprs.r12;
        if (!l2) {
            return NEXUSVM_ERR_GENERIC;
        }
        
        /* Restore L2 guest state to VMCS */
        vmx_vmwrite(VMCS_GUEST_RAX, l2->guest_state.gprs.rax);
        vmx_vmwrite(VMCS_GUEST_RBX, l2->guest_state.gprs.rbx);
        vmx_vmwrite(VMCS_GUEST_RCX, l2->guest_state.gprs.rcx);
        vmx_vmwrite(VMCS_GUEST_RDX, l2->guest_state.gprs.rdx);
        vmx_vmwrite(VMCS_GUEST_RSI, l2->guest_state.gprs.rsi);
        vmx_vmwrite(VMCS_GUEST_RDI, l2->guest_state.gprs.rdi);
        vmx_vmwrite(VMCS_GUEST_RBP, l2->guest_state.gprs.rbp);
        vmx_vmwrite(VMCS_GUEST_RSP, l2->guest_state.gprs.rsp);
        vmx_vmwrite(VMCS_GUEST_RIP, l2->guest_state.gprs.rip);
        vmx_vmwrite(VMCS_GUEST_RFLAGS, l2->guest_state.gprs.rflags);
        
        vmx_vmwrite(VMCS_GUEST_CR0, l2->guest_state.cr.cr0);
        vmx_vmwrite(VMCS_GUEST_CR2, l2->guest_state.cr.cr2);
        vmx_vmwrite(VMCS_GUEST_CR3, l2->guest_state.cr.cr3);
        vmx_vmwrite(VMCS_GUEST_CR4, l2->guest_state.cr.cr4);
        
        free(l2);
        vcpu->cpu_state.gprs.r12 = 0;
        
        NEXUSVM_LOG_DEBUG("L2 state restored\n");
    }
    
    return NEXUSVM_OK;
}

void nested_get_status(nexusvcpu_t *vcpu, bool *is_nested, bool *is_guest)
{
    if (!vcpu) return;
    
    hypervisor_t *hv = hypervisor_get_instance();
    
    if (hv->cpu_vendor == CPU_VENDOR_INTEL) {
        if (is_nested) *is_nested = (vcpu->vm->nested_vmx != NULL);
        if (is_guest) *is_guest = 
            (vcpu->vm->nested_vmx && vcpu->vm->nested_vmx->l1_guest_mode);
    } else if (hv->cpu_vendor == CPU_VENDOR_AMD) {
        if (is_nested) *is_nested = (vcpu->vm->nested_svm != NULL);
        if (is_guest) *is_guest = 
            (vcpu->vm->nested_svm && vcpu->vm->nested_svm->nested_running);
    } else {
        if (is_nested) *is_nested = false;
        if (is_guest) *is_guest = false;
    }
}

/*============================================================================
 * VMFUNC Support
 *============================================================================*/

nexusvm_result_t nested_handle_vmfunc(nexusvcpu_t *vcpu, u32 function)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("VMFUNC function: %u\n", function);
    
    /* VMFUNC functions:
     * 0: EPTP switching (invalidate single context)
     * Others: reserved or hypervisor-defined
     */
    
    switch (function) {
        case 0:
            /* EPTP switching - change EPT pointer */
            /* Would load new EPTP from registers */
            return NEXUSVM_OK;
        default:
            NEXUSVM_LOG_WARN("Unsupported VMFUNC: %u\n", function);
            return NEXUSVM_ERR_NOT_SUPPORTED;
    }
}
