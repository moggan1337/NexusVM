/**
 * NexusVM VMX (Intel VT-x) Implementation
 */

#include <hypervisor/vmx/vmx.h>
#include <hypervisor/hypervisor.h>
#include <hypervisor/mmu/ept.h>
#include <utils/log.h>
#include <utils/assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* VMX feature detection */
static bool vmx_detected = false;
static bool vmx_enabled = false;

/*============================================================================
 * VMX Assembly Routines (implemented in asm.S)
 *============================================================================*/

extern void vmx_on_asm(void *vmx_region);
extern void vmx_off_asm(void);
extern void vmx_vmread_asm(u32 encoding, u64 *value);
extern void vmx_vmwrite_asm(u32 encoding, u64 value);
extern u64 vmx_vmcall_asm(u64 rax, u64 rbx, u64 rcx, u64 rdx);
extern void vmx_vmlaunch_asm(void);
extern void vmx_vmresume_asm(void);
extern u64 vmx_vmread_safe_asm(u32 encoding, int *error);
extern int vmx_vmwrite_safe_asm(u32 encoding, u64 value);

/*============================================================================
 * VMX Capability Detection
 *============================================================================*/

void detect_vmx_features(hypervisor_t *hv)
{
    /* Check CPUID for VMX support */
    u32 eax, ebx, ecx, edx;
    
    __asm__ volatile (
        "mov $1, %%eax\n\t"
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        :
        : "memory"
    );
    
    hv->has_vmx = (ecx >> 5) & 1;
    
    if (!hv->has_vmx) {
        NEXUSVM_LOG_WARN("VMX not supported on this CPU\n");
        return;
    }
    
    /* Read VMX capability MSRs */
    hv->vmx_basic.revision_id = (u32)vmx_read_capability(MSR_IA32_VMX_BASIC);
    hv->vmx_cap_pinbased = vmx_read_capability(MSR_IA32_VMX_PINBASED_CTLS);
    hv->vmx_cap_procbased = vmx_read_capability(MSR_IA32_VMX_PROCBASED_CTLS);
    hv->vmx_cap_procbased2 = vmx_read_capability(MSR_IA32_VMX_SECONDARY_CTLS);
    hv->vmx_cap_exit = vmx_read_capability(MSR_IA32_VMX_VMEXIT_CTLS);
    hv->vmx_cap_entry = vmx_read_capability(MSR_IA32_VMX_VMENTRY_CTLS);
    hv->vmx_cap_ept_vpid = vmx_read_capability(MSR_IA32_VMX_EPT_VPID_CAP);
    
    /* Check for EPT support */
    hv->has_ept = (hv->vmx_cap_procbased2 >> 1) & 1;
    
    /* Check for VPID support */
    hv->has_vpid = (hv->vmx_cap_procbased2 >> 5) & 1;
    
    /* Check for unrestricted guest */
    hv->has_vpid = (hv->vmx_cap_procbased2 >> 7) & 1;
    
    /* Check forEPT accessed/dirty bits */
    bool ept_ad = (hv->vmx_cap_ept_vpid >> 6) & 1;
    bool ept_wb = (hv->vmx_cap_ept_vpid >> 14) & 1;
    
    NEXUSVM_LOG_INFO("VMX Basic Revision: 0x%08x\n", hv->vmx_basic.revision_id);
    NEXUSVM_LOG_INFO("VMX PIN controls: 0x%016lx\n", hv->vmx_cap_pinbased);
    NEXUSVM_LOG_INFO("VMX PROC controls: 0x%016lx\n", hv->vmx_cap_procbased);
    NEXUSVM_LOG_INFO("VMX PROC2 controls: 0x%016lx\n", hv->vmx_cap_procbased2);
    NEXUSVM_LOG_INFO("VMX EPT/VPID cap: 0x%016lx\n", hv->vmx_cap_ept_vpid);
    
    vmx_detected = true;
}

/*============================================================================
 * VMX MSR Access
 *============================================================================*/

u64 vmx_read_capability(u32 msr)
{
    u32 edx, eax;
    
    __asm__ volatile (
        "rdmsr\n\t"
        : "=a" (eax), "=d" (edx)
        : "c" (msr)
    );
    
    return ((u64)edx << 32) | eax;
}

vmx_basic_msr_t vmx_get_basic_info(void)
{
    vmx_basic_msr_t info;
    u64 value = vmx_read_capability(MSR_IA32_VMX_BASIC);
    
    info.revision_id = (u32)value;
    info.vmx_on_info = (u32)(value >> 32);
    info.vmcs_physical_addr_width = (value >> 48) & 0xFF;
    
    return info;
}

/*============================================================================
 * VMX Enable/Disable
 *============================================================================*/

