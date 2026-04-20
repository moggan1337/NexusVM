/**
 * NexusVM Hypervisor Core Implementation
 */

#include <hypervisor/hypervisor.h>
#include <hypervisor/cpu_features.h>
#include <hypervisor/vmx/vmx.h>
#include <hypervisor/amd-v/svm.h>
#include <hypervisor/mmu/ept.h>
#include <hypervisor/sched/scheduler.h>
#include <hypervisor/io/io_emul.h>
#include <utils/log.h>
#include <utils/spinlock.h>
#include <utils/assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Global hypervisor instance */
static hypervisor_t *g_hypervisor = NULL;
static bool g_initialized = false;

/*============================================================================
 * CPU Feature Detection
 *============================================================================*/

/**
 * Detect CPU vendor and features
 */
static void detect_cpu_features(hypervisor_t *hv)
{
    /* Read CPU vendor string */
    char vendor[13];
    vendor[12] = '\0';
    
    /* CPUID with EAX=0 returns vendor string in EBX, EDX, ECX */
    __asm__ volatile (
        "mov $0, %%eax\n\t"
        "cpuid\n\t"
        : "=b" (*(u32 *)vendor),
          "=d" (*(u32 *)(vendor + 8)),
          "=c" (*(u32 *)(vendor + 4))
        :
        : "eax"
    );
    
    if (strncmp(vendor, "GenuineIntel", 12) == 0) {
        hv->cpu_vendor = CPU_VENDOR_INTEL;
        detect_vmx_features(hv);
    } else if (strncmp(vendor, "AuthenticAMD", 12) == 0) {
        hv->cpu_vendor = CPU_VENDOR_AMD;
        detect_svm_features(hv);
    } else {
        hv->cpu_vendor = CPU_VENDOR_UNKNOWN;
        NEXUSVM_LOG_ERROR("Unknown CPU vendor: %.12s\n", vendor);
    }
    
    /* Detect general x86-64 features */
    u32 eax, ebx, ecx, edx;
    
    /* CPUID with EAX=1 returns feature flags in EDX and ECX */
    __asm__ volatile (
        "mov $1, %%eax\n\t"
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        :
        : "memory"
    );
    
    hv->has_rdrand = (edx >> 30) & 1;
    
    /* CPUID with EAX=7, ECX=0 returns extended features in EBX, ECX, EDX */
    __asm__ volatile (
        "mov $7, %%eax\n\t"
        "mov $0, %%ecx\n\t"
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        :
        : "memory"
    );
    
    hv->has_smep = (ebx >> 7) & 1;
    hv->has_smap = (ebx >> 20) & 1;
    hv->has_fsgsbase = (ebx >> 0) & 1;
    hv->has_mpx = (ebx >> 3) & 1;
    hv->has_pcid = (ebx >> 17) & 1;
    hv->has_fsgsbase = (ebx >> 0) & 1;
    hv->has_umip = (ebx >> 2) & 1;
    hv->has_pku = (ebx >> 3) & 1;
    
    /* Check XGETBV and XSAVES */
    u32 xcr0, xss;
    __asm__ volatile (
        "mov $0, %%eax\n\t"
        "xgetbv\n\t"
        : "=a" (xcr0), "=d" (edx)
        :
        : "ecx"
    );
    
    hv->has_xsaves = (ecx >> 3) & 1;
    
    /* Get logical CPU count */
    __asm__ volatile (
        "mov $1, %%eax\n\t"
        "cpuid\n\t"
        : "=b" (ebx)
        :
        : "eax", "ecx", "edx"
    );
    hv->logical_cpu_count = (ebx >> 16) & 0xFF;
    
    NEXUSVM_LOG_INFO("Detected CPU vendor: %s\n", 
                     hv->cpu_vendor == CPU_VENDOR_INTEL ? "Intel" :
                     hv->cpu_vendor == CPU_VENDOR_AMD ? "AMD" : "Unknown");
    NEXUSVM_LOG_INFO("Has VMX: %s, Has SVM: %s\n",
                     hv->has_vmx ? "yes" : "no",
                     hv->has_svm ? "yes" : "no");
    NEXUSVM_LOG_INFO("Has EPT: %s, Has NPT: %s\n",
                     hv->has_ept ? "yes" : "no",
                     hv->has_npt ? "yes" : "no");
    NEXUSVM_LOG_INFO("Has SMEP: %s, Has SMAP: %s, Has PCID: %s\n",
                     hv->has_smep ? "yes" : "no",
                     hv->has_smap ? "yes" : "no",
                     hv->has_pcid ? "yes" : "no");
}

