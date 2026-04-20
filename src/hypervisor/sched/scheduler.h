/**
 * NexusVM Scheduler Implementation
 */

#ifndef NEXUSVM_SCHEDULER_H
#define NEXUSVM_SCHEDULER_H

#include <hypervisor/types.h>
#include <hypervisor/hypervisor.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Scheduler Initialization
 *============================================================================*/

/**
 * Initialize the scheduler
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t scheduler_init(void);

/**
 * Shutdown the scheduler
 */
void scheduler_shutdown(void);

/*============================================================================
 * Scheduler Operations
 *============================================================================*/

/**
 * Add a vCPU to the scheduler
 * @param vcpu vCPU to add
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t scheduler_add_vcpu(nexusvcpu_t *vcpu);

/**
 * Remove a vCPU from the scheduler
 * @param vcpu vCPU to remove
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t scheduler_remove_vcpu(nexusvcpu_t *vcpu);

/**
 * Schedule next vCPU to run
 * @return Next vCPU to run, or NULL if idle
 */
nexusvcpu_t *scheduler_schedule(void);

/**
 * Yield current vCPU
 * @param vcpu Current vCPU
 */
void scheduler_yield(nexusvcpu_t *vcpu);

/**
 * Wake up a vCPU
 * @param vcpu vCPU to wake
 */
void scheduler_wake(nexusvcpu_t *vcpu);

/**
 * Set vCPU priority
 * @param vcpu vCPU
 * @param priority New priority
 */
void scheduler_set_priority(nexusvcpu_t *vcpu, vm_priority_t priority);

/**
 * Set scheduler policy for a vCPU
 * @param vcpu vCPU
 * @param policy New policy
 */
void scheduler_set_policy(nexusvcpu_t *vcpu, sched_policy_t policy);

/*============================================================================
 * CPU Load Balancing
 *============================================================================*/

/**
 * Load balance across CPUs
 * @param cpu Target CPU
 */
void scheduler_load_balance(u32 cpu);

/**
 * Get CPU load
 * @param cpu CPU index
 * @return Load percentage (0-100)
 */
u32 scheduler_get_cpu_load(u32 cpu);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Get scheduler statistics
 * @param cpu_stats Output array for per-CPU stats
 * @param count Number of CPUs
 */
void scheduler_get_stats(cpu_stats_t *cpu_stats, u32 count);

#ifdef __cplusplus
}
#endif

#endif /* NEXUSVM_SCHEDULER_H */
