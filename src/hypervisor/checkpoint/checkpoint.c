/**
 * NexusVM Checkpoint Implementation
 */

#include <hypervisor/checkpoint/checkpoint.h>
#include <hypervisor/hypervisor.h>
#include <utils/log.h>
#include <utils/crc32.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* Checkpoint file format:
 * [Header] [VM Config] [vCPU States] [Device State] [Memory Pages]
 */

/*============================================================================
 * File I/O Helpers
 *============================================================================*/

static FILE *checkpoint_open(const char *path, const char *mode)
{
    return fopen(path, mode);
}

static void checkpoint_close(FILE *f)
{
    if (f) fclose(f);
}

static nexusvm_result_t checkpoint_write(FILE *f, const void *data, u64 size)
{
    if (fwrite(data, 1, size, f) != size) {
        return NEXUSVM_ERR_GENERIC;
    }
    return NEXUSVM_OK;
}

static nexusvm_result_t checkpoint_read(FILE *f, void *data, u64 size)
{
    if (fread(data, 1, size, f) != size) {
        return NEXUSVM_ERR_GENERIC;
    }
    return NEXUSVM_OK;
}

/*============================================================================
 * Checkpoint Creation
 *============================================================================*/

nexusvm_result_t checkpoint_create(nexusvm_t *vm, const char *path)
{
    if (!vm || !path) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Creating checkpoint of VM %s\n", vm->name);
    
    FILE *f = checkpoint_open(path, "wb");
    if (!f) {
        NEXUSVM_LOG_ERROR("Failed to create checkpoint file: %s\n", path);
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Write header */
    checkpoint_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic[0] = 'N';
    header.magic[1] = 'X';
    header.magic[2] = 'V';
    header.magic[3] = 'M';
    header.magic[4] = 'C';
    header.magic[5] = 'H';
    header.magic[6] = 'K';
    header.magic[7] = 'P';
    header.version = NEXUSVM_CHECKPOINT_VERSION;
    header.timestamp = time(NULL);
    header.vm_id = vm->id;
    header.vcpu_count = vm->vcpu_count;
    header.memory_regions = vm->memory_region_count;
    header.memory_size = vm->memory_size;
    strncpy(header.vm_name, vm->name, sizeof(header.vm_name) - 1);
    header.state = vm->state;
    
    nexusvm_result_t result = checkpoint_write(f, &header, sizeof(header));
    if (result != NEXUSVM_OK) {
        checkpoint_close(f);
        return result;
    }
    
    /* Write VM config */
    result = checkpoint_write(f, &vm->config, sizeof(vm_config_t));
    if (result != NEXUSVM_OK) {
        checkpoint_close(f);
        return result;
    }
    
    /* Write vCPU states */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        cpu_state_t *state = &vm->vcpus[i]->cpu_state;
        result = checkpoint_write(f, state, sizeof(cpu_state_t));
        if (result != NEXUSVM_OK) {
            checkpoint_close(f);
            return result;
        }
    }
    
    /* Write device state */
    void *device_state;
    u64 device_size;
    result = checkpoint_save_device_state(vm, &device_state, &device_size);
    if (result == NEXUSVM_OK) {
        result = checkpoint_write(f, &device_size, sizeof(device_size));
        if (result == NEXUSVM_OK) {
            result = checkpoint_write(f, device_state, device_size);
        }
        free(device_state);
    }
    
    /* Write memory */
    result = checkpoint_save_memory(vm, path); /* Append memory to file */
    
    checkpoint_close(f);
    
    NEXUSVM_LOG_INFO("Checkpoint created: %s\n", path);
    
    return result;
}

nexusvm_result_t snapshot_create(nexusvm_t *vm, const char *path)
{
    if (!vm || !path) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Creating snapshot of VM %s\n", vm->name);
    
    /* Snapshot is similar to checkpoint but VM keeps running */
    vm->state = VM_STATE_SUSPENDED;
    
    nexusvm_result_t result = checkpoint_create(vm, path);
    
    if (result == NEXUSVM_OK) {
        vm->state = VM_STATE_RUNNING;
    }
    
    return result;
}

/*============================================================================
 * Checkpoint Restoration
 *============================================================================*/

