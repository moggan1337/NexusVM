/**
 * NexusVM Live Migration Implementation
 */

#include <hypervisor/migration/migration.h>
#include <hypervisor/hypervisor.h>
#include <hypervisor/checkpoint/checkpoint.h>
#include <utils/log.h>
#include <utils/spinlock.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* Migration state structure */
typedef struct migration_session {
    nexusvm_t *vm;
    migration_config_t config;
    migration_state_t state;
    
    /* Connection */
    void *connection;
    bool is_source;
    
    /* Page tracking */
    u64 *dirty_pages;
    u64 dirty_page_count;
    u64 total_pages;
    u64 transferred_pages;
    u64 iteration;
    
    /* Statistics */
    migration_stats_t stats;
    
    /* Thread handle (would be pthread in real implementation) */
    void *thread;
    
    /* Lock */
    spinlock_t lock;
} migration_session_t;

/*============================================================================
 * Migration Session Management
 *============================================================================*/

nexusvm_result_t migration_start(nexusvm_t *vm, migration_config_t *config, 
                                  void **session_out)
{
    if (!vm || !config || !session_out) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Starting live migration of VM %s\n", vm->name);
    
    migration_session_t *session = (migration_session_t *)
        calloc(1, sizeof(migration_session_t));
    if (!session) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    session->vm = vm;
    session->config = *config;
    session->state = MIGRATION_STATE_INITIATED;
    session->is_source = true;
    
    /* Calculate total pages */
    session->total_pages = vm->memory_size / PAGE_SIZE;
    
    /* Allocate dirty page tracking bitmap */
    session->dirty_pages = (u64 *)calloc(
        (session->total_pages + 63) / 64, sizeof(u64));
    if (!session->dirty_pages) {
        free(session);
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Mark all pages dirty for initial copy */
    for (u64 i = 0; i < session->total_pages; i++) {
        session->dirty_pages[i / 64] |= (1ULL << (i % 64));
    }
    
    session->dirty_page_count = session->total_pages;
    
    spinlock_init(&session->lock);
    
    /* Update VM state */
    vm->state = VM_STATE_MIGRATING;
    
    *session_out = session;
    
    NEXUSVM_LOG_INFO("Migration session created: %lu pages to transfer\n",
                     session->total_pages);
    
    return NEXUSVM_OK;
}

nexusvm_result_t migration_complete(void *session)
{
    if (!session) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    migration_session_t *s = (migration_session_t *)session;
    
    spinlock_lock(&s->lock);
    s->state = MIGRATION_STATE_COMPLETED;
    spinlock_unlock(&s->lock);
    
    /* Resume VM if it was paused */
    if (s->vm->state == VM_STATE_MIGRATING) {
        s->vm->state = VM_STATE_RUNNING;
    }
    
    /* Clean up */
    if (s->dirty_pages) {
        free(s->dirty_pages);
    }
    
    NEXUSVM_LOG_INFO("Migration completed in %lu iterations\n", s->iteration);
    
    free(s);
    return NEXUSVM_OK;
}

nexusvm_result_t migration_cancel(void *session)
{
    if (!session) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    migration_session_t *s = (migration_session_t *)session;
    
    spinlock_lock(&s->lock);
    s->state = MIGRATION_STATE_CANCELLED;
    spinlock_unlock(&s->lock);
    
    /* Resume VM */
    if (s->vm->state == VM_STATE_MIGRATING) {
        s->vm->state = VM_STATE_RUNNING;
    }
    
    /* Clean up */
    if (s->connection) {
        migration_disconnect(s->connection);
    }
    if (s->dirty_pages) {
        free(s->dirty_pages);
    }
    
    NEXUSVM_LOG_INFO("Migration cancelled\n");
    
    free(s);
    return NEXUSVM_OK;
}

/*============================================================================
 * Migration Phases
 *============================================================================*/

nexusvm_result_t migration_precopy(void *session)
{
    if (!session) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    migration_session_t *s = (migration_session_t *)session;
    
    spinlock_lock(&s->lock);
    s->state = MIGRATION_STATE_PRECOPY;
    spinlock_unlock(&s->lock);
    
    NEXUSVM_LOG_INFO("Starting precopy phase, iteration %lu\n", s->iteration);
    
    /* Copy dirty pages */
    u64 pages_this_iteration = 0;
    u64 bytes_this_iteration = 0;
    
    for (u64 i = 0; i < s->total_pages && pages_this_iteration < 10000; i++) {
        if (s->dirty_pages[i / 64] & (1ULL << (i % 64))) {
            /* This page is dirty, would copy it here */
            pages_this_iteration++;
            bytes_this_iteration += PAGE_SIZE;
            
            /* Clear dirty bit */
            s->dirty_pages[i / 64] &= ~(1ULL << (i % 64));
            s->transferred_pages++;
            s->dirty_page_count--;
        }
    }
    
    s->iteration++;
    
    /* Update statistics */
    s->stats.dirty_pages = s->dirty_page_count;
    s->stats.transferred_pages = s->transferred_pages;
    s->stats.remaining_pages = s->dirty_page_count;
    s->stats.progress_percent = 
        (double)(s->total_pages - s->dirty_page_count) / s->total_pages * 100.0;
    
    NEXUSVM_LOG_INFO("Precopy iteration %lu: %lu pages remaining, %.1f%% complete\n",
                     s->iteration, s->dirty_page_count, s->stats.progress_percent);
    
    /* Check if converged */
    if (s->dirty_page_count < 100 || s->iteration > s->config.max_convergence_iterations) {
        spinlock_lock(&s->lock);
        s->state = MIGRATION_STATE_STOP_AND_COPY;
        spinlock_unlock(&s->lock);
        return NEXUSVM_OK;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t migration_stop_and_copy(void *session)
{
    if (!session) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    migration_session_t *s = (migration_session_t *)session;
    
    spinlock_lock(&s->lock);
    s->state = MIGRATION_STATE_STOP_AND_COPY;
    spinlock_unlock(&s->lock);
    
    NEXUSVM_LOG_INFO("Starting stop-and-copy phase\n");
    
    /* Pause VM */
    vm_pause(s->vm);
    
    /* Copy remaining pages */
    u64 bytes_copied = 0;
    for (u64 i = 0; i < s->total_pages; i++) {
        if (s->dirty_pages[i / 64] & (1ULL << (i % 64))) {
            /* Copy page */
            bytes_copied += PAGE_SIZE;
            s->transferred_pages++;
        }
    }
    
    /* Send final VM state */
    nexusvm_result_t result = migration_send_vm_state(s->connection, s->vm);
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to send VM state: %d\n", result);
        return result;
    }
    
    /* Update state */
    spinlock_lock(&s->lock);
    s->state = MIGRATION_STATE_TRANSFER;
    spinlock_unlock(&s->lock);
    
    NEXUSVM_LOG_INFO("Stop-and-copy complete: %lu bytes transferred\n", bytes_copied);
    
    return NEXUSVM_OK;
}

/*============================================================================
 * Page Transfer
 *============================================================================*/

void migration_track_dirty_page(void *session, u64 page_frame)
{
    if (!session || page_frame >= ((migration_session_t *)session)->total_pages) {
        return;
    }
    
    migration_session_t *s = (migration_session_t *)session;
    
    spinlock_lock(&s->lock);
    if (!(s->dirty_pages[page_frame / 64] & (1ULL << (page_frame % 64)))) {
        s->dirty_pages[page_frame / 64] |= (1ULL << (page_frame % 64));
        s->dirty_page_count++;
    }
    spinlock_unlock(&s->lock);
}

void migration_get_stats(void *session, migration_stats_t *stats)
{
    if (!session || !stats) return;
    
    migration_session_t *s = (migration_session_t *)session;
    memcpy(stats, &s->stats, sizeof(migration_stats_t));
}

/*============================================================================
 * Network Transport (Stub Implementation)
 *============================================================================*/

nexusvm_result_t migration_connect(const char *host, u16 port, void **connection)
{
    if (!host || !connection) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    NEXUSVM_LOG_INFO("Connecting to %s:%u for migration\n", host, port);
    
    /* In a real implementation, this would use TCP sockets */
    /* For now, return a placeholder */
    *connection = NULL;
    
    return NEXUSVM_OK;
}

void migration_disconnect(void *connection)
{
    if (!connection) return;
    
    NEXUSVM_LOG_INFO("Disconnecting migration connection\n");
}

nexusvm_result_t migration_send(void *connection, const void *data, u64 size)
{
    if (!data || size == 0) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Stub: would send data over network */
    return NEXUSVM_OK;
}

nexusvm_result_t migration_receive(void *connection, void *data, u64 size, u64 *received)
{
    if (!data || !received) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Stub: would receive data from network */
    *received = 0;
    return NEXUSVM_OK;
}

/*============================================================================
 * Memory Transfer
 *============================================================================*/

nexusvm_result_t migration_send_pages(void *connection, const void *pages, 
                                       u64 count, bool compressed)
{
    if (!pages || count == 0) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Compress if requested */
    if (compressed) {
        NEXUSVM_LOG_DEBUG("Compressing %lu pages\n", count);
    }
    
    /* Send page data */
    return migration_send(connection, pages, count * PAGE_SIZE);
}

nexusvm_result_t migration_receive_pages(void *connection, void *pages, u64 count)
{
    if (!pages || count == 0) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    u64 received;
    return migration_receive(connection, pages, count * PAGE_SIZE, &received);
}

/*============================================================================
 * VM State Transfer
 *============================================================================*/

nexusvm_result_t migration_serialize_vm_state(nexusvm_t *vm, void **buffer, u64 *size)
{
    if (!vm || !buffer || !size) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Calculate state size */
    u64 state_size = sizeof(vm_config_t) +
                     sizeof(cpu_state_t) * vm->vcpu_count +
                     sizeof(sched_entity_t) * vm->vcpu_count;
    
    *buffer = malloc(state_size);
    if (!*buffer) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    u8 *ptr = (u8 *)*buffer;
    
    /* Serialize VM config */
    memcpy(ptr, &vm->config, sizeof(vm_config_t));
    ptr += sizeof(vm_config_t);
    
    /* Serialize vCPU states */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        memcpy(ptr, &vm->vcpus[i]->cpu_state, sizeof(cpu_state_t));
        ptr += sizeof(cpu_state_t);
    }
    
    *size = state_size;
    
    NEXUSVM_LOG_DEBUG("Serialized VM state: %lu bytes\n", state_size);
    
    return NEXUSVM_OK;
}

nexusvm_result_t migration_deserialize_vm_state(nexusvm_t *vm, const void *buffer, u64 size)
{
    if (!vm || !buffer) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    const u8 *ptr = (const u8 *)buffer;
    
    /* Deserialize VM config */
    memcpy(&vm->config, ptr, sizeof(vm_config_t));
    ptr += sizeof(vm_config_t);
    
    /* Deserialize vCPU states */
    for (u32 i = 0; i < vm->vcpu_count; i++) {
        memcpy(&vm->vcpus[i]->cpu_state, ptr, sizeof(cpu_state_t));
        ptr += sizeof(cpu_state_t);
    }
    
    NEXUSVM_LOG_DEBUG("Deserialized VM state\n");
    
    return NEXUSVM_OK;
}

nexusvm_result_t migration_send_vm_state(void *connection, nexusvm_t *vm)
{
    if (!connection || !vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    void *buffer;
    u64 size;
    
    nexusvm_result_t result = migration_serialize_vm_state(vm, &buffer, &size);
    if (result != NEXUSVM_OK) {
        return result;
    }
    
    /* Send header */
    migration_header_t header;
    header.magic = NEXUSVM_MIGRATION_MAGIC;
    header.version = NEXUSVM_MIGRATION_VERSION;
    header.type = NEXUSVM_MIGRATION_TYPE_STATE;
    header.size = size;
    header.checksum = 0; /* Would calculate CRC32 */
    
    result = migration_send(connection, &header, sizeof(header));
    if (result != NEXUSVM_OK) {
        free(buffer);
        return result;
    }
    
    /* Send state data */
    result = migration_send(connection, buffer, size);
    free(buffer);
    
    return result;
}

nexusvm_result_t migration_receive_vm_state(void *connection, nexusvm_t *vm)
{
    if (!connection || !vm) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Receive header */
    migration_header_t header;
    u64 received;
    
    nexusvm_result_t result = migration_receive(connection, &header, 
                                                 sizeof(header), &received);
    if (result != NEXUSVM_OK) {
        return result;
    }
    
    if (header.magic != NEXUSVM_MIGRATION_MAGIC ||
        header.version != NEXUSVM_MIGRATION_VERSION ||
        header.type != NEXUSVM_MIGRATION_TYPE_STATE) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    /* Receive state data */
    void *buffer = malloc(header.size);
    if (!buffer) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    result = migration_receive(connection, buffer, header.size, &received);
    if (result != NEXUSVM_OK) {
        free(buffer);
        return result;
    }
    
    result = migration_deserialize_vm_state(vm, buffer, header.size);
    free(buffer);
    
    return result;
}
