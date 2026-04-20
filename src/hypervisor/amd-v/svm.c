/**
 * NexusVM AMD-V (SVM) Implementation
 */

#include <hypervisor/amd-v/svm.h>
#include <hypervisor/hypervisor.h>
#include <hypervisor/mmu/npt.h>
#include <utils/log.h>
#include <utils/assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef __amd64__

/* SVM feature detection */
static bool svm_detected = false;
static bool svm_enabled = false;

/* SVM MSRs */
#define MSR_VM_CR              0xC0010114
#define MSR_VM_HSAVE_PA        0xC0010117
#define MSR_SVM_FLAGS          0xC0010118
#define MSR_SVM_LOCK           0xC0010119
#define MSR_SVM_VGIF           0xC001011A

/* VMCB offsets */
#define VMCB_STATE_SAVE_AREA   0x00
#define VMCB_CONTROL_AREA      0x400

/*============================================================================
 * SVM Assembly Routines
 *============================================================================*/

extern void svm_vmload_asm(void *vmcb);
extern void svm_vmsave_asm(void *vmcb);
extern void svm_stgi_asm(void);
extern void svm_clgi_asm(void);
extern void svm_vmmcall_asm(void);
extern void svm_vmrun_asm(void *vmcb);

/*============================================================================
 * SVM Feature Detection
 *============================================================================*/

void detect_svm_features(hypervisor_t *hv)
{
    /* Check CPUID for SVM support */
    u32 eax, ebx, ecx, edx;
    
    __asm__ volatile (
        "mov $0x80000001, %%eax\n\t"
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        :
        : "memory"
    );
    
    hv->has_svm = (edx >> 2) & 1;
    
    if (!hv->has_svm) {
        NEXUSVM_LOG_WARN("SVM (AMD-V) not supported on this CPU\n");
        return;
    }
    
    /* Get additional SVM features */
    __asm__ volatile (
        "mov $0x8000000A, %%eax\n\t"
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        :
        : "memory"
    );
    
    /* EBX contains SVM revision and features */
    u32 svm_rev = ebx & 0xFF;
    
    NEXUSVM_LOG_INFO("SVM Revision: %u\n", svm_rev);
    NEXUSVM_LOG_INFO("SVM features detected\n");
    
    /* Check for NPT */
    hv->has_npt = (edx >> 0) & 1;
    
    /* Check for other SVM features */
    bool has_lbr_virt = (edx >> 1) & 1;
    bool has_svm_lock = (edx >> 2) & 1;
    bool has_nrips = (edx >> 3) & 1;
    bool has_tsc_rate_msr = (edx >> 4) & 1;
    bool has_vmcb_clean = (edx >> 5) & 1;
    bool has_decode_assist = (edx >> 6) & 1;
    bool has_msr_proto_bit = (edx >> 7) & 1;
    bool has_pause_filter = (edx >> 8) & 1;
    bool has_pause_filter_thresh = (edx >> 9) & 1;
    bool has_avic = (edx >> 10) & 1;
    bool has_vgif = (edx >> 16) & 1;
    
    NEXUSVM_LOG_INFO("NPT (AMD-V): %s\n", hv->has_npt ? "supported" : "not supported");
    NEXUSVM_LOG_INFO("AVIC: %s\n", has_avic ? "supported" : "not supported");
    NEXUSVM_LOG_INFO("VGIF: %s\n", has_vgif ? "supported" : "not supported");
    
    svm_detected = true;
}

/*============================================================================
 * SVM MSR Access
 *============================================================================*/

static u64 svm_read_msr(u32 msr)
{
    u32 edx, eax;
    
    __asm__ volatile (
        "rdmsr\n\t"
        : "=a" (eax), "=d" (edx)
        : "c" (msr)
    );
    
    return ((u64)edx << 32) | eax;
}

static void svm_write_msr(u32 msr, u64 value)
{
    __asm__ volatile (
        "wrmsr\n\t"
        :
        : "a" (value & 0xFFFFFFFF), 
          "d" ((value >> 32) & 0xFFFFFFFF),
          "c" (msr)
    );
}