nexusvm_result_t checkpoint_restore(const char *path, nexusvm_t **vm_out)
{
    if (!path || !vm_out) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Restoring checkpoint from %s\n", path);
    
    FILE *f = checkpoint_open(path, "rb");
    if (!f) {
        NEXUSVM_LOG_ERROR("Failed to open checkpoint file: %s\n", path);
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Read header */
    checkpoint_header_t header;
    nexusvm_result_t result = checkpoint_read(f, &header, sizeof(header));
    if (result != NEXUSVM_OK) {
        checkpoint_close(f);
        return result;
    }
    
    /* Verify magic */
    if (strncmp(header.magic, "NXVHMCHKP", 8) != 0) {
        NEXUSVM_LOG_ERROR("Invalid checkpoint magic\n");
        checkpoint_close(f);
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Read VM config */
    vm_config_t config;
    result = checkpoint_read(f, &config, sizeof(config));
    if (result != NEXUSVM_OK) {
        checkpoint_close(f);
        return result;
    }
    
    /* Create VM */
    nexusvm_t *vm;
    result = vm_create(&config, &vm);
    if (result != NEXUSVM_OK) {
        checkpoint_close(f);
        return result;
    }
    
    /* Read vCPU states */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        cpu_state_t state;
        result = checkpoint_read(f, &state, sizeof(state));
        if (result != NEXUSVM_OK) {
            vm_destroy(vm);
            checkpoint_close(f);
            return result;
        }
        vcpu_set_state(vm->vcpus[i], &state);
    }
    
    /* Read device state */
    u64 device_size;
    result = checkpoint_read(f, &device_size, sizeof(device_size));
    if (result == NEXUSVM_OK && device_size > 0) {
        void *device_state = malloc(device_size);
        if (device_state) {
            result = checkpoint_read(f, device_state, device_size);
            if (result == NEXUSVM_OK) {
                checkpoint_restore_device_state(vm, device_state, device_size);
            }
            free(device_state);
        }
    }
    
    checkpoint_close(f);
    
    /* Restore memory */
    checkpoint_restore_memory(vm, path);
    
    NEXUSVM_LOG_INFO("Checkpoint restored: %s\n", vm->name);
    
    *vm_out = vm;
    return NEXUSVM_OK;
}

nexusvm_result_t snapshot_restore(nexusvm_t *vm, const char *path)
{
    if (!vm || !path) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Restoring snapshot to VM %s\n", vm->name);
    
    /* For snapshot restore, we keep VM structure but restore state */
    FILE *f = checkpoint_open(path, "rb");
    if (!f) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Skip header */
    fseek(f, sizeof(checkpoint_header_t), SEEK_SET);
    
    /* Skip config */
    fseek(f, sizeof(vm_config_t), SEEK_CUR);
    
    /* Restore vCPU states */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        cpu_state_t state;
        checkpoint_read(f, &state, sizeof(state));
        vcpu_set_state(vm->vcpus[i], &state);
    }
    
    /* Skip device state */
    u64 device_size;
    checkpoint_read(f, &device_size, sizeof(device_size));
    fseek(f, device_size, SEEK_CUR);
    
    checkpoint_close(f);
    
    /* Restore memory */
    return checkpoint_restore_memory(vm, path);
}

/*============================================================================
 * Checkpoint File Operations
 *============================================================================*/

nexusvm_result_t checkpoint_get_info(const char *path, checkpoint_header_t *info)
{
    if (!path || !info) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    FILE *f = checkpoint_open(path, "rb");
    if (!f) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    nexusvm_result_t result = checkpoint_read(f, info, sizeof(checkpoint_header_t));
    checkpoint_close(f);
    
    return result;
}

u32 checkpoint_list(const char *dir, checkpoint_header_t *entries, u32 max_entries)
{
    if (!dir || !entries || max_entries == 0) {
        return 0;
    }
    
    /* Would use opendir/readdir in real implementation */
    /* For now, return 0 */
    return 0;
}

nexusvm_result_t checkpoint_delete(const char *path)
{
    if (!path) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    if (remove(path) != 0) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    NEXUSVM_LOG_INFO("Checkpoint deleted: %s\n", path);
    
    return NEXUSVM_OK;
}

/*============================================================================
 * Incremental Checkpoints
 *============================================================================*/

nexusvm_result_t checkpoint_create_incremental(nexusvm_t *vm, 
                                                const char *base_checkpoint,
                                                const char *path)
{
    if (!vm || !path) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Creating incremental checkpoint\n");
    
    /* Read base checkpoint header */
    checkpoint_header_t base_header;
    nexusvm_result_t result = checkpoint_get_info(base_checkpoint, &base_header);
    if (result != NEXUSVM_OK) {
        /* No base, create full checkpoint */
        return checkpoint_create(vm, path);
    }
    
    /* Create incremental checkpoint */
    FILE *f = checkpoint_open(path, "wb");
    if (!f) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Write header */
    checkpoint_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic[0] = 'N';
    header.magic[1] = 'X';
    header.magic[2] = 'V';
    header.magic[3] = 'M';
    header.magic[4] = 'I';
    header.magic[5] = 'N';
    header.magic[6] = 'C';
    header.magic[7] = 'K';
    header.version = NEXUSVM_CHECKPOINT_VERSION;
    header.timestamp = time(NULL);
    header.vm_id = vm->id;
    
    checkpoint_write(f, &header, sizeof(header));
    
    /* Write vCPU states only */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        cpu_state_t *state = &vm->vcpus[i]->cpu_state;
        checkpoint_write(f, state, sizeof(cpu_state_t));
    }
    
    checkpoint_close(f);
    
    return NEXUSVM_OK;
}

