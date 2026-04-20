/**
 * NexusVM Scheduler Implementation
 */

#include <hypervisor/sched/scheduler.h>
#include <hypervisor/hypervisor.h>
#include <utils/log.h>
#include <utils/spinlock.h>
#include <utils/list.h>
#include <string.h>
#include <stdlib.h>

/* Scheduler constants */
#define SCHED_TICK_NS       10000000   /* 10ms tick */
#define SCHED_MIN_TIMESLICE 1000000    /* 1ms minimum */
#define SCHED_MAX_TIMESLICE 100000000  /* 100ms maximum */

/* Per-CPU runqueue */
typedef struct cpu_runqueue {
    nexusvcpu_t *current;
    struct list_head runqueue;
    u32 cpu_id;
    u64 idle_time;
    u64 busy_time;
    spinlock_t lock;
} cpu_runqueue_t;

static cpu_runqueue_t *g_runqueues = NULL;
static u32 g_cpu_count = 0;
static bool g_scheduler_initialized = false;

/*============================================================================
 * Scheduler Initialization
 *============================================================================*/

nexusvm_result_t scheduler_init(void)
{
    NEXUSVM_LOG_INFO("Initializing scheduler\n");
    
    hypervisor_t *hv = hypervisor_get_instance();
    if (!hv) {
        return NEXUSVM_ERR_GENERIC;
    }
    
    g_cpu_count = hv->online_cpu_count;
    
    /* Allocate runqueues */
    g_runqueues = (cpu_runqueue_t *)calloc(g_cpu_count, sizeof(cpu_runqueue_t));
    if (!g_runqueues) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    /* Initialize each runqueue */
    for (u32 i = 0; i < g_cpu_count; i++) {
        g_runqueues[i].cpu_id = i;
        g_runqueues[i].current = NULL;
        INIT_LIST_HEAD(&g_runqueues[i].runqueue);
        spinlock_init(&g_runqueues[i].lock);
    }
    
    g_scheduler_initialized = true;
    
    NEXUSVM_LOG_INFO("Scheduler initialized with %u CPUs\n", g_cpu_count);
    
    return NEXUSVM_OK;
}

void scheduler_shutdown(void)
{
    if (g_runqueues) {
        free(g_runqueues);
        g_runqueues = NULL;
    }
    
    g_scheduler_initialized = false;
    NEXUSVM_LOG_INFO("Scheduler shutdown\n");
}

/*============================================================================
 * Scheduler Operations
 *============================================================================*/

nexusvm_result_t scheduler_add_vcpu(nexusvcpu_t *vcpu)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    u32 cpu = vcpu->physical_cpu;
    
    if (cpu >= g_cpu_count) {
        cpu = vcpu->vcpu_id % g_cpu_count;
    }
    
    spinlock_lock(&g_runqueues[cpu].lock);
    
    /* Create scheduling entity if not exists */
    if (!vcpu->sched_entity) {
        vcpu->sched_entity = (sched_entity_t *)calloc(1, sizeof(sched_entity_t));
        if (!vcpu->sched_entity) {
            spinlock_unlock(&g_runqueues[cpu].lock);
            return NEXUSVM_ERR_NO_MEMORY;
        }
        
        vcpu->sched_entity->vcpu_id = vcpu->vcpu_id;
        vcpu->sched_entity->vm_id = vcpu->vm->id;
        vcpu->sched_entity->policy = SCHED_CFS;
        vcpu->sched_entity->priority = PRIORITY_NORMAL;
        vcpu->sched_entity->weight = 1024; /* Default weight */
        vcpu->sched_entity->runtime = 0;
        vcpu->sched_entity->vruntime = 0;
    }
    
    /* Add to runqueue */
    list_add_tail(&vcpu->sched_entity->next, &g_runqueues[cpu].runqueue);
    
    spinlock_unlock(&g_runqueues[cpu].lock);
    
    NEXUSVM_LOG_DEBUG("vCPU %u added to CPU %u runqueue\n", vcpu->vcpu_id, cpu);
    
    return NEXUSVM_OK;
}

nexusvm_result_t scheduler_remove_vcpu(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->sched_entity) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    u32 cpu = vcpu->physical_cpu;
    if (cpu >= g_cpu_count) {
        cpu = vcpu->vcpu_id % g_cpu_count;
    }
    
    spinlock_lock(&g_runqueues[cpu].lock);
    
    list_del(&vcpu->sched_entity->next);
    
    spinlock_unlock(&g_runqueues[cpu].lock);
    
    return NEXUSVM_OK;
}

/*============================================================================
 * Scheduling Algorithms
 *============================================================================*/

/**
 * CFS (Completely Fair Scheduler) pick next
 */
