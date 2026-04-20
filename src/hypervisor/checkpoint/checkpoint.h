/**
 * NexusVM Checkpoint Implementation
 */

#ifndef NEXUSVM_CHECKPOINT_H
#define NEXUSVM_CHECKPOINT_H

#include <hypervisor/types.h>
#include <hypervisor/hypervisor.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Checkpoint magic and version */
#define NEXUSVM_CHECKPOINT_MAGIC     0x4E5856434B   /* "NXVMCHK" */
#define NEXUSVM_CHECKPOINT_VERSION   1

/*============================================================================
 * Checkpoint Creation
 *============================================================================*/

/**
 * Create a checkpoint of a VM
 * @param vm VM to checkpoint
 * @param path Output file path
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_create(nexusvm_t *vm, const char *path);

/**
 * Create a snapshot (live checkpoint)
 * @param vm VM to snapshot
 * @param path Output file path
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t snapshot_create(nexusvm_t *vm, const char *path);

/*============================================================================
 * Checkpoint Restoration
 *============================================================================*/

/**
 * Restore a VM from checkpoint
 * @param path Checkpoint file path
 * @param vm Output VM pointer
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_restore(const char *path, nexusvm_t **vm);

/**
 * Restore VM state from snapshot
 * @param vm Target VM
 * @param path Snapshot file path
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t snapshot_restore(nexusvm_t *vm, const char *path);

/*============================================================================
 * Checkpoint File Operations
 *============================================================================*/

/**
 * Get checkpoint info
 * @param path Checkpoint file path
 * @param info Output info structure
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_get_info(const char *path, checkpoint_header_t *info);

/**
 * List checkpoints in directory
 * @param dir Directory path
 * @param entries Output entries array
 * @param max_entries Maximum entries
 * @return Number of entries found
 */
u32 checkpoint_list(const char *dir, checkpoint_header_t *entries, u32 max_entries);

/**
 * Delete a checkpoint
 * @param path Checkpoint file path
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_delete(const char *path);

/*============================================================================
 * Incremental Checkpoints
 *============================================================================*/

/**
 * Create incremental checkpoint
 * @param vm VM to checkpoint
 * @param base_checkpoint Base checkpoint path
 * @param path Output path for incremental checkpoint
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_create_incremental(nexusvm_t *vm, 
                                                const char *base_checkpoint,
                                                const char *path);

/**
 * Apply incremental checkpoint
 * @param vm Target VM
 * @param base_checkpoint Base checkpoint path
 * @param incremental Incremental checkpoint path
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_apply_incremental(nexusvm_t *vm,
                                               const char *base_checkpoint,
                                               const char *incremental);

/*============================================================================
 * Memory Checkpoint
 *============================================================================*/

/**
 * Create memory-only checkpoint
 * @param vm VM to checkpoint
 * @param path Output path
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_save_memory(nexusvm_t *vm, const char *path);

/**
 * Restore memory from checkpoint
 * @param vm Target VM
 * @param path Checkpoint path
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_restore_memory(nexusvm_t *vm, const char *path);

/*============================================================================
 * Device State
 *============================================================================*/

/**
 * Serialize device state
 * @param vm VM
 * @param buffer Output buffer
 * @param size Output size
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_save_device_state(nexusvm_t *vm, void **buffer, u64 *size);

/**
 * Restore device state
 * @param vm Target VM
 * @param buffer State buffer
 * @param size Buffer size
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t checkpoint_restore_device_state(nexusvm_t *vm, void *buffer, u64 size);

#ifdef __cplusplus
}
#endif

#endif /* NEXUSVM_CHECKPOINT_H */