nexusvm_result_t checkpoint_apply_incremental(nexusvm_t *vm,
                                               const char *base_checkpoint,
                                               const char *incremental)
{
    if (!vm || !base_checkpoint || !incremental) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Applying incremental checkpoint\n");
    
    /* First restore from base */
    nexusvm_t *temp_vm;
    nexusvm_result_t result = checkpoint_restore(base_checkpoint, &temp_vm);
    if (result != NEXUSVM_OK) {
        return result;
    }
    
    /* Open incremental */
    FILE *f = checkpoint_open(incremental, "rb");
    if (!f) {
        vm_destroy(temp_vm);
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Skip header */
    fseek(f, sizeof(checkpoint_header_t), SEEK_SET);
    
    /* Apply vCPU state changes */
    for (u32 i = 0; i < temp_vm->vcpu_count; i++) {
        cpu_state_t state;
        checkpoint_read(f, &state, sizeof(state));
        
        if (i < vm->vcpu_count) {
            vcpu_set_state(vm->vcpus[i], &state);
        }
    }
    
    checkpoint_close(f);
    vm_destroy(temp_vm);
    
    return NEXUSVM_OK;
}

/*============================================================================
 * Memory Checkpoint
 *============================================================================*/

nexusvm_result_t checkpoint_save_memory(nexusvm_t *vm, const char *path)
{
    if (!vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("Saving VM memory (%lu MB)\n", 
                      vm->memory_size / (1024 * 1024));
    
    /* Open checkpoint file in append mode */
    FILE *f = checkpoint_open(path, "ab");
    if (!f) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Write memory regions */
    for (u32 i = 0; i < vm->memory_region_count; i++) {
        memory_region_t *region = &vm->memory_regions[i];
        
        /* Write region header */
        checkpoint_write(f, region, sizeof(memory_region_t));
        
        /* Write region data */
        if (region->type == MEM_REGION_RAM && region->size > 0) {
            phys_addr_t host_phys = vm_gpa_to_hpa(vm, region->guest_phys);
            if (host_phys != INVALID_PHYS_ADDR) {
                /* Get host virtual address (would use phys_to_virt in real kernel) */
                void *host_virt = (void *)host_phys;
                checkpoint_write(f, host_virt, region->size);
            }
        }
    }
    
    checkpoint_close(f);
    
    return NEXUSVM_OK;
}

nexusvm_result_t checkpoint_restore_memory(nexusvm_t *vm, const char *path)
{
    if (!vm || !path) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_DEBUG("Restoring VM memory\n");
    
    /* Open checkpoint file */
    FILE *f = checkpoint_open(path, "rb");
    if (!f) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Skip header */
    fseek(f, sizeof(checkpoint_header_t), SEEK_SET);
    
    /* Skip config */
    fseek(f, sizeof(vm_config_t), SEEK_SET);
    
    /* Skip vCPU states */
    fseek(f, sizeof(cpu_state_t) * vm->vcpu_count, SEEK_CUR);
    
    /* Skip device state size + device state */
    u64 device_size;
    fread(&device_size, 1, sizeof(device_size), f);
    fseek(f, device_size, SEEK_CUR);
    
    /* Read memory regions */
    for (u32 i = 0; i < vm->memory_region_count; i++) {
        memory_region_t region;
        fread(&region, 1, sizeof(region), f);
        
        if (region.type == MEM_REGION_RAM && region.size > 0) {
            phys_addr_t host_phys = vm_gpa_to_hpa(vm, region.guest_phys);
            if (host_phys != INVALID_PHYS_ADDR) {
                void *host_virt = (void *)host_phys;
                fread(host_virt, 1, region.size, f);
            }
        }
    }
    
    checkpoint_close(f);
    
    return NEXUSVM_OK;
}

/*============================================================================
 * Device State
 *============================================================================*/

nexusvm_result_t checkpoint_save_device_state(nexusvm_t *vm, void **buffer, u64 *size)
{
    if (!vm || !buffer || !size) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Calculate device state size */
    u64 total_size = sizeof(u32); /* Device count */
    total_size += sizeof(pci_config_space_t) * vm->assigned_device_count;
    
    *buffer = malloc(total_size);
    if (!*buffer) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    u8 *ptr = (u8 *)*buffer;
    
    /* Write device count */
    *(u32 *)ptr = vm->assigned_device_count;
    ptr += sizeof(u32);
    
    /* Write PCI device states */
    struct pci_device *dev = vm->assigned_devices;
    while (dev) {
        memcpy(ptr, &dev->config, sizeof(pci_config_space_t));
        ptr += sizeof(pci_config_space_t);
        dev = dev->next;
    }
    
    *size = total_size;
    
    return NEXUSVM_OK;
}

nexusvm_result_t checkpoint_restore_device_state(nexusvm_t *vm, void *buffer, u64 size)
{
    if (!vm || !buffer) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    u8 *ptr = (u8 *)buffer;
    
    /* Read device count */
    u32 device_count = *(u32 *)ptr;
    ptr += sizeof(u32);
    
    /* Restore PCI device states */
    for (u32 i = 0; i < device_count && i < vm->assigned_device_count; i++) {
        pci_config_space_t *config = (pci_config_space_t *)ptr;
        ptr += sizeof(pci_config_space_t);
        
        /* Would restore device state here */
    }
    
    return NEXUSVM_OK;
}