/*============================================================================
 * Hypervisor Initialization
 *============================================================================*/

nexusvm_result_t hypervisor_init(void)
{
    if (g_initialized) {
        NEXUSVM_LOG_WARN("Hypervisor already initialized\n");
        return NEXUSVM_OK;
    }
    
    NEXUSVM_LOG_INFO("Initializing NexusVM Hypervisor...\n");
    
    /* Allocate hypervisor structure */
    g_hypervisor = (hypervisor_t *)calloc(1, sizeof(hypervisor_t));
    if (!g_hypervisor) {
        NEXUSVM_LOG_ERROR("Failed to allocate hypervisor structure\n");
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Detect CPU features */
    detect_cpu_features(g_hypervisor);
    
    /* Check if virtualization is supported */
    if (!g_hypervisor->has_vmx && !g_hypervisor->has_svm) {
        NEXUSVM_LOG_ERROR("Hardware virtualization not supported\n");
        free(g_hypervisor);
        g_hypervisor = NULL;
        return NEXUSVM_ERR_CPU_NOT_SUPPORTED;
    }
    
    /* Initialize scheduler */
    nexusvm_result_t result = scheduler_init();
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to initialize scheduler: %d\n", result);
        free(g_hypervisor);
        g_hypervisor = NULL;
        return result;
    }
    
    /* Initialize hypercall interface */
    result = hypercall_init();
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to initialize hypercalls: %d\n", result);
        scheduler_shutdown();
        free(g_hypervisor);
        g_hypervisor = NULL;
        return result;
    }
    
    /* Initialize memory management */
    result = ept_init();
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to initialize EPT: %d\n", result);
        hypercall_shutdown();
        scheduler_shutdown();
        free(g_hypervisor);
        g_hypervisor = NULL;
        return result;
    }
    
    /* Initialize I/O emulation */
    result = io_emul_init();
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to initialize I/O emulation: %d\n", result);
        ept_shutdown();
        hypercall_shutdown();
        scheduler_shutdown();
        free(g_hypervisor);
        g_hypervisor = NULL;
        return result;
    }
    
    /* Initialize VM list */
    g_hypervisor->vms = NULL;
    g_hypervisor->vm_count = 0;
    g_hypervisor->next_vm_id = 1;
    
    g_initialized = true;
    NEXUSVM_LOG_INFO("NexusVM Hypervisor initialized successfully\n");
    
    return NEXUSVM_OK;
}

/*============================================================================
 * Hypervisor Shutdown
 *============================================================================*/

void hypervisor_shutdown(void)
{
    if (!g_initialized || !g_hypervisor) {
        return;
    }
    
    NEXUSVM_LOG_INFO("Shutting down NexusVM Hypervisor...\n");
    
    /* Stop all VMs */
    nexusvm_t *vm = g_hypervisor->vms;
    while (vm) {
        nexusvm_t *next = vm->next;
        vm_stop(vm);
        vm_destroy(vm);
        vm = next;
    }
    
    /* Shutdown subsystems */
    io_emul_shutdown();
    ept_shutdown();
    hypercall_shutdown();
    scheduler_shutdown();
    
    /* Free hypervisor structure */
    free(g_hypervisor);
    g_hypervisor = NULL;
    g_initialized = false;
    
    NEXUSVM_LOG_INFO("NexusVM Hypervisor shutdown complete\n");
}

/*============================================================================
 * Hypervisor Accessors
 *============================================================================*/

hypervisor_t *hypervisor_get_instance(void)
{
    return g_hypervisor;
}

bool hypervisor_is_running(void)
{
    return g_initialized && g_hypervisor != NULL;
}

cpu_vendor_t hypervisor_get_cpu_vendor(void)
{
    return g_hypervisor ? g_hypervisor->cpu_vendor : CPU_VENDOR_UNKNOWN;
}

u32 hypervisor_get_cpu_count(void)
{
    return g_hypervisor ? g_hypervisor->online_cpu_count : 0;
}