nexusvm_result_t vmx_enable(void)
{
    if (!vmx_detected) {
        return NEXUSVM_ERR_CPU_NOT_SUPPORTED;
    }
    
    /* Read CR4 */
    u64 cr4;
    __asm__ volatile (
        "mov %%cr4, %%rax\n\t"
        "mov %%rax, %0\n\t"
        : "=r" (cr4)
    );
    
    /* Enable VMX */
    cr4 |= (1 << 13); /* CR4.VMXE */
    
    /* Write CR4 */
    __asm__ volatile (
        "mov %0, %%rax\n\t"
        "mov %%rax, %%cr4\n\t"
        :
        : "r" (cr4)
        : "rax"
    );
    
    vmx_enabled = true;
    NEXUSVM_LOG_INFO("VMX enabled\n");
    
    return NEXUSVM_OK;
}

void vmx_disable(void)
{
    u64 cr4;
    __asm__ volatile (
        "mov %%cr4, %%rax\n\t"
        "mov %%rax, %0\n\t"
        : "=r" (cr4)
    );
    
    cr4 &= ~(1 << 13);
    
    __asm__ volatile (
        "mov %0, %%rax\n\t"
        "mov %%rax, %%cr4\n\t"
        :
        : "r" (cr4)
        : "rax"
    );
    
    vmx_enabled = false;
}

bool vmx_is_enabled(void)
{
    u64 cr4;
    __asm__ volatile (
        "mov %%cr4, %%rax\n\t"
        "mov %%rax, %0\n\t"
        : "=r" (cr4)
    );
    
    return (cr4 >> 13) & 1;
}

bool vmx_is_supported(void)
{
    u32 ecx;
    __asm__ volatile (
        "mov $1, %%eax\n\t"
        "cpuid\n\t"
        : "=c" (ecx)
        :
        : "eax", "ebx", "edx"
    );
    
    return (ecx >> 5) & 1;
}

/*============================================================================
 * VMX Region Management
 *============================================================================*/

nexusvm_result_t vmx_alloc_vmxon_region(void **region)
{
    vmx_basic_msr_t basic = vmx_get_basic_info();
    
    /* Check if region size is > 4096 */
    u32 region_size = basic.vmx_on_info & 0x1FFF;
    if (region_size < 4096) {
        region_size = 4096;
    }
    
    /* Allocate aligned memory */
    void *r = aligned_alloc(4096, region_size);
    if (!r) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    memset(r, 0, region_size);
    
    /* Set VMCS revision ID */
    *(u32 *)r = basic.revision_id;
    
    *region = r;
    return NEXUSVM_OK;
}

void vmx_free_vmxon_region(void *region)
{
    if (region) {
        free(region);
    }
}

nexusvm_result_t vmx_alloc_vmcs_region(void **region)
{
    vmx_basic_msr_t basic = vmx_get_basic_info();
    
    u32 region_size = basic.vmx_on_info & 0x1FFF;
    if (region_size < 4096) {
        region_size = 4096;
    }
    
    void *r = aligned_alloc(4096, region_size);
    if (!r) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    memset(r, 0, region_size);
    *(u32 *)r = basic.revision_id;
    
    *region = r;
    return NEXUSVM_OK;
}

void vmx_free_vmcs_region(void *region)
{
    if (region) {
        free(region);
    }
}

/*============================================================================
 * VMXON/VMCS Operations
 *============================================================================*/

nexusvm_result_t vmx_on(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmx_on_region) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    phys_addr_t phys = (phys_addr_t)vcpu->vmx_on_region;
    
    __asm__ volatile (
        "mov %1, %%rax\n\t"
        "vmx_on %%rax\n\t"
        "jnz .Lvmx_fail\n\t"
        "mov $0, %0\n\t"
        "jmp .Lvmx_done\n\t"
        ".Lvmx_fail:\n\t"
        "mov $-1, %0\n\t"
        ".Lvmx_done:\n\t"
        : "=r" (vcpu->vcpu_id) /* result placeholder */
        : "r" (phys)
        : "rax", "cc", "memory"
    );
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_off(nexusvcpu_t *vcpu)
{
    __asm__ volatile (
        "vmx_off\n\t"
        :
        :
        : "cc", "memory"
    );
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_vmcs_init(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmx_vmcs_region) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    phys_addr_t phys = (phys_addr_t)vcpu->vmx_vmcs_region;
    
    __asm__ volatile (
        "mov %1, %%rax\n\t"
        "vmptrst %%rax\n\t"
        "test %%rax, %%rax\n\t"
        "jnz .Lvmcs_init_fail\n\t"
        "mov $0, %0\n\t"
        "jmp .Lvmcs_init_done\n\t"
        ".Lvmcs_init_fail:\n\t"
        "mov $-1, %0\n\t"
        ".Lvmcs_init_done:\n\t"
        : "=r" (vcpu->vcpu_id)
        : "r" (phys)
        : "rax", "cc", "memory"
    );
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_vmcs_clear(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmx_vmcs_region) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    phys_addr_t phys = (phys_addr_t)vcpu->vmx_vmcs_region;
    int result;
    
    __asm__ volatile (
        "mov %2, %%rax\n\t"
        "vmclear %%rax\n\t"
        "setz %0\n\t"
        : "=r" (result)
        : "a" (phys)
        : "r" (phys)
        : "cc", "memory"
    );
    
    return result ? NEXUSVM_OK : NEXUSVM_ERR_VMX_FAILED;
}