/*============================================================================
 * SVM Enable/Disable
 *============================================================================*/

nexusvm_result_t svm_enable(void)
{
    if (!svm_detected) {
        return NEXUSVM_ERR_CPU_NOT_SUPPORTED;
    }
    
    /* Read EFER */
    u64 efer = svm_read_msr(0xC0000080);
    
    /* Set SVM enable bit */
    efer |= (1 << 12);
    
    /* Write EFER */
    svm_write_msr(0xC0000080, efer);
    
    svm_enabled = true;
    NEXUSVM_LOG_INFO("SVM enabled\n");
    
    return NEXUSVM_OK;
}

void svm_disable(void)
{
    u64 efer = svm_read_msr(0xC0000080);
    efer &= ~(1 << 12);
    svm_write_msr(0xC0000080, efer);
    
    svm_enabled = false;
}

bool svm_is_enabled(void)
{
    u64 efer = svm_read_msr(0xC0000080);
    return (efer >> 12) & 1;
}

bool svm_is_supported(void)
{
    u32 edx;
    __asm__ volatile (
        "mov $0x80000001, %%eax\n\t"
        "cpuid\n\t"
        : "=d" (edx)
        :
        : "eax", "ebx", "ecx"
    );
    
    return (edx >> 2) & 1;
}

/*============================================================================
 * VMCB Management
 *============================================================================*/

nexusvm_result_t svm_alloc_vmcb(void **vmcb_out)
{
    /* VMCB must be aligned to 4096 */
    void *vmcb = aligned_alloc(4096, 4096);
    if (!vmcb) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    memset(vmcb, 0, 4096);
    
    *vmcb_out = vmcb;
    return NEXUSVM_OK;
}

void svm_free_vmcb(void *vmcb)
{
    if (vmcb) {
        free(vmcb);
    }
}

nexusvm_result_t svm_alloc_hsave(void **region_out)
{
    /* HSAVE area must be 4096 bytes, aligned */
    void *region = aligned_alloc(4096, 4096);
    if (!region) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    memset(region, 0, 4096);
    
    *region_out = region;
    return NEXUSVM_OK;
}

void svm_free_hsave(void *region)
{
    if (region) {
        free(region);
    }
}

/*============================================================================
 * VMCB Operations
 *============================================================================*/

nexusvm_result_t svm_vmrun(void *vmcb)
{
    if (!vmcb) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    __asm__ volatile (
        "mov %0, %%rax\n\t"
        "vmrun %%rax\n\t"
        :
        : "r" (vmcb)
        : "rax", "cc", "memory"
    );
    
    return NEXUSVM_OK;
}

void svm_vmload(void *vmcb)
{
    __asm__ volatile (
        "mov %0, %%rax\n\t"
        "vmload %%rax\n\t"
        :
        : "r" (vmcb)
        : "rax", "cc", "memory"
    );
}

void svm_vmsave(void *vmcb)
{
    __asm__ volatile (
        "mov %0, %%rax\n\t"
        "vmsave %%rax\n\t"
        :
        : "r" (vmcb)
        : "rax", "cc", "memory"
    );
}

void svm_clgi(void)
{
    __asm__ volatile (
        "clgi\n\t"
        :
        :
        : "cc", "memory"
    );
}

void svm_stgi(void)
{
    __asm__ volatile (
        "stgi\n\t"
        :
        :
        : "cc", "memory"
    );
}

void svm_vmmcall(void)
{
    __asm__ volatile (
        "vmmcall\n\t"
        :
        :
        : "cc", "memory"
    );
}

/*============================================================================
 * Intercept Control
 *============================================================================*/

void svm_set_intercept(void *vmcb, u32 offset, u32 bit, bool enable)
{
    if (!vmcb || offset >= 0x400) return;
    
    u8 *control = (u8 *)vmcb + VMCB_CONTROL_AREA;
    
    if (enable) {
        control[offset] |= (1 << bit);
    } else {
        control[offset] &= ~(1 << bit);
    }
}

