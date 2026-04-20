/**
 * NexusVM Hypervisor Core Module
 * 
 * This module implements the core hypervisor functionality including
 * VM lifecycle management, CPU virtualization, and memory management.
 */

#ifndef NEXUSVM_HYPERVISOR_H
#define NEXUSVM_HYPERVISOR_H

#include <hypervisor/types.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Hypervisor Instance
 *============================================================================*/

/**
 * Global hypervisor state
 */
typedef struct hypervisor {
    cpu_vendor_t cpu_vendor;
    u32 cpu_count;
    u32 online_cpu_count;
    u32 logical_cpu_count;
    
    /* VM Management */
    struct nexusvm *vms;
    u32 vm_count;
    u32 next_vm_id;
    
    /* Scheduler */
    sched_entity_t *scheduler_runqueue;
    sched_entity_t *current_entity;
    
    /* Memory Management */
    phys_addr_t high_memory_start;
    u64 total_ram;
    u64 free_ram;
    
    /* Capabilities */
    bool has_vmx;
    bool has_svm;
    bool has_ept;
    bool has_npt;
    bool has_vpid;
    bool has_pcid;
    bool has_xtpr;
    bool has_fsgsbase;
    bool has_rdrand;
    bool has_rdseed;
    bool has_mpx;
    bool has_xsaves;
    bool has_smep;
    bool has_smap;
    bool has_umip;
    bool has_pku;
    
    /* VMX specific */
    vmx_basic_msr_t vmx_basic;
    u64 vmx_cap_pinbased;
    u64 vmx_cap_procbased;
    u64 vmx_cap_procbased2;
    u64 vmx_cap_exit;
    u64 vmx_cap_entry;
    u64 vmx_cap_ept_vpid;
    
    /* Statistics */
    u64 total_vmexits;
    u64 total_vmentries;
    
    /* Lock for hypervisor state */
    void *lock;
} hypervisor_t;

/*============================================================================
 * Virtual Machine Structure
 *============================================================================*/

/**
 * NexusVM Virtual Machine
 */
typedef struct nexusvm {
    u32 id;
    char name[64];
    vm_state_t state;
    
    /* Virtual CPUs */
    struct nexusvcpu *vcpus;
    u32 vcpu_count;
    
    /* Memory */
    memory_region_t *memory_regions;
    u32 memory_region_count;
    u64 memory_size;
    
    /* Extended Page Tables */
    eptp_t *ept_pointer;
    void *ept_root;
    
    /* Nested Virtualization */
    nested_vmx_t *nested_vmx;
    nested_svm_t *nested_svm;
    bool is_nested_guest;
    
    /* Device Passthrough */
    struct pci_device *assigned_devices;
    u32 assigned_device_count;
    
    /* Interrupt Controller */
    void *lapic;
    void *ioapic;
    void *pic;
    
    /* Scheduler */
    sched_entity_t *sched_entity;
    
    /* Configuration */
    vm_config_t config;
    
    /* Statistics */
    u64 uptime_ns;
    u64 total_runtime_ns;
    u64 vmexit_count;
    
    /* Links */
    struct nexusvm *next;
    struct nexusvm *prev;
} nexusvm_t;

/**
 * Virtual CPU
 */
typedef struct nexusvcpu {
    u32 vcpu_id;
    u32 apic_id;
    u32 physical_cpu;
    
    /* VM Context */
    nexusvm_t *vm;
    
    /* CPU State */
    cpu_state_t cpu_state;
    
    /* VMX/SVM specific */
    void *vmcs;               /* VMX */
    void *vmcb;               /* SVM */
    void *vmx_on_region;
    void *vmx_vmcs_region;
    
    /* MSR Bitmap */
    void *msr_bitmap;
    
    /* I/O Bitmap */
    void *io_bitmap_a;
    void *io_bitmap_b;
    
    /* EPT */
    eptp_t *eptp;
    
    /* Scheduler */
    sched_entity_t *sched_entity;
    
    /* Statistics */
    u64 vmexit_count;
    u64 vmentry_count;
    u64 instruction_count;
    
    /* Local APIC */
    void *lapic;
    
    /* Running state */
    bool running;
    bool initialized;
    
    /* Per-VCPU lock */
    void *lock;
    
    /* Host state save area */
    cpu_state_t host_state;
} nexusvcpu_t;

/*============================================================================
 * Hypervisor Functions
 *============================================================================*/

/**
 * Initialize the hypervisor
 * @return NEXUSVM_OK on success, error code otherwise
 */
nexusvm_result_t hypervisor_init(void);

/**
 * Shutdown the hypervisor
 */
void hypervisor_shutdown(void);

/**
 * Get hypervisor instance
 */
hypervisor_t *hypervisor_get_instance(void);

/**
 * Check if hypervisor is running
 */
bool hypervisor_is_running(void);

/**
 * Get CPU vendor
 */
cpu_vendor_t hypervisor_get_cpu_vendor(void);

/**
 * Get number of online CPUs
 */
u32 hypervisor_get_cpu_count(void);

/*============================================================================
 * VM Lifecycle Management
 *============================================================================*/

