/**
 * NexusVM Live Migration Implementation
 */

#ifndef NEXUSVM_MIGRATION_H
#define NEXUSVM_MIGRATION_H

#include <hypervisor/types.h>
#include <hypervisor/hypervisor.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Migration Session Management
 *============================================================================*/

/**
 * Start a live migration
 * @param vm VM to migrate
 * @param config Migration configuration
 * @param session Output for migration session
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_start(nexusvm_t *vm, migration_config_t *config, 
                                  void **session);

/**
 * Complete migration
 * @param session Migration session
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_complete(void *session);

/**
 * Cancel migration
 * @param session Migration session
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_cancel(void *session);

/*============================================================================
 * Migration Phases
 *============================================================================*/

/**
 * Perform precopy phase
 * @param session Migration session
 * @return NEXUSVM_OK if precopy complete
 */
nexusvm_result_t migration_precopy(void *session);

/**
 * Perform stop-and-copy phase
 * @param session Migration session
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_stop_and_copy(void *session);

/*============================================================================
 * Page Transfer
 *============================================================================*/

/**
 * Track dirty pages
 * @param session Migration session
 * @param page_frame Page frame number
 */
void migration_track_dirty_page(void *session, u64 page_frame);

/**
 * Get migration statistics
 * @param session Migration session
 * @param stats Output statistics
 */
void migration_get_stats(void *session, migration_stats_t *stats);

/*============================================================================
 * Network Transport
 *============================================================================*/

/**
 * Connect to target host
 * @param host Target hostname/IP
 * @param port Target port
 * @param connection Output connection handle
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_connect(const char *host, u16 port, void **connection);

/**
 * Disconnect from target host
 * @param connection Connection handle
 */
void migration_disconnect(void *connection);

/**
 * Send data to target
 * @param connection Connection handle
 * @param data Data buffer
 * @param size Data size
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_send(void *connection, const void *data, u64 size);

/**
 * Receive data from target
 * @param connection Connection handle
 * @param data Data buffer
 * @param size Buffer size
 * @param received Actual received size
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_receive(void *connection, void *data, u64 size, u64 *received);

/*============================================================================
 * Memory Transfer
 *============================================================================*/

/**
 * Send memory pages
 * @param connection Connection handle
 * @param pages Page data
 * @param count Number of pages
 * @param compressed Use compression
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_send_pages(void *connection, const void *pages, 
                                       u64 count, bool compressed);

/**
 * Receive memory pages
 * @param connection Connection handle
 * @param pages Output page buffer
 * @param count Number of pages
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_receive_pages(void *connection, void *pages, u64 count);

/*============================================================================
 * VM State Transfer
 *============================================================================*/

/**
 * Serialize VM state
 * @param vm VM to serialize
 * @param buffer Output buffer
 * @param size Output size
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_serialize_vm_state(nexusvm_t *vm, void **buffer, u64 *size);

/**
 * Deserialize VM state
 * @param vm Target VM
 * @param buffer State buffer
 * @param size Buffer size
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_deserialize_vm_state(nexusvm_t *vm, const void *buffer, u64 size);

/**
 * Send VM state
 * @param connection Connection handle
 * @param vm VM to send
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_send_vm_state(void *connection, nexusvm_t *vm);

/**
 * Receive VM state
 * @param connection Connection handle
 * @param vm Target VM
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t migration_receive_vm_state(void *connection, nexusvm_t *vm);

#ifdef __cplusplus
}
#endif

#endif /* NEXUSVM_MIGRATION_H */
