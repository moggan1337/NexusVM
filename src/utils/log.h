/**
 * NexusVM Utility Functions
 */

#ifndef NEXUSVM_UTILS_H
#define NEXUSVM_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*============================================================================
 * Logging
 *============================================================================*/

#define NEXUSVM_LOG_DEBUG(fmt, ...) \
    nexusvm_log(NEXUSVM_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define NEXUSVM_LOG_INFO(fmt, ...) \
    nexusvm_log(NEXUSVM_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define NEXUSVM_LOG_WARN(fmt, ...) \
    nexusvm_log(NEXUSVM_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define NEXUSVM_LOG_ERROR(fmt, ...) \
    nexusvm_log(NEXUSVM_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

typedef enum {
    NEXUSVM_LOG_LEVEL_DEBUG = 0,
    NEXUSVM_LOG_LEVEL_INFO,
    NEXUSVM_LOG_LEVEL_WARN,
    NEXUSVM_LOG_LEVEL_ERROR,
} nexusvm_log_level_t;

void nexusvm_log(nexusvm_log_level_t level, const char *file, int line, 
                 const char *fmt, ...);

/*============================================================================
 * Spinlock
 *============================================================================*/

typedef struct spinlock {
    volatile int locked;
} spinlock_t;

void spinlock_init(spinlock_t *lock);
void spinlock_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);
bool spinlock_trylock(spinlock_t *lock);

/*============================================================================
 * Linked List
 *============================================================================*/

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

#define LIST_HEAD(name) \
    struct list_head name = { &(name), &(name) }

#define INIT_LIST_HEAD(head) \
    do { \
        (head)->next = (head); \
        (head)->prev = (head); \
    } while (0)

#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

#define list_add(new, head) \
    do { \
        (new)->next = (head)->next; \
        (new)->prev = (head); \
        (head)->next->prev = (new); \
        (head)->next = (new); \
    } while (0)

#define list_add_tail(new, head) \
    do { \
        (new)->next = (head); \
        (new)->prev = (head)->prev; \
        (head)->prev->next = (new); \
        (head)->prev = (new); \
    } while (0)

#define list_del(entry) \
    do { \
        (entry)->prev->next = (entry)->next; \
        (entry)->next->prev = (entry)->prev; \
    } while (0)

#define list_empty(head) ((head)->next == (head))

/*============================================================================
 * CRC32
 *============================================================================*/

uint32_t crc32(const void *data, size_t len);

/*============================================================================
 * String Utilities
 *============================================================================*/

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
char *strncpy(char *dest, const char *src, size_t n);
char *strcpy(char *dest, const char *src);

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);

/*============================================================================
 * Bit Operations
 *============================================================================*/

#define BITS_PER_LONG 64

#define BIT(nr) (1UL << (nr))
#define BIT_MASK(nr) (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr) ((nr) / BITS_PER_LONG)

#define test_bit(nr, addr) (((1UL << ((nr) % BITS_PER_LONG)) & \
                           ((addr)[BIT_WORD(nr)])) != 0)
#define set_bit(nr, addr) ((addr)[BIT_WORD(nr)] |= BIT_MASK(nr))
#define clear_bit(nr, addr) ((addr)[BIT_WORD(nr)] &= ~BIT_MASK(nr))
#define change_bit(nr, addr) ((addr)[BIT_WORD(nr)] ^= BIT_MASK(nr))

#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)
unsigned long find_next_bit(const unsigned long *addr, unsigned long size, 
                             unsigned long offset);

/*============================================================================
 * Alignment
 *============================================================================*/

#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

#define PAGE_ALIGN(x) ALIGN(x, 4096)
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/*============================================================================
 * Min/Max
 *============================================================================*/

#define MIN(a, b) ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a < _b ? _a : _b; \
})

#define MAX(a, b) ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a > _b ? _a : _b; \
})

#define CLAMP(val, lo, hi) ({ \
    typeof(val) _val = (val); \
    typeof(lo) _lo = (lo); \
    typeof(hi) _hi = (hi); \
    _val < _lo ? _lo : (_val > _hi ? _hi : _val); \
})

/*============================================================================
 * Container Of
 *============================================================================*/

#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/*============================================================================
 * Read/Write Barriers
 *============================================================================*/

#define mb() __asm__ __volatile__("" ::: "memory")
#define rmb() mb()
#define wmb() mb()

/*============================================================================
 * Assertions
 *============================================================================*/

#ifdef NEXUSVM_DEBUG
#define BUG() do { \
    NEXUSVM_LOG_ERROR("BUG at %s:%d\n", __FILE__, __LINE__); \
    while(1); \
} while(0)

#define BUG_ON(condition) do { \
    if (condition) BUG(); \
} while(0)

#define WARN_ON(condition) do { \
    if (condition) \
        NEXUSVM_LOG_WARN("WARNING at %s:%d\n", __FILE__, __LINE__); \
} while(0)
#else
#define BUG() do { } while(0)
#define BUG_ON(condition) do { } while(0)
#define WARN_ON(condition) do { } while(0)
#endif

#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

#endif /* NEXUSVM_UTILS_H */