void svm_set_io_bitmap(void *vmcb, phys_addr_t bitmap_pa)
{
    if (!vmcb) return;
    
    u64 *vmcb64 = (u64 *)((u8 *)vmcb + VMCB_OFFSET_IOIO_MAP);
    *vmcb64 = bitmap_pa;
}

void svm_set_msr_bitmap(void *vmcb, phys_addr_t bitmap_pa)
{
    if (!vmcb) return;
    
    u64 *vmcb64 = (u64 *)((u8 *)vmcb + VMCB_OFFSET_MSR_MAP);
    *vmcb64 = bitmap_pa;
}

void svm_set_npt(void *vmcb, u64 ncr3)
{
    if (!vmcb) return;
    
    u64 *vmcb64 = (u64 *)((u8 *)vmcb + VMCB_OFFSET_NESTED_PAGING_ROOT);
    *vmcb64 = ncr3;
}

/*============================================================================
 * vCPU SVM Operations
 *============================================================================*/

nexusvm_result_t svm_vcpu_init(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Allocate VMCB */
    nexusvm_result_t result = svm_alloc_vmcb(&vcpu->vmcb);
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to allocate VMCB\n");
        return result;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t svm_vcpu_setup(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmcb) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    void *vmcb = vcpu->vmcb;
    hypervisor_t *hv = hypervisor_get_instance();
    
    /* Setup intercepts */
    u8 *intercept = (u8 *)vmcb + VMCB_OFFSET_INTERCEPT;
    
    /* Enable CPUID intercept */
    intercept[0x78 / 8] |= (1 << (0x78 % 8));  /* CPUID */
    
    /* Enable MSR read/write intercepts */
    intercept[0x7C / 8] |= (1 << (0x7C % 8));   /* RDMSR */
    intercept[0x7D / 8] |= (1 << (0x7D % 8));   /* WRMSR */
    
    /* Enable CR intercepts */
    intercept[0x00 / 8] |= (1 << (0x00 % 8));    /* CR0 read */
    intercept[0x02 / 8] |= (1 << (0x02 % 8));    /* CR2 read */
    intercept[0x03 / 8] |= (1 << (0x03 % 8));    /* CR3 read */
    intercept[0x04 / 8] |= (1 << (0x04 % 8));    /* CR4 read */
    
    intercept[0x10 / 8] |= (1 << (0x10 % 8));    /* CR0 write */
    intercept[0x12 / 8] |= (1 << (0x12 % 8));    /* CR2 write */
    intercept[0x13 / 8] |= (1 << (0x13 % 8));    /* CR3 write */
    intercept[0x14 / 8] |= (1 << (0x14 % 8));    /* CR4 write */
    
    /* Enable INTR intercept */
    intercept[0x60 / 8] |= (1 << (0x60 % 8));    /* INTR */
    intercept[0x61 / 8] |= (1 << (0x61 % 8));    /* NMI */
    
    /* Enable misc intercepts */
    intercept[0x76 / 8] |= (1 << (0x76 % 8));    /* INVLPG */
    intercept[0x7A / 8] |= (1 << (0x7A % 8));    /* HLT */
    intercept[0x7B / 8] |= (1 << (0x7B % 8));    /* INVD */
    intercept[0x7F / 8] |= (1 << (0x7F % 8));    /* RDTSC */
    
    /* Enable VMRUN/VMMCALL/VMEXIT intercepts */
    intercept[0xC0 / 8] |= (1 << (0xC0 % 8));    /* VMRUN */
    intercept[0xC1 / 8] |= (1 << (0xC1 % 8));    /* VMMCALL */
    intercept[0xC2 / 8] |= (1 << (0xC2 % 8));    /* VMLOAD */
    intercept[0xC3 / 8] |= (1 << (0xC3 % 8));    /* VMSAVE */
    intercept[0xC4 / 8] |= (1 << (0xC4 % 8));    /* STGI */
    intercept[0xC5 / 8] |= (1 << (0xC5 % 8));    /* CLGI */
    
    /* Setup MSR bitmap */
    if (vcpu->msr_bitmap) {
        svm_set_msr_bitmap(vmcb, (phys_addr_t)vcpu->msr_bitmap);
    }
    
    /* Setup NPT if supported */
    if (hv->has_npt && vcpu->vm->ept_root) {
        /* NPT uses similar structure to EPT */
        svm_set_npt(vmcb, (u64)vcpu->vm->ept_root);
    }
    
    /* Setup guest state save area */
    u64 *state = (u64 *)vmcb;
    
    /* General purpose registers */
    state[0x00 / 8] = vcpu->cpu_state.gprs.rax;
    state[0x08 / 8] = vcpu->cpu_state.gprs.rbx;
    state[0x10 / 8] = vcpu->cpu_state.gprs.rcx;
    state[0x18 / 8] = vcpu->cpu_state.gprs.rdx;
    state[0x20 / 8] = vcpu->cpu_state.gprs.rsi;
    state[0x28 / 8] = vcpu->cpu_state.gprs.rdi;
    state[0x30 / 8] = vcpu->cpu_state.gprs.rbp;
    state[0x38 / 8] = vcpu->cpu_state.gprs.rsp;
    
    /* RIP and RFLAGS */
    state[0x78 / 8] = vcpu->cpu_state.gprs.rip;
    state[0x80 / 8] = vcpu->cpu_state.gprs.rflags;
    
    /* Control registers */
    state[0x88 / 8] = vcpu->cpu_state.cr.cr0;
    state[0x90 / 8] = vcpu->cpu_state.cr.cr2;
    state[0x98 / 8] = vcpu->cpu_state.cr.cr3;
    state[0xA0 / 8] = vcpu->cpu_state.cr.cr4;
    
    /* Segment registers */
    state[0xA8 / 8] = vcpu->cpu_state.cs.selector;
    state[0xB0 / 8] = vcpu->cpu_state.ss.selector;
    state[0xB8 / 8] = vcpu->cpu_state.ds.selector;
    state[0xC0 / 8] = vcpu->cpu_state.es.selector;
    state[0xC8 / 8] = vcpu->cpu_state.fs.selector;
    state[0xD0 / 8] = vcpu->cpu_state.gs.selector;
    
    /* Segment base addresses */
    state[0xD8 / 8] = vcpu->cpu_state.cs.base;
    state[0xE0 / 8] = vcpu->cpu_state.ss.base;
    state[0xE8 / 8] = vcpu->cpu_state.ds.base;
    state[0xF0 / 8] = vcpu->cpu_state.es.base;
    state[0xF8 / 8] = vcpu->cpu_state.fs.base;
    state[0x100 / 8] = vcpu->cpu_state.gs.base;
    
    /* GDTR and IDTR */
    state[0x108 / 8] = vcpu->cpu_state.gdtr_limit;
    state[0x110 / 8] = vcpu->cpu_state.gdtr_base;
    state[0x118 / 8] = vcpu->cpu_state.idtr_limit;
    state[0x120 / 8] = vcpu->cpu_state.idtr_base;
    
    /* EFER MSR */
    state[0x1E0 / 8] = vcpu->cpu_state.msrs.ia32_efer;
    
    /* APIC base */
    state[0x1E8 / 8] = 0xFEE00000; /* Default LAPIC base */
    
    return NEXUSVM_OK;
}