/**
 * Create a new virtual machine
 * @param config VM configuration
 * @param vm Output parameter for created VM
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_create(vm_config_t *config, nexusvm_t **vm);

/**
 * Destroy a virtual machine
 * @param vm VM to destroy
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_destroy(nexusvm_t *vm);

/**
 * Start a virtual machine
 * @param vm VM to start
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_start(nexusvm_t *vm);

/**
 * Stop a virtual machine
 * @param vm VM to stop
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_stop(nexusvm_t *vm);

/**
 * Pause a virtual machine
 * @param vm VM to pause
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_pause(nexusvm_t *vm);

/**
 * Resume a paused virtual machine
 * @param vm VM to resume
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_resume(nexusvm_t *vm);

/**
 * Reset a virtual machine
 * @param vm VM to reset
 * @param cold True for cold reset, false for warm reset
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_reset(nexusvm_t *vm, bool cold);

/**
 * Get VM by ID
 * @param vm_id VM identifier
 * @return VM pointer or NULL if not found
 */
nexusvm_t *vm_get_by_id(u32 vm_id);

/**
 * Get VM by name
 * @param name VM name
 * @return VM pointer or NULL if not found
 */
nexusvm_t *vm_get_by_name(const char *name);

/**
 * Enumerate all VMs
 * @param vm_ids Output array for VM IDs
 * @param max_count Maximum number of IDs to return
 * @return Number of VMs
 */
u32 vm_enumerate(u32 *vm_ids, u32 max_count);

/*============================================================================
 * Virtual CPU Management
 *============================================================================*/

/**
 * Create a virtual CPU
 * @param vm Parent VM
 * @param vcpu_id Virtual CPU ID
 * @param vcpu Output parameter for created vCPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vcpu_create(nexusvm_t *vm, u32 vcpu_id, nexusvcpu_t **vcpu);

/**
 * Destroy a virtual CPU
 * @param vcpu vCPU to destroy
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vcpu_destroy(nexusvcpu_t *vcpu);

/**
 * Start a virtual CPU
 * @param vcpu vCPU to start
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vcpu_start(nexusvcpu_t *vcpu);

/**
 * Stop a virtual CPU
 * @param vcpu vCPU to stop
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vcpu_stop(nexusvcpu_t *vcpu);

/**
 * Run a virtual CPU (main execution loop)
 * @param vcpu vCPU to run
 * @return NEXUSVM_OK on clean exit
 */
nexusvm_result_t vcpu_run(nexusvcpu_t *vcpu);

/**
 * Set vCPU affinity
 * @param vcpu vCPU to set affinity
 * @param affinity CPU affinity mask
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vcpu_set_affinity(nexusvcpu_t *vcpu, affinity_t *affinity);

/**
 * Migrate vCPU to another physical CPU
 * @param vcpu vCPU to migrate
 * @param target_cpu Target physical CPU
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vcpu_migrate(nexusvcpu_t *vcpu, u32 target_cpu);

/**
 * Get vCPU state
 * @param vcpu vCPU
 * @param state Output state buffer
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vcpu_get_state(nexusvcpu_t *vcpu, cpu_state_t *state);

/**
 * Set vCPU state
 * @param vcpu vCPU
 * @param state State to set
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vcpu_set_state(nexusvcpu_t *vcpu, cpu_state_t *state);

/*============================================================================
 * Memory Management
 *============================================================================*/

/**
 * Allocate memory for a VM
 * @param vm VM
 * @param guest_phys Guest physical address
 * @param size Size to allocate
 * @param region_type Memory type
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_alloc_memory(nexusvm_t *vm, guest_addr_t guest_phys,
                                  u64 size, memory_region_type_t region_type);

/**
 * Free VM memory
 * @param vm VM
 * @param guest_phys Guest physical address
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_free_memory(nexusvm_t *vm, guest_addr_t guest_phys);

/**
 * Map memory in EPT
 * @param vm VM
 * @param guest_phys Guest physical address
 * @param host_phys Host physical address
 * @param size Size of mapping
 * @param read Read permission
 * @param write Write permission
 * @param execute Execute permission
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_ept_map(nexusvm_t *vm, guest_addr_t guest_phys,
                             phys_addr_t host_phys, u64 size,
                             bool read, bool write, bool execute);

/**
 * Unmap memory from EPT
 * @param vm VM
 * @param guest_phys Guest physical address
 * @param size Size to unmap
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t vm_ept_unmap(nexusvm_t *vm, guest_addr_t guest_phys, u64 size);

/**
 * Get EPTP for VM
 * @param vm VM
 * @return EPTP value
 */
eptp_t *vm_get_eptp(nexusvm_t *vm);

/**
 * Translate guest physical to host physical
 * @param vm VM
 * @param guest_phys Guest physical address
 * @return Host physical address or INVALID_PHYS_ADDR if not mapped
 */
phys_addr_t vm_gpa_to_hpa(nexusvm_t *vm, guest_addr_t guest_phys);

/*============================================================================
 * Hypercall Interface
 *============================================================================*/

/**
 * Handle hypercall from guest
 * @param vcpu vCPU making hypercall
 * @param params Hypercall parameters
 * @param result Hypercall result
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t hypercall_handle(nexusvcpu_t *vcpu, hypercall_params_t *params,
                                   hypercall_result_t *result);

/**
 * Initialize hypercall interface
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t hypercall_init(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUSVM_HYPERVISOR_H */