phys_addr_t vmx_vmcs_current(nexusvcpu_t *vcpu)
{
    phys_addr_t current;
    
    __asm__ volatile (
        "vmptrst %0\n\t"
        : "=m" (current)
        :
        : "cc", "memory"
    );
    
    return current;
}

/*============================================================================
 * VMCS Read/Write
 *============================================================================*/

u64 vmx_vmread(u32 encoding)
{
    u64 value;
    int error;
    
    __asm__ volatile (
        "1: mov %2, %%rax\n\t"
        ".byte 0x0f, 0x78, 0xc0\n\t"
        "mov %%rax, %1\n\t"
        "mov $0, %0\n\t"
        "jmp 2f\n\t"
        "2:\n\t"
        ".section .data\n\t"
        ".Lvmread_error:\n\t"
        ".byte 0\n\t"
        ".previous\n\t"
        : "=r" (error), "=r" (value)
        : "r" ((u64)encoding)
        : "rax", "cc", "memory"
    );
    
    return value;
}

void vmx_vmwrite(u32 encoding, u64 value)
{
    __asm__ volatile (
        "mov %0, %%rax\n\t"
        "mov %1, %%rdx\n\t"
        ".byte 0x0f, 0x79, 0xc2\n\t"
        :
        : "r" ((u64)encoding), "r" (value)
        : "rax", "rdx", "cc", "memory"
    );
}

/*============================================================================
 * vCPU VMX Operations
 *============================================================================*/