static nexusvcpu_t *scheduler_cfs_pick_next(u32 cpu)
{
    cpu_runqueue_t *rq = &g_runqueues[cpu];
    struct list_head *pos;
    sched_entity_t *best = NULL;
    u64 min_vruntime = ~0ULL;
    
    list_for_each(pos, &rq->runqueue) {
        sched_entity_t *se = list_entry(pos, sched_entity_t, next);
        if (se->vruntime < min_vruntime) {
            min_vruntime = se->vruntime;
            best = se;
        }
    }
    
    if (!best) return NULL;
    
    /* Find vCPU for this entity */
    nexusvm_t *vm = hypervisor_get_instance()->vms;
    while (vm) {
        for (u32 i = 0; i < vm->vcpu_count; i++) {
            nexusvcpu_t *vcpu = vm->vcpus[i];
            if (vcpu && vcpu->sched_entity == best) {
                return vcpu;
            }
        }
        vm = vm->next;
    }
    
    return NULL;
}

/**
 * Round-robin pick next
 */
static nexusvcpu_t *scheduler_rr_pick_next(u32 cpu)
{
    cpu_runqueue_t *rq = &g_runqueues[cpu];
    
    if (list_empty(&rq->runqueue)) {
        return NULL;
    }
    
    sched_entity_t *se = list_entry(rq->runqueue.next, sched_entity_t, next);
    
    /* Find vCPU */
    nexusvm_t *vm = hypervisor_get_instance()->vms;
    while (vm) {
        for (u32 i = 0; i < vm->vcpu_count; i++) {
            nexusvcpu_t *vcpu = vm->vcpus[i];
            if (vcpu && vcpu->sched_entity == se) {
                return vcpu;
            }
        }
        vm = vm->next;
    }
    
    return NULL;
}

/**
 * Real-time FIFO pick next
 */
static nexusvcpu_t *scheduler_fifo_pick_next(u32 cpu)
{
    cpu_runqueue_t *rq = &g_runqueues[cpu];
    struct list_head *pos;
    sched_entity_t *earliest = NULL;
    u64 min_deadline = ~0ULL;
    
    list_for_each(pos, &rq->runqueue) {
        sched_entity_t *se = list_entry(pos, sched_entity_t, next);
        if (se->policy == SCHED_FIFO && se->deadline < min_deadline) {
            min_deadline = se->deadline;
            earliest = se;
        }
    }
    
    if (!earliest) {
        return scheduler_cfs_pick_next(cpu);
    }
    
    /* Find vCPU */
    nexusvm_t *vm = hypervisor_get_instance()->vms;
    while (vm) {
        for (u32 i = 0; i < vm->vcpu_count; i++) {
            nexusvcpu_t *vcpu = vm->vcpus[i];
            if (vcpu && vcpu->sched_entity == earliest) {
                return vcpu;
            }
        }
        vm = vm->next;
    }
    
    return NULL;
}

nexusvcpu_t *scheduler_schedule(void)
{
    if (!g_scheduler_initialized || g_cpu_count == 0) {
        return NULL;
    }
    
    /* Get current CPU */
    u32 cpu = 0; /* Would be smp_processor_id() in real kernel */
    
    cpu_runqueue_t *rq = &g_runqueues[cpu];
    
    spinlock_lock(&rq->lock);
    
    /* Pick next vCPU based on policy */
    nexusvcpu_t *next = NULL;
    
    if (rq->current && rq->current->sched_entity) {
        sched_entity_t *current_se = rq->current->sched_entity;
        
        /* Update virtual time */
        current_se->runtime += SCHED_TICK_NS;
        current_se->vruntime += SCHED_TICK_NS * 1024 / current_se->weight;
        
        /* Check if current vCPU should continue */
        if (rq->current->running && 
            current_se->vruntime < SCHED_MAX_TIMESLICE) {
            spinlock_unlock(&rq->lock);
            return rq->current;
        }
        
        /* Move to end of queue */
        list_del(&current_se->next);
        list_add_tail(&current_se->next, &rq->runqueue);
    }
    
    /* Pick next based on scheduling policy */
    sched_entity_t *first = list_entry(rq->runqueue.next, sched_entity_t, next);
    if (first) {
        switch (first->policy) {
            case SCHED_FIFO:
            case SCHED_RR:
                next = scheduler_fifo_pick_next(cpu);
                break;
            case SCHED_CFS:
            default:
                next = scheduler_cfs_pick_next(cpu);
                break;
        }
    }
    
    spinlock_unlock(&rq->lock);
    
    return next;
}

void scheduler_yield(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->sched_entity) return;
    
    /* Move to end of runqueue */
    u32 cpu = vcpu->physical_cpu;
    if (cpu >= g_cpu_count) cpu = vcpu->vcpu_id % g_cpu_count;
    
    cpu_runqueue_t *rq = &g_runqueues[cpu];
    
    spinlock_lock(&rq->lock);
    list_del(&vcpu->sched_entity->next);
    list_add_tail(&vcpu->sched_entity->next, &rq->runqueue);
    spinlock_unlock(&rq->lock);
}