/*============================================================================
 * VM Lifecycle Management
 *============================================================================*/

nexusvm_result_t vm_create(vm_config_t *config, nexusvm_t **vm_out)
{
    if (!g_hypervisor || !config) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Creating VM: %s\n", config->name);
    
    /* Allocate VM structure */
    nexusvm_t *vm = (nexusvm_t *)calloc(1, sizeof(nexusvm_t));
    if (!vm) {
        NEXUSVM_LOG_ERROR("Failed to allocate VM structure\n");
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Assign VM ID */
    vm->id = g_hypervisor->next_vm_id++;
    
    /* Copy configuration */
    strncpy(vm->name, config->name, sizeof(vm->name) - 1);
    vm->config = *config;
    vm->vcpu_count = config->vcpu_count;
    vm->memory_size = config->memory_size;
    
    /* Set initial state */
    vm->state = VM_STATE_CREATED;
    
    /* Allocate memory regions array */
    if (config->memory_region_count > 0) {
        vm->memory_regions = (memory_region_t *)calloc(
            config->memory_region_count, sizeof(memory_region_t));
        if (!vm->memory_regions) {
            free(vm);
            return NEXUSVM_ERR_NO_MEMORY;
        }
    }
    
    /* Allocate EPT root */
    vm->ept_root = ept_alloc_root();
    if (!vm->ept_root) {
        free(vm->memory_regions);
        free(vm);
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Setup EPT pointer */
    vm->ept_pointer = (eptp_t *)calloc(1, sizeof(eptp_t));
    if (!vm->ept_pointer) {
        ept_free_root(vm->ept_root);
        free(vm->memory_regions);
        free(vm);
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    vm->ept_pointer->memory_type = EPT_MEMORY_TYPE_WRITE_BACK;
    vm->ept_pointer->page_walk_length = 4 - 1; /* 4-level paging */
    vm->ept_pointer->enable_accessed_dirty = 1;
    vm->ept_pointer->pml4_physical_address = 
        ((phys_addr_t)vm->ept_root >> 12) & 0xFFFFFFFFFF;
    
    /* Allocate vCPUs */
    vm->vcpus = (nexusvcpu_t **)calloc(config->vcpu_count, sizeof(nexusvcpu_t *));
    if (!vm->vcpus) {
        free(vm->ept_pointer);
        ept_free_root(vm->ept_root);
        free(vm->memory_regions);
        free(vm);
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    for (u32 i = 0; i < config->vcpu_count; i++) {
        nexusvm_result_t result = vcpu_create(vm, i, &vm->vcpus[i]);
        if (result != NEXUSVM_OK) {
            /* Cleanup on failure */
            for (u32 j = 0; j < i; j++) {
                vcpu_destroy(vm->vcpus[j]);
            }
            free(vm->vcpus);
            free(vm->ept_pointer);
            ept_free_root(vm->ept_root);
            free(vm->memory_regions);
            free(vm);
            return result;
        }
    }
    
    /* Add to VM list */
    vm->next = g_hypervisor->vms;
    if (g_hypervisor->vms) {
        g_hypervisor->vms->prev = vm;
    }
    g_hypervisor->vms = vm;
    g_hypervisor->vm_count++;
    
    vm->state = VM_STATE_INITIALIZED;
    
    NEXUSVM_LOG_INFO("VM created: ID=%u, vCPUs=%u, Memory=%lu MB\n",
                     vm->id, vm->vcpu_count, vm->memory_size / (1024 * 1024));
    
    *vm_out = vm;
    return NEXUSVM_OK;
}

nexusvm_result_t vm_destroy(nexusvm_t *vm)
{
    if (!vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (vm->state == VM_STATE_RUNNING) {
        vm_stop(vm);
    }
    
    NEXUSVM_LOG_INFO("Destroying VM: %s (ID=%u)\n", vm->name, vm->id);
    
    /* Destroy vCPUs */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        if (vm->vcpus[i]) {
            vcpu_destroy(vm->vcpus[i]);
        }
    }
    free(vm->vcpus);
    
    /* Free EPT */
    if (vm->ept_root) {
        ept_free_root(vm->ept_root);
    }
    free(vm->ept_pointer);
    
    /* Free memory regions */
    for (u32 i = 0; i < vm->memory_region_count; i++) {
        if (vm->memory_regions[i].name) {
            free(vm->memory_regions[i].name);
        }
    }
    free(vm->memory_regions);
    
    /* Remove from VM list */
    if (vm->prev) {
        vm->prev->next = vm->next;
    } else {
        g_hypervisor->vms = vm->next;
    }
    if (vm->next) {
        vm->next->prev = vm->prev;
    }
    g_hypervisor->vm_count--;
    
    /* Free nested virtualization state */
    if (vm->nested_vmx) {
        free(vm->nested_vmx);
    }
    if (vm->nested_svm) {
        free(vm->nested_svm);
    }
    
    free(vm);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vm_start(nexusvm_t *vm)
{
    if (!vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (vm->state == VM_STATE_RUNNING) {
        NEXUSVM_LOG_WARN("VM already running: %s\n", vm->name);
        return NEXUSVM_OK;
    }
    
    if (vm->state != VM_STATE_INITIALIZED && 
        vm->state != VM_STATE_PAUSED &&
        vm->state != VM_STATE_SHUTDOWN) {
        NEXUSVM_LOG_ERROR("Cannot start VM in state %d\n", vm->state);
        return NEXUSVM_ERR_VM_NOT_RUNNING;
    }
    
    NEXUSVM_LOG_INFO("Starting VM: %s\n", vm->name);
    
    /* Start all vCPUs */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        nexusvm_result_t result = vcpu_start(vm->vcpus[i]);
        if (result != NEXUSVM_OK) {
            NEXUSVM_LOG_ERROR("Failed to start vCPU %u: %d\n", i, result);
            /* Stop already started vCPUs */
            for (u32 j = 0; j < i; j++) {
                vcpu_stop(vm->vcpus[j]);
            }
            return result;
        }
    }
    
    vm->state = VM_STATE_RUNNING;
    
    return NEXUSVM_OK;
}

nexusvm_result_t vm_stop(nexusvm_t *vm)
{
    if (!vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (vm->state != VM_STATE_RUNNING) {
        NEXUSVM_LOG_WARN("VM not running: %s\n", vm->name);
        return NEXUSVM_ERR_VM_NOT_RUNNING;
    }
    
    NEXUSVM_LOG_INFO("Stopping VM: %s\n", vm->name);
    
    /* Stop all vCPUs */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        vcpu_stop(vm->vcpus[i]);
    }
    
    vm->state = VM_STATE_SHUTDOWN;
    
    return NEXUSVM_OK;
}

nexusvm_result_t vm_pause(nexusvm_t *vm)
{
    if (!vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (vm->state != VM_STATE_RUNNING) {
        return NEXUSVM_ERR_VM_NOT_RUNNING;
    }
    
    NEXUSVM_LOG_INFO("Pausing VM: %s\n", vm->name);
    
    /* Pause all vCPUs */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        nexusvcpu_t *vcpu = vm->vcpus[i];
        if (vcpu->running) {
            vcpu->running = false;
        }
    }
    
    vm->state = VM_STATE_PAUSED;
    
    return NEXUSVM_OK;
}

nexusvm_result_t vm_resume(nexusvm_t *vm)
{
    if (!vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (vm->state != VM_STATE_PAUSED) {
        return NEXUSVM_ERR_VM_RUNNING;
    }
    
    NEXUSVM_LOG_INFO("Resuming VM: %s\n", vm->name);
    
    /* Resume all vCPUs */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        nexusvcpu_t *vcpu = vm->vcpus[i];
        if (vcpu->initialized) {
            vcpu->running = true;
        }
    }
    
    vm->state = VM_STATE_RUNNING;
    
    return NEXUSVM_OK;
}

nexusvm_result_t vm_reset(nexusvm_t *vm, bool cold)
{
    if (!vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("%s resetting VM: %s\n", 
                     cold ? "Cold" : "Warm", vm->name);
    
    /* Stop if running */
    if (vm->state == VM_STATE_RUNNING) {
        vm_stop(vm);
    }
    
    /* Reset vCPU states */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        nexusvcpu_t *vcpu = vm->vcpus[i];
        memset(&vcpu->cpu_state, 0, sizeof(cpu_state_t));
        /* Set initial register values for reset */
        vcpu->cpu_state.gprs.rsp = 0xFFFFFFF0;
        vcpu->cpu_state.gprs.rip = 0xFFF0;
        vcpu->cpu_state.gprs.rflags = 2; /* Reserved bit set */
    }
    
    /* Clear EPT if cold reset */
    if (cold) {
        ept_clear_root(vm->ept_root);
    }
    
    vm->state = VM_STATE_SHUTDOWN;
    
    return NEXUSVM_OK;
}

/*============================================================================
 * VM Accessors
 *============================================================================*/

nexusvm_t *vm_get_by_id(u32 vm_id)
{
    nexusvm_t *vm = g_hypervisor->vms;
    while (vm) {
        if (vm->id == vm_id) {
            return vm;
        }
        vm = vm->next;
    }
    return NULL;
}

nexusvm_t *vm_get_by_name(const char *name)
{
    if (!name) return NULL;
    
    nexusvm_t *vm = g_hypervisor->vms;
    while (vm) {
        if (strcmp(vm->name, name) == 0) {
            return vm;
        }
        vm = vm->next;
    }
    return NULL;
}

u32 vm_enumerate(u32 *vm_ids, u32 max_count)
{
    if (!vm_ids || max_count == 0) return 0;
    
    u32 count = 0;
    nexusvm_t *vm = g_hypervisor->vms;
    
    while (vm && count < max_count) {
        vm_ids[count++] = vm->id;
        vm = vm->next;
    }
    
    return count;
}

/*============================================================================
 * Memory Management
 *============================================================================*/

nexusvm_result_t vm_alloc_memory(nexusvm_t *vm, guest_addr_t guest_phys,
                                  u64 size, memory_region_type_t region_type)
{
    if (!vm || size == 0) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Allocate host memory */
    phys_addr_t host_phys;
    void *host_mem = ept_alloc_pages(size / PAGE_SIZE + 1, &host_phys);
    if (!host_mem) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Create memory region */
    memory_region_t *region = &vm->memory_regions[vm->memory_region_count++];
    region->guest_phys = guest_phys;
    region->host_phys = host_phys;
    region->size = size;
    region->type = region_type;
    region->flags = MEM_REGION_READABLE | MEM_REGION_WRITABLE;
    
    /* Map in EPT */
    nexusvm_result_t result = vm_ept_map(vm, guest_phys, host_phys, size,
                                          true, true, false);
    if (result != NEXUSVM_OK) {
        ept_free_pages(host_mem);
        return result;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t vm_ept_map(nexusvm_t *vm, guest_addr_t guest_phys,
                             phys_addr_t host_phys, u64 size,
                             bool read, bool write, bool execute)
{
    if (!vm || !vm->ept_root) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    u64 pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
    
    for (u64 i = 0; i < pages; i++) {
        guest_addr_t gpa = guest_phys + (i << PAGE_SHIFT);
        phys_addr_t hpa = host_phys + (i << PAGE_SHIFT);
        
        ept_pte_t pte;
        memset(&pte, 0, sizeof(pte));
        pte.present = 1;
        pte.write = write ? 1 : 0;
        pte.executable = execute ? 0 : 1; /* In EPT, 0 = executable */
        pte.memory_type = EPT_MEMORY_TYPE_WRITE_BACK;
        pte.physical_address = (hpa >> 12) & 0xFFFFFFFFFF;
        
        nexusvm_result_t result = ept_map(vm->ept_root, gpa, &pte);
        if (result != NEXUSVM_OK) {
            NEXUSVM_LOG_ERROR("EPT map failed at GPA 0x%lx\n", gpa);
            return result;
        }
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t vm_ept_unmap(nexusvm_t *vm, guest_addr_t guest_phys, u64 size)
{
    if (!vm || !vm->ept_root) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    u64 pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
    
    for (u64 i = 0; i < pages; i++) {
        guest_addr_t gpa = guest_phys + (i << PAGE_SHIFT);
        ept_unmap(vm->ept_root, gpa);
    }
    
    return NEXUSVM_OK;
}

phys_addr_t vm_gpa_to_hpa(nexusvm_t *vm, guest_addr_t guest_phys)
{
    if (!vm || !vm->ept_root) {
        return INVALID_PHYS_ADDR;
    }
    
    ept_pte_t pte;
    if (ept_lookup(vm->ept_root, guest_phys, &pte) != NEXUSVM_OK) {
        return INVALID_PHYS_ADDR;
    }
    
    if (!pte.present) {
        return INVALID_PHYS_ADDR;
    }
    
    u64 page_offset = guest_phys & (PAGE_SIZE - 1);
    return (pte.physical_address << 12) | page_offset;
}

eptp_t *vm_get_eptp(nexusvm_t *vm)
{
    return vm ? vm->ept_pointer : NULL;
}

/*============================================================================
 * Virtual CPU Management
 *============================================================================*/

nexusvm_result_t vcpu_create(nexusvm_t *vm, u32 vcpu_id, nexusvcpu_t **vcpu_out)
{
    if (!vm || !vcpu_out) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    nexusvcpu_t *vcpu = (nexusvcpu_t *)calloc(1, sizeof(nexusvcpu_t));
    if (!vcpu) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    vcpu->vcpu_id = vcpu_id;
    vcpu->vm = vm;
    vcpu->physical_cpu = vcpu_id % g_hypervisor->online_cpu_count;
    
    /* Initialize CPU state */
    memset(&vcpu->cpu_state, 0, sizeof(cpu_state_t));
    memset(&vcpu->host_state, 0, sizeof(cpu_state_t));
    
    /* Set initial state */
    vcpu->running = false;
    vcpu->initialized = false;
    
    /* Allocate MSR bitmap (required for MSR interception) */
    vcpu->msr_bitmap = calloc(1, 4096 * 2); /* 4KB read + 4KB write */
    if (!vcpu->msr_bitmap) {
        free(vcpu);
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Allocate I/O bitmaps */
    vcpu->io_bitmap_a = calloc(1, 4096);
    vcpu->io_bitmap_b = calloc(1, 4096);
    if (!vcpu->io_bitmap_a || !vcpu->io_bitmap_b) {
        free(vcpu->msr_bitmap);
        free(vcpu->io_bitmap_a);
        free(vcpu->io_bitmap_b);
        free(vcpu);
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Initialize VMX or SVM based on CPU */
    if (g_hypervisor->cpu_vendor == CPU_VENDOR_INTEL) {
        nexusvm_result_t result = vmx_vcpu_init(vcpu);
        if (result != NEXUSVM_OK) {
            free(vcpu->msr_bitmap);
            free(vcpu->io_bitmap_a);
            free(vcpu->io_bitmap_b);
            free(vcpu);
            return result;
        }
    } else if (g_hypervisor->cpu_vendor == CPU_VENDOR_AMD) {
        nexusvm_result_t result = svm_vcpu_init(vcpu);
        if (result != NEXUSVM_OK) {
            free(vcpu->msr_bitmap);
            free(vcpu->io_bitmap_a);
            free(vcpu->io_bitmap_b);
            free(vcpu);
            return result;
        }
    }
    
    *vcpu_out = vcpu;
    return NEXUSVM_OK;
}

nexusvm_result_t vcpu_destroy(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (vcpu->running) {
        vcpu_stop(vcpu);
    }
    
    /* Free VMX/SVM resources */
    if (g_hypervisor->cpu_vendor == CPU_VENDOR_INTEL) {
        vmx_vcpu_cleanup(vcpu);
    } else if (g_hypervisor->cpu_vendor == CPU_VENDOR_AMD) {
        svm_vcpu_cleanup(vcpu);
    }
    
    /* Free bitmaps */
    free(vcpu->msr_bitmap);
    free(vcpu->io_bitmap_a);
    free(vcpu->io_bitmap_b);
    
    free(vcpu);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vcpu_start(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (vcpu->running) {
        return NEXUSVM_OK;
    }
    
    if (!vcpu->initialized) {
        /* Initialize vCPU */
        nexusvm_result_t result;
        
        if (g_hypervisor->cpu_vendor == CPU_VENDOR_INTEL) {
            result = vmx_vcpu_setup(vcpu);
        } else if (g_hypervisor->cpu_vendor == CPU_VENDOR_AMD) {
            result = svm_vcpu_setup(vcpu);
        } else {
            return NEXUSVM_ERR_NOT_SUPPORTED;
        }
        
        if (result != NEXUSVM_OK) {
            return result;
        }
        
        vcpu->initialized = true;
    }
    
    vcpu->running = true;
    
    /* Schedule vCPU */
    scheduler_add_vcpu(vcpu);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vcpu_stop(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    vcpu->running = false;
    
    /* Remove from scheduler */
    scheduler_remove_vcpu(vcpu);
    
    return NEXUSVM_OK;
}

nexusvm_result_t vcpu_run(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->initialized) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    vcpu->vmentry_count++;
    
    if (g_hypervisor->cpu_vendor == CPU_VENDOR_INTEL) {
        return vmx_vcpu_run(vcpu);
    } else if (g_hypervisor->cpu_vendor == CPU_VENDOR_AMD) {
        return svm_vcpu_run(vcpu);
    }
    
    return NEXUSVM_ERR_NOT_SUPPORTED;
}

nexusvm_result_t vcpu_get_state(nexusvcpu_t *vcpu, cpu_state_t *state)
{
    if (!vcpu || !state) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    memcpy(state, &vcpu->cpu_state, sizeof(cpu_state_t));
    return NEXUSVM_OK;
}

nexusvm_result_t vcpu_set_state(nexusvcpu_t *vcpu, cpu_state_t *state)
{
    if (!vcpu || !state) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    memcpy(&vcpu->cpu_state, state, sizeof(cpu_state_t));
    return NEXUSVM_OK;
}

nexusvm_result_t vcpu_set_affinity(nexusvcpu_t *vcpu, affinity_t *affinity)
{
    if (!vcpu || !affinity) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    vcpu->sched_entity->allowed_cpus = *affinity;
    return NEXUSVM_OK;
}

nexusvm_result_t vcpu_migrate(nexusvcpu_t *vcpu, u32 target_cpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (target_cpu >= g_hypervisor->online_cpu_count) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Migrating vCPU %u from CPU %u to CPU %u\n",
                     vcpu->vcpu_id, vcpu->physical_cpu, target_cpu);
    
    vcpu->physical_cpu = target_cpu;
    return NEXUSVM_OK;
}

/*============================================================================
 * Hypercall Handler
 *============================================================================*/

nexusvm_result_t hypercall_handle(nexusvcpu_t *vcpu, hypercall_params_t *params,
                                   hypercall_result_t *result)
{
    if (!vcpu || !params || !result) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    u64 call_number = params->rax;
    
    result->status = NEXUSVM_OK;
    result->ret0 = 0;
    result->ret1 = 0;
    result->ret2 = 0;
    result->ret3 = 0;
    
    switch (call_number) {
        case HYPERCALL_VM_GET_INFO: {
            nexusvm_t *vm = vcpu->vm;
            result->ret0 = vm->id;
            result->ret1 = vm->vcpu_count;
            result->ret2 = vm->memory_size;
            result->ret3 = vm->state;
            break;
        }
        
        case HYPERCALL_VCPU_GET_STATE: {
            cpu_state_t *state = (cpu_state_t *)params->rbx;
            if (state) {
                memcpy(state, &vcpu->cpu_state, sizeof(cpu_state_t));
            }
            break;
        }
        
        case HYPERCALL_VCPU_SET_STATE: {
            cpu_state_t *state = (cpu_state_t *)params->rbx;
            if (state) {
                memcpy(&vcpu->cpu_state, state, sizeof(cpu_state_t));
            }
            break;
        }
        
        case HYPERCALL_VM_GET_STATISTICS: {
            nexusvm_t *vm = vcpu->vm;
            result->ret0 = vm->uptime_ns;
            result->ret1 = vm->vmexit_count;
            result->ret2 = vm->total_runtime_ns;
            break;
        }
        
        case HYPERCALL_VCPU_GET_STATISTICS: {
            result->ret0 = vcpu->vmexit_count;
            result->ret1 = vcpu->vmentry_count;
            result->ret2 = vcpu->instruction_count;
            break;
        }
        
        default:
            NEXUSVM_LOG_WARN("Unknown hypercall: 0x%lx\n", call_number);
            result->status = NEXUSVM_ERR_NOT_SUPPORTED;
            break;
    }
    
    return result->status;
}