nexusvm_result_t vmx_vcpu_init(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Allocate VMXON region */
    nexusvm_result_t result = vmx_alloc_vmxon_region(&vcpu->vmx_on_region);
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to allocate VMXON region\n");
        return result;
    }
    
    /* Allocate VMCS region */
    result = vmx_alloc_vmcs_region(&vcpu->vmx_vmcs_region);
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to allocate VMCS region\n");
        vmx_free_vmxon_region(vcpu->vmx_on_region);
        return result;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_vcpu_setup(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmx_on_region || !vcpu->vmx_vmcs_region) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    hypervisor_t *hv = hypervisor_get_instance();
    
    /* Enter VMX operation */
    vmx_on(vcpu);
    
    /* Clear and initialize VMCS */
    vmx_vmcs_clear(vcpu);
    
    /* Setup VMCS controls */
    u32 pin_ctls = 0;
    u32 proc_ctls = 0;
    u32 proc2_ctls = 0;
    u32 exit_ctls = 0;
    u32 entry_ctls = 0;
    
    /* Pin-based controls - enable external interrupt VM exit */
    pin_ctls = VMX_PIN_BASED_CTLS_EXT_INT_EXIT | 
               VMX_PIN_BASED_CTLS_NMI_EXIT;
    
    if (hv->vmx_cap_pinbased & VMX_PIN_BASED_CTLS_VIRTUAL_NMI) {
        pin_ctls |= VMX_PIN_BASED_CTLS_VIRTUAL_NMI;
    }
    
    /* Primary processor controls */
    proc_ctls = VMX_PROC_BASED_CTLS_CR3_STORE |
                VMX_PROC_BASED_CTLS_CR3_LOAD |
                VMX_PROC_BASED_CTLS_TPR_SHADOW |
                VMX_PROC_BASED_CTLS_IO_BITMAP |
                VMX_PROC_BASED_CTLS_MSR_BITMAP |
                VMX_PROC_BASED_CTLS_PAUSE |
                VMX_PROC_BASED_CTLS_SECONDARY;
    
    /* Secondary processor controls */
    if (hv->has_ept) {
        proc2_ctls |= VMX_PROC_BASED_CTLS2_EPT;
    }
    if (hv->has_vpid) {
        proc2_ctls |= VMX_PROC_BASED_CTLS2_VPID;
    }
    proc2_ctls |= VMX_PROC_BASED_CTLS2_UNRESTRICTED_GUEST;
    
    /* VM-exit controls */
    exit_ctls = VMX_EXIT_CTLS_SAVE_DEBUG |
                VMX_EXIT_CTLS_ACK_INTERRUPT |
                VMX_EXIT_CTLS_HOST_ADDRESS_SPACE_SIZE;
    
    if (hv->vmx_cap_exit & (1ULL << 18)) {
        exit_ctls |= VMX_EXIT_CTLS_SAVE_PAT;
    }
    if (hv->vmx_cap_exit & (1ULL << 19)) {
        exit_ctls |= VMX_EXIT_CTLS_LOAD_PAT;
    }
    if (hv->vmx_cap_exit & (1ULL << 20)) {
        exit_ctls |= VMX_EXIT_CTLS_SAVE_EFER;
    }
    if (hv->vmx_cap_exit & (1ULL << 21)) {
        exit_ctls |= VMX_EXIT_CTLS_LOAD_EFER;
    }
    
    /* VM-entry controls */
    entry_ctls = VMX_ENTRY_CTLS_LOAD_DEBUG |
                 VMX_ENTRY_CTLS_LOAD_PAT |
                 VMX_ENTRY_CTLS_LOAD_EFER;
    
    /* Write control fields */
    vmx_vmwrite(VMCS_PIN_BASED_VM_EXECUTION_CONTROLS, pin_ctls);
    vmx_vmwrite(VMCS_PRIMARY_PROC_BASED_VM_EXEC_CONTROLS, proc_ctls);
    vmx_vmwrite(VMCS_SECONDARY_PROC_BASED_VM_EXEC_CONTROLS, proc2_ctls);
    vmx_vmwrite(VMCS_VM_EXIT_CONTROLS, exit_ctls);
    vmx_vmwrite(VMCS_VM_ENTRY_CONTROLS, entry_ctls);
    
    /* Setup MSR bitmap */
    vmx_vmwrite(VMCS_MSR_BITMAP_ADDRESS, (u64)vcpu->msr_bitmap);
    
    /* Setup I/O bitmaps */
    vmx_vmwrite(VMCS_IO_BITMAP_A_ADDRESS, (u64)vcpu->io_bitmap_a);
    vmx_vmwrite(VMCS_IO_BITMAP_B_ADDRESS, (u64)vcpu->io_bitmap_b);
    
    /* Setup EPTP if enabled */
    if (hv->has_ept && vcpu->vm->ept_pointer) {
        vmx_set_eptp(vcpu, vcpu->vm->ept_pointer);
    }
    
    /* Setup host state */
    u64 cr0, cr3, cr4;
    __asm__ volatile (
        "mov %%cr0, %%rax\n\t"
        "mov %%cr3, %%rbx\n\t"
        "mov %%cr4, %%rcx\n\t"
        "mov %%rax, %0\n\t"
        "mov %%rbx, %1\n\t"
        "mov %%rcx, %2\n\t"
        : "=r" (cr0), "=r" (cr3), "=r" (cr4)
    );
    
    vmx_vmwrite(VMCS_HOST_CR0, cr0);
    vmx_vmwrite(VMCS_HOST_CR3, cr3);
    vmx_vmwrite(VMCS_HOST_CR4, cr4);
    
    /* Get segment bases and limits */
    struct {
        u64 gs_base, fs_base, ds_base, es_base, ss_base, cs_base;
        u16 ds_limit, es_limit, ss_limit, cs_limit;
    } segs;
    
    __asm__ volatile (
        "mov %%gs, %%ax\n\t"
        "lar %%eax, %%ebx\n\t"
        "mov %%fs:0, %0\n\t"
        "mov %%gs:0, %1\n\t"
        "mov %%ds:0, %2\n\t"
        "mov %%es:0, %3\n\t"
        "mov %%ss:0, %4\n\t"
        "mov %%cs:0, %5\n\t"
        "mov %%ax, %6\n\t"
        : "=r" (segs.fs_base), "=r" (segs.gs_base),
          "=r" (segs.ds_base), "=r" (segs.es_base),
          "=r" (segs.ss_base), "=r" (segs.cs_base),
          "=r" (segs.ds_limit)
        :
        : "rax", "rbx", "ecx", "edx"
    );
    
    vmx_vmwrite(VMCS_HOST_FS_BASE, segs.fs_base);
    vmx_vmwrite(VMCS_HOST_GS_BASE, segs.gs_base);
    
    /* Get GDTR and IDTR */
    struct {
        u64 gdtr_base, idtr_base;
        u16 gdtr_limit, idtr_limit;
    } tables;
    
    __asm__ volatile (
        "sgdt %0\n\t"
        "sidt %1\n\t"
        : "=m" (tables.gdtr_limit), "=m" (tables.idtr_limit)
    );
    
    vmx_vmwrite(VMCS_HOST_GDTR_BASE, tables.gdtr_base);
    vmx_vmwrite(VMCS_HOST_IDTR_BASE, tables.idtr_base);
    
    /* Get TR base */
    u64 tr_base;
    __asm__ volatile (
        "str %0\n\t"
        : "=r" (tr_base)
    );
    vmx_vmwrite(VMCS_HOST_TR_BASE, tr_base);
    
    /* Setup guest state */
    vmx_vmwrite(VMCS_GUEST_CR0, vcpu->cpu_state.cr.cr0);
    vmx_vmwrite(VMCS_GUEST_CR3, vcpu->cpu_state.cr.cr3);
    vmx_vmwrite(VMCS_GUEST_CR4, vcpu->cpu_state.cr.cr4);
    
    vmx_vmwrite(VMCS_GUEST_RSP, vcpu->cpu_state.gprs.rsp);
    vmx_vmwrite(VMCS_GUEST_RIP, vcpu->cpu_state.gprs.rip);
    vmx_vmwrite(VMCS_GUEST_RFLAGS, vcpu->cpu_state.gprs.rflags);
    
    /* Setup guest segment registers */
    vmx_vmwrite(VMCS_GUEST_ES_SELECTOR, vcpu->cpu_state.es.selector);
    vmx_vmwrite(VMCS_GUEST_CS_SELECTOR, vcpu->cpu_state.cs.selector);
    vmx_vmwrite(VMCS_GUEST_SS_SELECTOR, vcpu->cpu_state.ss.selector);
    vmx_vmwrite(VMCS_GUEST_DS_SELECTOR, vcpu->cpu_state.ds.selector);
    vmx_vmwrite(VMCS_GUEST_FS_SELECTOR, vcpu->cpu_state.fs.selector);
    vmx_vmwrite(VMCS_GUEST_GS_SELECTOR, vcpu->cpu_state.gs.selector);
    
    vmx_vmwrite(VMCS_GUEST_ES_BASE, vcpu->cpu_state.es.base);
    vmx_vmwrite(VMCS_GUEST_CS_BASE, vcpu->cpu_state.cs.base);
    vmx_vmwrite(VMCS_GUEST_SS_BASE, vcpu->cpu_state.ss.base);
    vmx_vmwrite(VMCS_GUEST_DS_BASE, vcpu->cpu_state.ds.base);
    vmx_vmwrite(VMCS_GUEST_FS_BASE, vcpu->cpu_state.fs.base);
    vmx_vmwrite(VMCS_GUEST_GS_BASE, vcpu->cpu_state.gs.base);
    
    /* Setup activity state */
    vmx_vmwrite(VMCS_GUEST_ACTIVITY_STATE, 0); /* Active */
    
    /* Load VMCS */
    vmx_vmcs_clear(vcpu);
    
    return NEXUSVM_OK;
}