void scheduler_wake(nexusvcpu_t *vcpu)
{
    if (!vcpu || !vcpu->sched_entity) return;
    
    u32 cpu = vcpu->physical_cpu;
    if (cpu >= g_cpu_count) cpu = vcpu->vcpu_id % g_cpu_count;
    
    cpu_runqueue_t *rq = &g_runqueues[cpu];
    
    spinlock_lock(&rq->lock);
    
    /* Remove from current position and add to front */
    list_del(&vcpu->sched_entity->next);
    list_add(&vcpu->sched_entity->next, &rq->runqueue);
    
    spinlock_unlock(&rq->lock);
}

void scheduler_set_priority(nexusvcpu_t *vcpu, vm_priority_t priority)
{
    if (!vcpu || !vcpu->sched_entity) return;
    
    vcpu->sched_entity->priority = priority;
    
    /* Adjust weight based on priority */
    switch (priority) {
        case PRIORITY_REALTIME:
            vcpu->sched_entity->weight = 4096;
            break;
        case PRIORITY_HIGH:
            vcpu->sched_entity->weight = 2048;
            break;
        case PRIORITY_NORMAL:
            vcpu->sched_entity->weight = 1024;
            break;
        case PRIORITY_LOW:
        default:
            vcpu->sched_entity->weight = 512;
            break;
    }
}

void scheduler_set_policy(nexusvcpu_t *vcpu, sched_policy_t policy)
{
    if (!vcpu || !vcpu->sched_entity) return;
    vcpu->sched_entity->policy = policy;
}

/*============================================================================
 * CPU Load Balancing
 *============================================================================*/

void scheduler_load_balance(u32 cpu)
{
    if (cpu >= g_cpu_count) return;
    
    cpu_runqueue_t *src_rq = &g_runqueues[cpu];
    cpu_runqueue_t *tgt_rq;
    
    u32 src_load = 0;
    u32 tgt_cpu = 0;
    u32 min_load = ~0U;
    
    /* Find least loaded CPU */
    for (u32 i = 0; i < g_cpu_count; i++) {
        if (i == cpu) continue;
        
        tgt_rq = &g_runqueues[i];
        
        spinlock_lock(&tgt_rq->lock);
        u32 load = 0;
        struct list_head *pos;
        list_for_each(pos, &tgt_rq->runqueue) {
            load++;
        }
        spinlock_unlock(&tgt_rq->lock);
        
        if (load < min_load) {
            min_load = load;
            tgt_cpu = i;
        }
    }
    
    spinlock_lock(&src_rq->lock);
    src_load = 0;
    struct list_head *pos;
    list_for_each(pos, &src_rq->runqueue) {
        src_load++;
    }
    spinlock_unlock(&src_rq->lock);
    
    /* If load imbalance is significant, migrate */
    if (src_load > min_load + 2) {
        spinlock_lock(&src_rq->lock);
        if (!list_empty(&src_rq->runqueue)) {
            sched_entity_t *se = list_entry(src_rq->runqueue.next, 
                                             sched_entity_t, next);
            list_del(&se->next);
            spinlock_unlock(&src_rq->lock);
            
            tgt_rq = &g_runqueues[tgt_cpu];
            spinlock_lock(&tgt_rq->lock);
            list_add_tail(&se->next, &tgt_rq->runqueue);
            spinlock_unlock(&tgt_rq->lock);
            
            NEXUSVM_LOG_DEBUG("Migrated entity from CPU %u to CPU %u\n",
                              cpu, tgt_cpu);
        } else {
            spinlock_unlock(&src_rq->lock);
        }
    }
}

u32 scheduler_get_cpu_load(u32 cpu)
{
    if (cpu >= g_cpu_count) return 0;
    
    cpu_runqueue_t *rq = &g_runqueues[cpu];
    
    spinlock_lock(&rq->lock);
    u32 count = 0;
    struct list_head *pos;
    list_for_each(pos, &rq->runqueue) {
        count++;
    }
    spinlock_unlock(&rq->lock);
    
    /* Return as percentage */
    return (count * 100) / (g_cpu_count + 1);
}

void scheduler_get_stats(cpu_stats_t *cpu_stats, u32 count)
{
    if (!cpu_stats || count == 0) return;
    
    for (u32 i = 0; i < MIN(count, g_cpu_count); i++) {
        cpu_runqueue_t *rq = &g_runqueues[i];
        
        spinlock_lock(&rq->lock);
        cpu_stats[i].idle_time = rq->idle_time;
        cpu_stats[i].busy_time = rq->busy_time;
        
        u32 queue_len = 0;
        struct list_head *pos;
        list_for_each(pos, &rq->runqueue) {
            queue_len++;
        }
        
        /* Context switch estimate */
        cpu_stats[i].context_switches = rq->busy_time / SCHED_TICK_NS;
        spinlock_unlock(&rq->lock);
    }
}