nexusvm_result_t svm_vcpu_run(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmcb) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    void *vmcb = vcpu->vmcb;
    
    /* Save host state */
    svm_vmsave(vmcb);
    
    /* Main SVM run loop */
    while (vcpu->running) {
        vcpu->vmentry_count++;
        
        /* Load guest state from VMCB */
        svm_vmload(vmcb);
        
        /* Enable interrupts */
        svm_stgi();
        
        /* Run guest */
        __asm__ volatile (
            "mov %0, %%rax\n\t"
            "vmrun %%rax\n\t"
            :
            : "r" (vmcb)
            : "rax", "cc", "memory"
        );
        
        /* Disable interrupts */
        svm_clgi();
        
        /* Save guest state to VMCB */
        svm_vmsave(vmcb);
        
        vcpu->vmexit_count++;
        
        /* Handle exit */
        u64 exit_code = svm_get_exit_code(vmcb);
        
        nexusvm_result_t result = svm_handle_exit(vmcb);
        if (result != NEXUSVM_OK) {
            return result;
        }
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_exit(void *vmcb)
{
    if (!vmcb) return NEXUSVM_ERR_INVALID_PARAM;
    
    u64 exit_code = svm_get_exit_code(vmcb);
    u64 exit_info1 = svm_get_exit_info1(vmcb);
    u64 exit_info2 = svm_get_exit_info2(vmcb);
    
    switch (exit_code) {
        case SVM_EXIT_CPUID:
            return svm_handle_cpuid(vmcb);
            
        case SVM_EXIT_RDMSR:
            return svm_handle_rdmsr(vmcb);
            
        case SVM_EXIT_WRMSR:
            return svm_handle_wrmsr(vmcb);
            
        case SVM_EXIT_READ_CR0:
        case SVM_EXIT_READ_CR2:
        case SVM_EXIT_READ_CR3:
        case SVM_EXIT_READ_CR4:
            return svm_handle_cr_read(vmcb, exit_code);
            
        case SVM_EXIT_WRITE_CR0:
        case SVM_EXIT_WRITE_CR2:
        case SVM_EXIT_WRITE_CR3:
        case SVM_EXIT_WRITE_CR4:
            return svm_handle_cr_write(vmcb, exit_code);
            
        case SVM_EXIT_INVLPG:
            return svm_handle_invlpg(vmcb, exit_info2);
            
        case SVM_EXIT_HLT:
            return svm_handle_hlt(vmcb);
            
        case SVM_EXIT_VMMCALL:
            return svm_handle_vmmcall(vmcb);
            
        case SVM_EXIT_NPF:
            return svm_handle_npf(vmcb, exit_info1, exit_info2);
            
        case SVM_EXIT_RDTSC:
        case SVM_EXIT_RDTSCP:
            return svm_handle_rdtsc(vmcb);
            
        case SVM_EXIT_INTR:
        case SVM_EXIT_NMI:
            return svm_handle_interrupt(vmcb);
            
        case SVM_EXIT_VMRUN:
            return svm_handle_vmrun(vmcb);
            
        default:
            NEXUSVM_LOG_WARN("Unhandled SVM exit: 0x%lx\n", exit_code);
            return NEXUSVM_ERR_GENERIC;
    }
}

u64 svm_get_exit_code(void *vmcb)
{
    return *(u64 *)((u8 *)vmcb + VMCB_OFFSET_EXITCODE);
}

u64 svm_get_exit_info1(void *vmcb)
{
    return *(u64 *)((u8 *)vmcb + VMCB_OFFSET_EXITINFO1);
}

u64 svm_get_exit_info2(void *vmcb)
{
    return *(u64 *)((u8 *)vmcb + VMCB_OFFSET_EXITINFO2);
}

void svm_vcpu_cleanup(nexusvcpu_t *vcpu)
{
    if (!vcpu) return;
    
    if (vcpu->vmcb) {
        svm_free_vmcb(vcpu->vmcb);
        vcpu->vmcb = NULL;
    }
}

/*============================================================================
 * SVM Exit Handlers
 *============================================================================*/

nexusvm_result_t svm_handle_cpuid(void *vmcb)
{
    u64 *state = (u64 *)vmcb;
    
    u32 eax = state[0x00 / 8] & 0xFFFFFFFF;
    u32 ecx = state[0x18 / 8] & 0xFFFFFFFF;
    u32 ebx, edx;
    
    __asm__ volatile (
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (eax), "c" (ecx)
    );
    
    state[0x00 / 8] = eax;
    state[0x08 / 8] = ebx;
    state[0x10 / 8] = ecx;
    state[0x18 / 8] = edx;
    
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_rdmsr(void *vmcb)
{
    u64 *state = (u64 *)vmcb;
    u32 ecx = state[0x18 / 8] & 0xFFFFFFFF;
    
    u64 value = 0;
    
    __asm__ volatile (
        "rdmsr\n\t"
        : "=a" (value), "=d" (ecx)
        : "c" (ecx)
    );
    
    state[0x00 / 8] = value & 0xFFFFFFFF;
    state[0x10 / 8] = (value >> 32) & 0xFFFFFFFF;
    
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_wrmsr(void *vmcb)
{
    u64 *state = (u64 *)vmcb;
    u32 ecx = state[0x18 / 8] & 0xFFFFFFFF;
    u64 value = state[0x00 / 8] | (state[0x10 / 8] << 32);
    
    __asm__ volatile (
        "wrmsr\n\t"
        :
        : "a" (value & 0xFFFFFFFF), 
          "d" ((value >> 32) & 0xFFFFFFFF),
          "c" (ecx)
    );
    
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_cr_read(void *vmcb, u64 exit_code)
{
    u64 *state = (u64 *)vmcb;
    
    /* CR reads are already handled by hardware */
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_cr_write(void *vmcb, u64 exit_code)
{
    /* CR writes are already handled by hardware */
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_invlpg(void *vmcb, u64 addr)
{
    __asm__ volatile (
        "invlpg %0\n\t"
        :
        : "m" (*(char *)addr)
    );
    
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_hlt(void *vmcb)
{
    /* In a real hypervisor, we'd sleep here */
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_vmmcall(void *vmcb)
{
    u64 *state = (u64 *)vmcb;
    
    hypercall_params_t params;
    hypercall_result_t result;
    
    params.rax = state[0x00 / 8];
    params.rbx = state[0x08 / 8];
    params.rcx = state[0x10 / 8];
    params.rdx = state[0x18 / 8];
    params.rsi = state[0x20 / 8];
    params.rdi = state[0x28 / 8];
    
    /* Get additional registers */
    params.r8 = state[0x30 / 8];
    params.r9 = state[0x38 / 8];
    params.r10 = state[0x40 / 8];
    params.r11 = state[0x48 / 8];
    params.r12 = state[0x50 / 8];
    
    /* Note: Need to get vcpu context for hypercall */
    nexusvm_result_t status = NEXUSVM_ERR_NOT_SUPPORTED; /* hypercall_handle(vcpu, &params, &result); */
    
    state[0x00 / 8] = result.ret0;
    state[0x08 / 8] = result.ret1;
    state[0x10 / 8] = result.ret2;
    state[0x18 / 8] = result.ret3;
    
    return status;
}

nexusvm_result_t svm_handle_npf(void *vmcb, u64 exit_info1, u64 exit_info2)
{
    NEXUSVM_LOG_DEBUG("NPT page fault: gpa=0x%lx, exit_info1=0x%lx\n",
                      exit_info2, exit_info1);
    
    /* Handle NPT violation - similar to EPT violation */
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_rdtsc(void *vmcb)
{
    u64 *state = (u64 *)vmcb;
    
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
    
    state[0x00 / 8] = tsc & 0xFFFFFFFF;
    state[0x10 / 8] = (tsc >> 32) & 0xFFFFFFFF;
    
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_interrupt(void *vmcb)
{
    /* Handle external interrupt or NMI */
    return NEXUSVM_OK;
}

nexusvm_result_t svm_handle_vmrun(void *vmcb)
{
    /* Nested virtualization: VMX within SVM */
    NEXUSVM_LOG_WARN("Nested VMX not fully implemented\n");
    return NEXUSVM_ERR_NOT_SUPPORTED;
}

#endif /* __amd64__ */