void vmx_set_eptp(nexusvcpu_t *vcpu, eptp_t *eptp)
{
    if (!vcpu || !eptp) return;
    
    u64 eptp_value = (eptp->memory_type << 3) |
                     ((eptp->page_walk_length - 1) << 6) |
                     (eptp->enable_accessed_dirty << 6) |
                     (eptp->pml4_physical_address & 0xFFFFFFFFFF);
    
    vmx_vmwrite(VMCS_EPT_POINTER, eptp_value);
}

nexusvm_result_t vmx_vcpu_run(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmx_vmcs_region) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Load VMCS */
    vmx_vmcs_clear(vcpu);
    
    /* Main VM run loop */
    while (vcpu->running) {
        vcpu->vmentry_count++;
        
        /* Launch or resume VM */
        if (vcpu->vcpu_id == 0) {
            __asm__ volatile (
                "vmlaunch\n\t"
                "jz .Lvm_success\n\t"
                "mov $1, %%rax\n\t"
                "jmp .Lvm_done\n\t"
                ".Lvm_success:\n\t"
                "mov $0, %%rax\n\t"
                ".Lvm_done:\n\t"
                : 
                :
                : "rax", "cc", "memory"
            );
        } else {
            __asm__ volatile (
                "vmresume\n\t"
                "jz .Lvm_success\n\t"
                "mov $1, %%rax\n\t"
                "jmp .Lvm_done\n\t"
                ".Lvm_success:\n\t"
                "mov $0, %%rax\n\t"
                ".Lvm_done:\n\t"
                :
                :
                : "rax", "cc", "memory"
            );
        }
        
        /* Handle VM exit */
        vcpu->vmexit_count++;
        
        u64 exit_reason = vmx_vmread(VMCS_VM_EXIT_REASON);
        u64 exit_qual = vmx_vmread(VMCS_VM_EXIT_QUALIFICATION);
        
        /* Handle different exit types */
        nexusvm_result_t result = vmx_handle_vmexit(vcpu, exit_reason, exit_qual);
        if (result != NEXUSVM_OK) {
            return result;
        }
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_vmexit(nexusvcpu_t *vcpu, u64 exit_reason, u64 exit_qual)
{
    u16 exit_code = exit_reason & 0xFFFF;
    
    switch (exit_code) {
        case VMX_EXIT_EXT_INTERRUPT:
        case VMX_EXIT_EXCEPTION_OR_NMI:
            return vmx_handle_interrupt(vcpu);
            
        case VMX_EXIT_CPUID:
            return vmx_handle_cpuid(vcpu);
            
        case VMX_EXIT_HLT:
            return vmx_handle_hlt(vcpu);
            
        case VMX_EXIT_CR_ACCESS:
            return vmx_handle_cr_access(vcpu, exit_qual);
            
        case VMX_EXIT_MSR_READ:
            return vmx_handle_msr_read(vcpu);
            
        case VMX_EXIT_MSR_WRITE:
            return vmx_handle_msr_write(vcpu);
            
        case VMX_EXIT_IO_INSTRUCTION:
            return vmx_handle_io(vcpu, exit_qual);
            
        case VMX_EXIT_EPT_VIOLATION:
            return vmx_handle_ept_violation(vcpu, exit_qual);
            
        case VMX_EXIT_VMCALL:
            return vmx_handle_vmcall(vcpu);
            
        case VMX_EXIT_VMXOFF:
            vcpu->running = false;
            return NEXUSVM_OK;
            
        case VMX_EXIT_RDTSC:
        case VMX_EXIT_RDTSCP:
            return vmx_handle_rdtsc(vcpu);
            
        case VMX_EXIT_INVLPG:
            return vmx_handle_invlpg(vcpu, exit_qual);
            
        case VMX_EXIT_XSETBV:
            return vmx_handle_xsetbv(vcpu);
            
        case VMX_EXIT_MCE_DURING_ENTRY:
        case VMX_EXIT_MCE_DURING_EXIT:
            return vmx_handle_machine_check(vcpu);
            
        case VMX_EXIT_TASK_SWITCH:
            return vmx_handle_task_switch(vcpu);
            
        default:
            NEXUSVM_LOG_WARN("Unhandled VM exit: 0x%x\n", exit_code);
            return NEXUSVM_ERR_GENERIC;
    }
}

void vmx_vcpu_cleanup(nexusvcpu_t *vcpu)
{
    if (!vcpu) return;
    
    if (vcpu->vmx_vmcs_region) {
        vmx_free_vmcs_region(vcpu->vmx_vmcs_region);
        vcpu->vmx_vmcs_region = NULL;
    }
    
    if (vcpu->vmx_on_region) {
        vmx_free_vmxon_region(vcpu->vmx_on_region);
        vcpu->vmx_on_region = NULL;
    }
}

/*============================================================================
 * VM Exit Handlers
 *============================================================================*/

nexusvm_result_t vmx_handle_interrupt(nexusvcpu_t *vcpu)
{
    /* Read interrupt information */
    u64 int_info = vmx_vmread(VMCS_VM_EXIT_INTERRUPTION_INFO);
    
    u32 vector = int_info & 0xFF;
    u32 type = (int_info >> 8) & 0x7;
    
    NEXUSVM_LOG_DEBUG("Interrupt: vector=%u, type=%u\n", vector, type);
    
    /* Acknowledge interrupt if needed */
    /* ... interrupt acknowledgment code ... */
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_cpuid(nexusvcpu_t *vcpu)
{
    u32 eax = vcpu->cpu_state.gprs.rax & 0xFFFFFFFF;
    u32 ebx = 0, ecx = 0, edx = 0;
    
    __asm__ volatile (
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (eax), "c" (ecx)
    );
    
    vcpu->cpu_state.gprs.rax = eax;
    vcpu->cpu_state.gprs.rbx = ebx;
    vcpu->cpu_state.gprs.rcx = ecx;
    vcpu->cpu_state.gprs.rdx = edx;
    
    /* Update guest RIP */
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_hlt(nexusvcpu_t *vcpu)
{
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    /* In a real hypervisor, we would sleep here */
    /* For now, just return to guest */
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_cr_access(nexusvcpu_t *vcpu, u64 qual)
{
    u32 cr_num = qual & 0xF;
    u32 access_type = (qual >> 4) & 0x3;
    u32 mov_type = (qual >> 6) & 1;
    u32 guest_reg = (qual >> 8) & 0xF;
    
    /* Handle CR access emulation */
    /* ... */
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_msr_read(nexusvcpu_t *vcpu)
{
    u64 msr = vcpu->cpu_state.gprs.rcx;
    u64 value = 0;
    
    /* Read MSR */
    __asm__ volatile (
        "rdmsr\n\t"
        : "=a" (value), "=d" (msr)
        : "c" (msr)
    );
    
    vcpu->cpu_state.gprs.rax = value & 0xFFFFFFFF;
    vcpu->cpu_state.gprs.rdx = (value >> 32) & 0xFFFFFFFF;
    
    /* Advance RIP */
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_msr_write(nexusvcpu_t *vcpu)
{
    u64 msr = vcpu->cpu_state.gprs.rcx;
    u64 value = vcpu->cpu_state.gprs.rax | 
                (vcpu->cpu_state.gprs.rdx << 32);
    
    /* Write MSR */
    __asm__ volatile (
        "wrmsr\n\t"
        :
        : "a" (value & 0xFFFFFFFF), "d" ((value >> 32) & 0xFFFFFFFF), "c" (msr)
    );
    
    /* Advance RIP */
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_io(nexusvcpu_t *vcpu, u64 qual)
{
    u16 port = qual & 0xFFFF;
    u32 size = (qual >> 16) & 0x7;
    u32 direction = (qual >> 24) & 1;
    
    /* Handle I/O port access */
    if (direction) {
        /* OUT instruction - write to port */
        /* ... */
    } else {
        /* IN instruction - read from port */
        /* ... */
    }
    
    /* Advance RIP */
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_ept_violation(nexusvcpu_t *vcpu, u64 qual)
{
    u64 guest_phys = vmx_vmread(VMCS_GUEST_LINEAR_ADDRESS);
    
    /* Check if it's a read, write, or execute violation */
    bool read = qual & 1;
    bool write = (qual >> 1) & 1;
    bool execute = (qual >> 2) & 1;
    bool ept_misconfig = (qual >> 3) & 1;
    bool valid_guest = (qual >> 7) & 1;
    
    if (ept_misconfig) {
        NEXUSVM_LOG_ERROR("EPT misconfiguration at GPA 0x%lx\n", guest_phys);
        return NEXUSVM_ERR_GENERIC;
    }
    
    if (!valid_guest) {
        NEXUSVM_LOG_ERROR("EPT violation with invalid guest address\n");
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Try to handle the EPT violation (page not present, etc.) */
    /* This would involve shadow page table handling in a real hypervisor */
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_vmcall(nexusvcpu_t *vcpu)
{
    hypercall_params_t params;
    hypercall_result_t result;
    
    params.rax = vcpu->cpu_state.gprs.rax;
    params.rbx = vcpu->cpu_state.gprs.rbx;
    params.rcx = vcpu->cpu_state.gprs.rcx;
    params.rdx = vcpu->cpu_state.gprs.rdx;
    params.rsi = vcpu->cpu_state.gprs.rsi;
    params.rdi = vcpu->cpu_state.gprs.rdi;
    params.r8 = vcpu->cpu_state.gprs.r8;
    params.r9 = vcpu->cpu_state.gprs.r9;
    params.r10 = vcpu->cpu_state.gprs.r10;
    params.r11 = vcpu->cpu_state.gprs.r11;
    params.r12 = vcpu->cpu_state.gprs.r12;
    
    nexusvm_result_t status = hypercall_handle(vcpu, &params, &result);
    
    vcpu->cpu_state.gprs.rax = result.ret0;
    vcpu->cpu_state.gprs.rbx = result.ret1;
    vcpu->cpu_state.gprs.rcx = result.ret2;
    vcpu->cpu_state.gprs.rdx = result.ret3;
    
    /* Advance RIP */
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    return status;
}

nexusvm_result_t vmx_handle_rdtsc(nexusvcpu_t *vcpu)
{
    u64 tsc;
    __asm__ volatile (
        "rdtsc\n\t"
        "shl $32, %%rdx\n\t"
        "or %%rax, %%rdx\n\t"
        "mov %%rdx, %0\n\t"
        : "=r" (tsc)
        :
        : "rax", "rdx"
    );
    
    vcpu->cpu_state.gprs.rax = tsc & 0xFFFFFFFF;
    vcpu->cpu_state.gprs.rdx = (tsc >> 32) & 0xFFFFFFFF;
    
    /* Advance RIP */
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_invlpg(nexusvcpu_t *vcpu, u64 qual)
{
    /* Invalidate TLB entry */
    u64 addr = qual;
    __asm__ volatile (
        "invlpg %0\n\t"
        :
        : "m" (*(char *)addr)
    );
    
    /* Advance RIP */
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_xsetbv(nexusvcpu_t *vcpu)
{
    u32 ecx = vcpu->cpu_state.gprs.rcx & 0xFFFFFFFF;
    u64 value = vcpu->cpu_state.gprs.rax | 
                ((u64)vcpu->cpu_state.gprs.rdx << 32);
    
    if (ecx == 0) {
        __asm__ volatile (
            "xsetbv\n\t"
            :
            : "a" (value & 0xFFFFFFFF), 
              "d" ((value >> 32) & 0xFFFFFFFF),
              "c" (ecx)
            : "cc", "memory"
        );
    }
    
    /* Advance RIP */
    u64 rip = vmx_vmread(VMCS_GUEST_RIP);
    u64 len = vmx_vmread(VMCS_VM_EXIT_INSTRUCTION_LENGTH);
    vmx_vmwrite(VMCS_GUEST_RIP, rip + len);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_handle_machine_check(nexusvcpu_t *vcpu)
{
    NEXUSVM_LOG_ERROR("Machine check exception\n");
    vcpu->running = false;
    return NEXUSVM_ERR_GENERIC;
}

nexusvm_result_t vmx_handle_task_switch(nexusvcpu_t *vcpu)
{
    NEXUSVM_LOG_WARN("Task switch not supported\n");
    return NEXUSVM_ERR_NOT_SUPPORTED;
}

/*============================================================================
 * MSR Bitmap Management
 *============================================================================*/

void vmx_enable_msr_read_intercept(nexusvcpu_t *vcpu, u32 msr)
{
    if (!vcpu || !vcpu->msr_bitmap) return;
    
    u32 index = msr / 8;
    u32 bit = msr % 8;
    
    if (index < 8192) {
        u8 *bitmap = (u8 *)vcpu->msr_bitmap;
        bitmap[index] |= (1 << bit);
    }
}

void vmx_disable_msr_read_intercept(nexusvcpu_t *vcpu, u32 msr)
{
    if (!vcpu || !vcpu->msr_bitmap) return;
    
    u32 index = msr / 8;
    u32 bit = msr % 8;
    
    if (index < 8192) {
        u8 *bitmap = (u8 *)vcpu->msr_bitmap;
        bitmap[index] &= ~(1 << bit);
    }
}

void vmx_enable_msr_write_intercept(nexusvcpu_t *vcpu, u32 msr)
{
    if (!vcpu || !vcpu->msr_bitmap) return;
    
    u32 index = msr / 8 + 2048; /* Second 4KB for writes */
    u32 bit = msr % 8;
    
    if (index < 8192) {
        u8 *bitmap = (u8 *)vcpu->msr_bitmap;
        bitmap[index] |= (1 << bit);
    }
}

void vmx_disable_msr_write_intercept(nexusvcpu_t *vcpu, u32 msr)
{
    if (!vcpu || !vcpu->msr_bitmap) return;
    
    u32 index = msr / 8 + 2048;
    u32 bit = msr % 8;
    
    if (index < 8192) {
        u8 *bitmap = (u8 *)vcpu->msr_bitmap;
        bitmap[index] &= ~(1 << bit);
    }
}

/*============================================================================
 * I/O Bitmap Management
 *============================================================================*/

void vmx_enable_io_port_intercept(nexusvcpu_t *vcpu, u16 port)
{
    if (!vcpu) return;
    
    u32 index = port / 8;
    u32 bit = port % 8;
    
    if (index < 4096) {
        u8 *bitmap_a = (u8 *)vcpu->io_bitmap_a;
        u8 *bitmap_b = (u8 *)vcpu->io_bitmap_b;
        bitmap_a[index] |= (1 << bit);
        bitmap_b[index] |= (1 << bit);
    }
}

void vmx_disable_io_port_intercept(nexusvcpu_t *vcpu, u16 port)
{
    if (!vcpu) return;
    
    u32 index = port / 8;
    u32 bit = port % 8;
    
    if (index < 4096) {
        u8 *bitmap_a = (u8 *)vcpu->io_bitmap_a;
        u8 *bitmap_b = (u8 *)vcpu->io_bitmap_b;
        bitmap_a[index] &= ~(1 << bit);
        bitmap_b[index] &= ~(1 << bit);
    }
}

nexusvm_result_t vmx_vmentry(nexusvcpu_t *vcpu)
{
    if (!vcpu) return NEXUSVM_ERR_INVALID_PARAM;
    
    __asm__ volatile (
        "vmlaunch\n\t"
        : 
        : 
        : "cc", "memory"
    );
    
    return NEXUSVM_OK;
}

nexusvm_result_t vmx_vmexit(nexusvcpu_t *vcpu)
{
    return vmx_vcpu_run(vcpu);
}

#endif /* __x86_64__ */
