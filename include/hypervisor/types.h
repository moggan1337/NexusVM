/**
 * NexusVM - Type-1 Hypervisor Core Types
 * 
 * This header defines the fundamental types and structures used throughout
 * the NexusVM hypervisor implementation.
 */

#ifndef NEXUSVM_TYPES_H
#define NEXUSVM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Fundamental Types
 *============================================================================*/

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;

typedef u64         phys_addr_t;    /* Physical address */
typedef u64         virt_addr_t;   /* Virtual address */
typedef u64         guest_addr_t;  /* Guest physical address */

#define PRIx64 "lx"
#define PRIx32 "x"
#define PRIx16 "hx"
#define PRIx8 "hx"

/*============================================================================
 * Result Type for Error Handling
 *============================================================================*/

typedef enum {
    NEXUSVM_OK = 0,
    NEXUSVM_ERR_GENERIC = -1,
    NEXUSVM_ERR_NOT_SUPPORTED = -2,
    NEXUSVM_ERR_INVALID_PARAM = -3,
    NEXUSVM_ERR_NO_MEMORY = -4,
    NEXUSVM_ERR_NOT_FOUND = -5,
    NEXUSVM_ERR_ALREADY_EXISTS = -6,
    NEXUSVM_ERR_VMX_FAILED = -7,
    NEXUSVM_ERR_SVM_FAILED = -8,
    NEXUSVM_ERR_CPU_NOT_SUPPORTED = -9,
    NEXUSVM_ERR_VM_RUNNING = -10,
    NEXUSVM_ERR_VM_NOT_RUNNING = -11,
    NEXUSVM_ERR_MIGRATION_FAILED = -12,
    NEXUSVM_ERR_CHECKPOINT_FAILED = -13,
} nexusvm_result_t;

/*============================================================================
 * CPU Virtualization Vendors
 *============================================================================*/

typedef enum {
    CPU_VENDOR_UNKNOWN = 0,
    CPU_VENDOR_INTEL = 1,
    CPU_VENDOR_AMD = 2,
} cpu_vendor_t;

/*============================================================================
 * Virtual Machine States
 *============================================================================*/

typedef enum {
    VM_STATE_CREATED = 0,
    VM_STATE_INITIALIZED = 1,
    VM_STATE_RUNNING = 2,
    VM_STATE_PAUSED = 3,
    VM_STATE_SUSPENDED = 4,
    VM_STATE_MIGRATING = 5,
    VM_STATE_CHECKPOINTING = 6,
    VM_STATE_SHUTTING_DOWN = 7,
    VM_STATE_SHUTDOWN = 8,
    VM_STATE_FAILED = 9,
} vm_state_t;

/*============================================================================
 * Register Structures
 *============================================================================*/

/**
 * General Purpose Registers (x86-64)
 */
typedef struct {
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, rbp, rsp;
    u64 r8, r9, r10, r11;
    u64 r12, r13, r14, r15;
    u64 rip, rflags;
} guest_gprs_t;

/**
 * Segment Registers
 */
typedef struct {
    u16 selector;
    u64 base;
    u32 limit;
    u32 attributes;
} segment_reg_t;

/**
 * Control Registers
 */
typedef struct {
    u64 cr0, cr2, cr3, cr4;
    u64 cr8;  /* Task priority register */
} guest_control_regs_t;

/**
 * Debug Registers
 */
typedef struct {
    u64 dr0, dr1, dr2, dr3;
    u64 dr6, dr7;
} guest_debug_regs_t;

/**
 * Model Specific Registers
 */
typedef struct {
    u64 ia32_efer;       /* Extended Feature Enable Register */
    u64 ia32_pat;        /* Page Attribute Table */
    u64 ia32_fs_base;
    u64 ia32_gs_base;
    u64 ia32_kernel_gs_base;
    u64 ia32_star;
    u64 ia32_lstar;
    u64 ia32_cstar;
    u64 ia32_sfmask;
    u64 ia32_sysenter_cs;
    u64 ia32_sysenter_esp;
    u64 ia32_sysenter_eip;
    u64 ia32_perf_global_ctrl;
    u64 ia32_pat;
    u64 ia32_debugctl;
    u64 ia32_mtrr_cap;
    u64 ia32_mtrr_def_type;
} guest_msrs_t;

/**
 * Complete CPU State
 */
typedef struct {
    guest_gprs_t gprs;
    guest_control_regs_t cr;
    guest_debug_regs_t dr;
    guest_msrs_t msrs;
    segment_reg_t cs, ds, es, fs, gs, ss;
    segment_reg_t tr, ldt;
    segment_reg_t gdt_base, idt_base;
    u64 gdtr_limit, idtr_limit;
    u64pending_exceptions;
    u64 active_events;
    u64 interruptibility_state;
} cpu_state_t;

/*============================================================================
 * VMX-Specific Structures
 *============================================================================*/

#ifdef __x86_64__

/**
 * VMX Basic MSR Values
 */
typedef struct {
    u32 revision_id;
    u32 vmx_on_info;         /* Bit 31 indicates VMXON region size */
    u32 vmcs_encrypt_size;   /* Bits 44:32 indicate encryption size */
    u64 vmcs_physical_addr_width;
} vmx_basic_msr_t;

/**
 * VMCS Field Encoding Categories
 */
typedef enum {
    VMCS_16BIT_CONTROL = 0,
    VMCS_16BIT_GUEST = 1,
    VMCS_16BIT_HOST = 2,
    VMCS_64BIT_CONTROL = 3,
    VMCS_64BIT_READONLY = 4,
    VMCS_64BIT_GUEST = 5,
    VMCS_64BIT_HOST = 6,
    VMCS_32BIT_CONTROL = 7,
    VMCS_32BIT_READONLY = 8,
    VMCS_32BIT_GUEST = 9,
    VMCS_32BIT_HOST = 10,
    VMCS_NATURAL_WIDTH_CONTROL = 11,
    VMCS_NATURAL_WIDTH_GUEST = 12,
    VMCS_NATURAL_WIDTH_HOST = 13,
} vmcs_field_type_t;

/* Important VMCS Field Encodings */
#define VMCS_VPID                         0x00000000
#define VMCS_POSTED_INTERRUPT_VECTOR      0x00000002
#define VMCS_POSTED_INTERRUPT_NPIE        0x00000004
#define VMCS_EPTP_INDEX                   0x00000006
#define VMCS_VMCS_LINK_POINTER            0x00002800
#define VMCS_VMCS_LINK_POINTER_HIGH       0x00002801
#define VMCS_GUEST_IA32_DEBUGCTL          0x00002802
#define VMCS_GUEST_IA32_DEBUGCTL_HIGH     0x00002803
#define VMCS_GUEST_IA32_PAT               0x00002804
#define VMCS_GUEST_IA32_PAT_HIGH          0x00002805
#define VMCS_GUEST_IA32_EFER              0x00002806
#define VMCS_GUEST_IA32_EFER_HIGH         0x00002807
#define VMCS_GUEST_IA32_PERF_GLOBAL_CTRL  0x00002808
#define VMCS_GUEST_IA32_PERF_GLOBAL_HIGH   0x00002809
#define VMCS_GUEST_PDPTE0                 0x0000280A
#define VMCS_GUEST_PDPTE0_HIGH            0x0000280B
#define VMCS_GUEST_PDPTE1                 0x0000280C
#define VMCS_GUEST_PDPTE1_HIGH            0x0000280D
#define VMCS_GUEST_PDPTE2                 0x0000280E
#define VMCS_GUEST_PDPTE2_HIGH            0x0000280F
#define VMCS_GUEST_PDPTE3                 0x00002810
#define VMCS_GUEST_PDPTE3_HIGH            0x00002811
#define VMCS_GUEST_IA32_BNDCFGS           0x00002812
#define VMCS_GUEST_IA32_BNDCFGS_HIGH      0x00002813
#define VMCS_HOST_IA32_PAT                0x00002C00
#define VMCS_HOST_IA32_PAT_HIGH           0x00002C01
#define VMCS_HOST_IA32_EFER               0x00002C02
#define VMCS_HOST_IA32_EFER_HIGH          0x00002C03
#define VMCS_HOST_IA32_PERF_GLOBAL_CTRL    0x00002C04
#define VMCS_HOST_IA32_PERF_GLOBAL_HIGH    0x00002C05
#define VMCS_HOST_CR0                     0x00006C00
#define VMCS_HOST_CR3                     0x00006C02
#define VMCS_HOST_CR4                     0x00006C04
#define VMCS_HOST_FS_BASE                 0x00006C06
#define VMCS_HOST_GS_BASE                 0x00006C08
#define VMCS_HOST_TR_BASE                 0x00006C0A
#define VMCS_HOST_GDTR_BASE                0x00006C0C
#define VMCS_HOST_IDTR_BASE                0x00006C0E
#define VMCS_HOST_RFLAGS                   0x00006C12
#define VMCS_HOST_RIP                      0x00006C14
#define VMCS_HOST_RSP                       0x00006C16

/* VM-Execution Control Fields */
#define VMCS_PIN_BASED_VM_EXECUTION_CONTROLS   0x00004000
#define VMCS_PRIMARY_PROC_BASED_VM_EXEC_CONTROLS 0x00004002
#define VMCS_EXCEPTION_BITMAP                  0x00004004
#define VMCS_PAGE_FAULT_ERROR_CODE_MASK        0x00004006
#define VMCS_PAGE_FAULT_ERROR_CODE_MATCH       0x00004008
#define VMCS_CR3_TARGET_COUNT                  0x0000400A
#define VMCS_VM_EXIT_CONTROLS                  0x0000400C
#define VMCS_VM_EXIT_CONTROLS_HIGH             0x0000400D
#define VMCS_VM_ENTRY_CONTROLS                 0x00004010
#define VMCS_VM_ENTRY_CONTROLS_HIGH            0x00004011
#define VMCS_VM_ENTRY_INTERRUPTION_INFO        0x00004012
#define VMCS_VM_ENTRY_EXCEPTION_ERROR_CODE     0x00004014
#define VMCS_VM_ENTRY_INSTRUCTION_LENGTH       0x00004016
#define VMCS_TPR_THRESHOLD                     0x00004018
#define VMCS_TPR_THRESHOLD_HIGH                0x00004019
#define VMCS_SECONDARY_PROC_BASED_VM_EXEC_CONTROLS 0x0000401E
#define VMCS_SECONDARY_PROC_BASED_VM_EXEC_CONTROLS_HIGH 0x0000401F
#define VMCS_PLE_GAP                           0x00004020
#define VMCS_PLE_GAP_HIGH                      0x00004021
#define VMCS_PLE_WINDOW                        0x00004022
#define VMCS_PLE_WINDOW_HIGH                   0x00004023

/* VM-Exit Information Fields */
#define VMCS_VM_EXIT_REASON                    0x00004400
#define VMCS_VM_EXIT_QUALIFICATION             0x00004402
#define VMCS_VM_EXIT_INSTRUCTION_LENGTH        0x00004404
#define VMCS_VM_EXIT_INSTRUCTION_INFO          0x00004406
#define VMCS_VM_EXIT_INTERRUPTION_INFO         0x00004408
#define VMCS_VM_EXIT_INTERRUPTION_ERROR_CODE   0x0000440A
#define VMCS_IDT_VECTORING_INFO               0x0000440C
#define VMCS_IDT_VECTORING_ERROR_CODE         0x0000440E
#define VMCS_VM_EXIT_INSTRUCTION_START         0x00004410
#define VMCS_VM_EXIT_INSTRUCTION_START_HIGH    0x00004411

/* Guest State Areas */
#define VMCS_GUEST_CR0                         0x00008000
#define VMCS_GUEST_CR3                         0x00008002
#define VMCS_GUEST_CR4                         0x00008004
#define VMCS_GUEST_ES_BASE                     0x00008006
#define VMCS_GUEST_CS_BASE                     0x00008008
#define VMCS_SS_BASE                           0x0000800A
#define VMCS_DS_BASE                           0x0000800C
#define VMCS_FS_BASE                           0x0000800E
#define VMCS_GS_BASE                           0x00008010
#define VMCS_LDTR_BASE                         0x00008012
#define VMCS_TR_BASE                           0x00008014
#define VMCS_GUEST_GDTR_BASE                   0x00008016
#define VMCS_GUEST_IDTR_BASE                   0x00008018
#define VMCS_GUEST_DR7                         0x0000801A
#define VMCS_GUEST_RSP                         0x0000801C
#define VMCS_GUEST_RIP                         0x0000801E
#define VMCS_GUEST_RFLAGS                      0x00008020
#define VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS    0x00008022
#define VMCS_GUEST_IA32_SYSENTER_CS            0x00008024
#define VMCS_GUEST_IA32_SYSENTER_ESP           0x00008026
#define VMCS_GUEST_IA32_SYSENTER_EIP           0x00008028
#define VMCS_GUEST_IA32_S_CET                  0x0000802A
#define VMCS_GUEST_SSP                         0x0000802C
#define VMCS_GUEST_IA32_INTERRUPT_SSP_TABLE_ADDR 0x0000802E

#define VMCS_GUEST_ES_LIMIT                    0x00008030
#define VMCS_GUEST_CS_LIMIT                    0x00008032
#define VMCS_GUEST_SS_LIMIT                    0x00008034
#define VMCS_GUEST_DS_LIMIT                    0x00008036
#define VMCS_GUEST_FS_LIMIT                    0x00008038
#define VMCS_GS_LIMIT                          0x0000803A
#define VMCS_GUEST_LDTR_LIMIT                  0x0000803C
#define VMCS_GUEST_TR_LIMIT                    0x0000803E
#define VMCS_GUEST_GDTR_LIMIT                  0x00008040
#define VMCS_GUEST_IDTR_LIMIT                  0x00008042
#define VMCS_GUEST_ES_ACCESS_RIGHTS            0x00008044
#define VMCS_GUEST_CS_ACCESS_RIGHTS            0x00008046
#define VMCS_GUEST_SS_ACCESS_RIGHTS            0x00008048
#define VMCS_GUEST_DS_ACCESS_RIGHTS            0x0000804A
#define VMCS_GUEST_FS_ACCESS_RIGHTS            0x0000804C
#define VMCS_GUEST_GS_ACCESS_RIGHTS            0x0000804E
#define VMCS_GUEST_LDTR_ACCESS_RIGHTS          0x00008050
#define VMCS_GUEST_TR_ACCESS_RIGHTS            0x00008052
#define VMCS_GUEST_INTERRUPTIBILITY_STATE      0x00008054
#define VMCS_GUEST_ACTIVITY_STATE              0x00008056
#define VMCS_GUEST_SM_BASE                     0x00008058
#define VMCS_GUEST_SM_BASE_HIGH                0x00008059
#define VMCS_GUEST_SMBASE                      0x0000805A
#define VMCS_GUEST_IA32_SYSENTER_CS            0x0000805C
#define VMCS_GUEST_IA32_SYSENTER_CS_HIGH       0x0000805D

/* CPU State Fields (for save/restore) */
#define VMCS_VMCS_REVISION_ID                  0x00000000

/**
 * VM-Exit Reasons
 */
typedef enum {
    VMX_EXIT_EXCEPTION_OR_NMI        = 0,
    VMX_EXIT_EXT_INTERRUPT            = 1,
    VMX_EXIT_TRIPLE_FAULT             = 2,
    VMX_EXIT_INIT_SIGNAL              = 3,
    VMX_EXIT_SIPI                     = 4,
    VMX_EXIT_SMI_IO                   = 5,
    VMX_EXIT_SMI_EXTERNAL             = 6,
    VMX_EXIT_PANIC_VMCS               = 7,
    VMX_EXIT_PANIC_MACHINE_CHECK      = 9,
    VMX_EXIT_VMX_TILE_SWITCH          = 10,
    VMX_EXIT_APIC_WRITE               = 11,
    VMX_EXIT_EOI_VIRTUALIZATION       = 12,
    VMX_EXIT_GASTOR_TRAP              = 13,
    VMX_EXIT_WBINVD                   = 14,
    VMX_EXIT_XSETBV                   = 15,
    VMX_EXIT_APIC_REG_VIRT            = 16,
    VMX_EXIT_VIRTUALIZED_EOI          = 17,
    VMX_EXIT_GALLeria_CHAOS          = 18,
    VMX_EXIT_EPT_VIOLATION            = 19,
    VMX_EXIT_EPT_MISCONFIG            = 20,
    VMX_EXIT_INVEPT                   = 21,
    VMX_EXIT_RDTSCP                   = 22,
    VMX_EXIT_VMX_PREEMPT_TIMER        = 23,
    VMX_EXIT_INVVPID                  = 24,
    VMX_EXIT_WBINVD_EXIT              = 25,
    VMX_EXIT_XRSTORS                  = 26,
    VMX_EXIT_XSAVES                   = 27,
    VMX_EXIT_UMWAIT                   = 28,
    VMX_EXIT_TPAUSE                   = 29,
    VMX_EXIT_LOAD_Uret                = 30,
    VMX_EXIT_MONITOR_TRAP_FLAG        = 31,
    VMX_EXIT_MONITOR                  = 32,
    VMX_EXIT_PAUSE                    = 33,
    VMX_EXIT_EXIT_HLT                 = 36,
    VMX_EXIT_EXIT_INVLPG              = 37,
    VMX_EXIT_EXIT_RDPMC               = 38,
    VMX_EXIT_EXIT_RDTSC               = 39,
    VMX_EXIT_EXIT_RDTSCP              = 40,
    VMX_EXIT_EXIT_RSM                 = 41,
    VMX_EXIT_EXIT_VMCALL              = 42,
    VMX_EXIT_EXIT_VMCLEAR             = 43,
    VMX_EXIT_EXIT_VMLAUNCH            = 44,
    VMX_EXIT_EXIT_VMPTRLD             = 45,
    VMX_EXIT_EXIT_VMPTRST             = 46,
    VMX_EXIT_EXIT_VMREAD              = 47,
    VMX_EXIT_EXIT_VMRESUME            = 48,
    VMX_EXIT_EXIT_VMRMW               = 49,
    VMX_EXIT_EXIT_VMSAVE              = 50,
    VMX_EXIT_EXIT_VMXOFF              = 51,
    VMX_EXIT_EXIT_VMXON               = 52,
    VMX_EXIT_EXIT_CR_ACCESS           = 53,
    VMX_EXIT_EXIT_DR_ACCESS           = 54,
    VMX_EXIT_EXIT_IO_INSTRUCTION      = 55,
    VMX_EXIT_EXIT_MSR_READ            = 56,
    VMX_EXIT_EXIT_MSR_WRITE           = 57,
    VMX_EXIT_EXIT_MCE_DURING_ENTRY    = 58,
    VMX_EXIT_EXIT_TPR_BELOW_THRESHOLD = 59,
    VMX_EXIT_EXIT_SMP_NA              = 60,
    VMX_EXIT_EXIT_SMP_S               = 61,
    VMX_EXIT_EXIT_SMP_R               = 62,
    VMX_EXIT_EXIT_FAST_SYSTEM_CALL    = 63,
    VMX_EXIT_EXIT_CPUID               = 64,
    VMX_EXIT_EXIT_HLT                 = 65,
    VMX_EXIT_EXIT_INVD                = 66,
    VMX_EXIT_EXIT_PAUSE               = 67,
    VMX_EXIT_EXIT_POPA                = 68,
    VMX_EXIT_EXIT_SYSENTER            = 69,
    VMX_EXIT_EXIT_SYSCALL             = 70,
    VMX_EXIT_EXIT_VERW                = 71,
    VMX_EXIT_EXIT_RDTSC               = 72,
    VMX_EXIT_EXIT_RDTSCP              = 73,
    VMX_EXIT_EXIT_IRET                = 74,
    VMX_EXIT_EXIT_SWAPGS              = 75,
    VMX_EXIT_EXIT_WBINVD              = 76,
    VMX_EXIT_EXIT_MONITOR             = 77,
    VMX_EXIT_EXIT_MWAIT               = 78,
    VMX_EXIT_EXIT_XSETBV             = 79,
    VMX_EXIT_EXIT_TASK_SWITCH          = 80,
    VMX_EXIT_EXIT_MCE_DURING_EXIT      = 81,
    VMX_EXIT_EXIT_TPR_THRESHOLD        = 82,
    VMX_EXIT_EXIT_APIC_ACCESS          = 83,
    VMX_EXIT_EXIT_EOI                  = 84,
    VMX_EXIT_EXIT_GDTR_IDTR            = 85,
    VMX_EXIT_EXIT_LDTR_TR              = 86,
    VMX_EXIT_EXIT_RDTSCP              = 87,
    VMX_EXIT_EXIT_RDPROCID            = 88,
} vmx_exit_reason_t;

/**
 * VMX Primary Processor-Based Execution Controls
 */
#define VMX_PROC_BASED_CTLS_HLT            BIT(7)
#define VMX_PROC_BASED_CTLS_MWAIT          BIT(10)
#define VMX_PROC_BASED_CTLS_RDTSC          BIT(12)
#define VMX_PROC_BASED_CTLS_CR3_STORE      BIT(15)
#define VMX_PROC_BASED_CTLS_CR3_LOAD       BIT(16)
#define VMX_PROC_BASED_CTLS_CR8_STORE      BIT(19)
#define VMX_PROC_BASED_CTLS_CR8_LOAD       BIT(20)
#define VMX_PROC_BASED_CTLS_TPR_SHADOW     BIT(21)
#define VMX_PROC_BASED_CTLS_NMI_WINDOW     BIT(22)
#define VMX_PROC_BASED_CTLS_MOV_DR         BIT(23)
#define VMX_PROC_BASED_CTLS_UNCOND_IO      BIT(24)
#define VMX_PROC_BASED_CTLS_IO_BITMAP       BIT(25)
#define VMX_PROC_BASED_CTLS_MSR_BITMAP     BIT(28)
#define VMX_PROC_BASED_CTLS_MONITOR         BIT(29)
#define VMX_PROC_BASED_CTLS_PAUSE          BIT(30)
#define VMX_PROC_BASED_CTLS_SECONDARY      BIT(31)

/**
 * VMX Secondary Processor-Based Execution Controls
 */
#define VMX_PROC_BASED_CTLS2_VIRTUAL_APIC     BIT(0)
#define VMX_PROC_BASED_CTLS2_EPT              BIT(1)
#define VMX_PROC_BASED_CTLS2_DESC_TABLE       BIT(2)
#define VMX_PROC_BASED_CTLS2_RDTSCP           BIT(3)
#define VMX_PROC_BASED_CTLS2_X2APIC          BIT(4)
#define VMX_PROC_BASED_CTLS2_VPID             BIT(5)
#define VMX_PROC_BASED_CTLS2_WBINVD          BIT(6)
#define VMX_PROC_BASED_CTLS2_UNRESTRICTED_GUEST BIT(7)
#define VMX_PROC_BASED_CTLS2_APIC_REG_VIRT    BIT(8)
#define VMX_PROC_BASED_CTLS2_VIRT_INT_DELIVERY BIT(9)
#define VMX_PROC_BASED_CTLS2_PAUSE_LOOP_EXIT  BIT(10)
#define VMX_PROC_BASED_CTLS2_RDRAND          BIT(11)
#define VMX_PROC_BASED_CTLS2_INVPCID         BIT(12)
#define VMX_PROC_BASED_CTLS2_VMFUNC          BIT(13)
#define VMX_PROC_BASED_CTLS2_VMCS_SHADOWING   BIT(14)
#define VMX_PROC_BASED_CTLS2_RDSEED          BIT(15)
#define VMX_PROC_BASED_CTLS2_EPT_VIOLATION    BIT(16)
#define VMX_PROC_BASED_CTLS2_PCOMMIT         BIT(22)
#define VMX_PROC_BASED_CTLS2_SHADOW_STACK     BIT(22)
#define VMX_PROC_BASED_CTLS2_PKS              BIT(24)

/**
 * VMX Pin-Based Execution Controls
 */
#define VMX_PIN_BASED_CTLS_EXT_INT_EXIT      BIT(0)
#define VMX_PIN_BASED_CTLS_NMI_EXIT         BIT(3)
#define VMX_PIN_BASED_CTLS_VIRTUAL_NMI      BIT(5)
#define VMX_PIN_BASED_CTLS_ACTIVE_VMX_PREEMPT_TIMER BIT(6)
#define VMX_PIN_BASED_CTLS_PROCESS_POSTED_INTERRUPTS BIT(7)

/**
 * VM-Entry Controls
 */
#define VMX_ENTRY_CTLS_LOAD_DEBUG           BIT(2)
#define VMX_ENTRY_CTLS_IA32E_MODE           BIT(9)
#define VMX_ENTRY_CTLS_ENTRY_TO_SMM         BIT(10)
#define VMX_ENTRY_CTLS_DEACTIVATE_DUAL      BIT(11)
#define VMX_ENTRY_CTLS_LOAD_PERF_GLOBAL_CTRL BIT(13)
#define VMX_ENTRY_CTLS_LOAD_PAT             BIT(14)
#define VMX_ENTRY_CTLS_LOAD_EFER            BIT(15)
#define VMX_ENTRY_CTLS_LOAD_BNDCFGS         BIT(16)
#define VMX_ENTRY_CTLS_HIDE_FROM_TRACE      BIT(21)

/**
 * VM-Exit Controls
 */
#define VMX_EXIT_CTLS_SAVE_DEBUG            BIT(2)
#define VMX_EXIT_CTLS_HOST_ADDRESS_SPACE_SIZE BIT(9)
#define VMX_EXIT_CTLS_LOAD_PERF_GLOBAL_CTRL  BIT(12)
#define VMX_EXIT_CTLS_ACK_INTERRUPT         BIT(15)
#define VMX_EXIT_CTLS_SAVE_PAT             BIT(18)
#define VMX_EXIT_CTLS_LOAD_PAT             BIT(19)
#define VMX_EXIT_CTLS_SAVE_EFER            BIT(20)
#define VMX_EXIT_CTLS_LOAD_EFER            BIT(21)
#define VMX_EXIT_CTLS_SAVE_VMX_PREEMPT_TIMER_VAL  BIT(22)

/* VMX Capability MSRs */
#define MSR_IA32_VMX_BASIC                  0x480
#define MSR_IA32_VMX_PINBASED_CTLS          0x481
#define MSR_IA32_VMX_PROCBASED_CTLS          0x482
#define MSR_IA32_VMX_VMEXIT_CTLS            0x483
#define MSR_IA32_VMX_VMENTRY_CTLS           0x484
#define MSR_IA32_VMX_MISC                    0x485
#define MSR_IA32_VMX_CR0_FIXED0             0x486
#define MSR_IA32_VMX_CR0_FIXED1             0x487
#define MSR_IA32_VMX_CR4_FIXED0             0x488
#define MSR_IA32_VMX_CR4_FIXED1             0x489
#define MSR_IA32_VMX_VMCS_ENUM              0x48A
#define MSR_IA32_VMX_SECONDARY_CTLS         0x48B
#define MSR_IA32_VMX_EPT_VPID_CAP           0x48C
#define MSR_IA32_VMX_TRUE_CTLS              0x48D
#define MSR_IA32_VMX_VMFUNC                 0x491

/* EPT Memory Types */
#define EPT_MEMORY_TYPE_UNCACHABLE          0
#define EPT_MEMORY_TYPE_WRITE_COMBINE        1
#define EPT_MEMORY_TYPE_WRITE_THROUGH        4
#define EPT_MEMORY_TYPE_WRITE_PROTECTED      5
#define EPT_MEMORY_TYPE_WRITE_BACK          6

/* EPT Page Walk Lengths */
#define EPT_PAGE_WALK_4                     4
#define EPT_PAGE_WALK_5                     5

#endif /* __x86_64__ */

/*============================================================================
 * SVM-Specific Structures (AMD-V)
 *============================================================================*/

#ifdef __amd64__

/* VMCB Bit Definitions */
#define VMCB_C bit (0)          /* Intercept CR read */
#define VMCB_D bit (1)          /* Intercept CR write */
#define VMCB_E bit (2)          /* Intercept DR read/write */
#define VMCB_F bit (3)          /* Intercept DR read/write */
#define VMCB_G bit (4)          /* Intercept GP faults */
#define VMCB_H bit (5)          /* Intercept INTR */
#define VMCB_I bit (6)          /* Intercept NMI */
#define VMCB_J bit (7)          /* Intercept SMI */
#define VMCB_K bit (8)          /* Intercept INIT */
#define VMCB_L bit (9)          /* Intercept VINTR */
#define VMCB_M bit (10)         /* Intercept CR0 write */
#define VMCB_N bit (11)         /* Intercept IDTR read */
#define VMCB_O bit (12)         /* Intercept GDTR read */
#define VMCB_P bit (13)         /* Intercept LDTR read */
#define VMCB_Q bit (14)         /* Intercept TR read */
#define VMCB_R bit (15)         /* Intercept IDTR write */
#define VMCB_S bit (16)         /* Intercept GDTR write */
#define VMCB_T bit (17)         /* Intercept LDTR write */
#define VMCB_U bit (18)         /* Intercept TR write */
#define VMCB_V bit (19)         /* Intercept RDTSC */
#define VMCB_W bit (20)         /* Intercept RDTSCP */
#define VMCB_X bit (21)         /* Intercept CR0 */
#define VMCB_Y bit (22)         /* Intercept CR1 */
#define VMCB_Z bit (23)         /* Intercept CR2 */
#define VMCB_a bit (24)         /* Intercept CR3 */
#define VMCB_b bit (25)         /* Intercept CR4 */
#define VMCB_c bit (26)         /* Intercept CR5 */
#define VMCB_d bit (27)         /* Intercept CR6 */
#define VMCB_e bit (28)         /* Intercept CR7 */
#define VMCB_f bit (29)         /* Intercept CR8 */
#define VMCB_g bit (30)         /* Intercept CR9 */
#define VMCB_h bit (31)         /* Intercept CR10 */
#define VMCB_i bit (32)         /* Intercept CR11 */
#define VMCB_j bit (33)         /* Intercept CR12 */
#define VMCB_k bit (34)         /* Intercept CR13 */
#define VMCB_l bit (35)         /* Intercept CLTS */
#define VMCB_m bit (36)         /* Intercept LMSW */
#define VMCB_n bit (37)         /* Intercept MOV CR */
#define VMCB_o bit (38)         /* Intercept MOV DR */
#define VMCB_p bit (39)         /* Intercept MOV NE */
#define VMCB_q bit (40)         /* Intercept INVD */
#define VMCB_r bit (41)         /* Intercept WBINVD */
#define VMCB_s bit (42)         /* Intercept MONITOR */
#define VMCB_t bit (43)         /* Intercept MWAIT */
#define VMCB_u bit (44)         /* Intercept PAUSE */
#define VMCB_v bit (45)         /* Intercept HLT */
#define VMCB_w bit (46)         /* Intercept SVML */
#define VMCB_x bit (47)         /* Intercept INR */
#define VMCB_y bit (48)         /* Intercept INVLPG */
#define VMCB_z bit (49)         Intercept WBINVD */
#define VMCB_A bit (50)         Intercept INTR */
#define VMCB_B bit (51)         Intercept NMI */
#define VMCB_C bit (52)         Intercept SMI */
#define VMCB_D bit (53)         Intercept INIT */
#define VMCB_E bit (54)         Intercept VINTR */
#define VMCB_F bit (55)         Intercept CPuid */
#define VMCB_G bit (56)         Intercept RSM */
#define VMCB_H bit (57)         Intercept POPA */
#define VMCB_I bit (58)         Intercept RET */
#define VMCB_J bit (59)         Intercept ENTER */
#define VMCB_K bit (60)         Intercept LEAVE */
#define VMCB_L bit (61)         Intercept LGDT */
#define VMCB_M bit (62)         Intercept LIDT */
#define VMCB_N bit (63)         Intercept SGDT */
#define VMCB_O bit (64)         Intercept SIDT */
#define VMCB_P bit (65)         Intercept LTR */
#define VMCB_Q bit (66)         Intercept STR */
#define VMCB_R bit (67)         Intercept GDT */
#define VMCB_S bit (68)         Intercept IDT */
#define VMCB_T bit (69)         Intercept LDT */
#define VMCB_U bit (70)         Intercept LMSW */
#define VMCB_V bit (71)         Intercept SMSW */
#define VMCB_W bit (72)         Intercept LMSW */
#define VMCB_X bit (73)         Intercept SYSCALL */
#define VMCB_Y bit (74)         Intercept SYSRET */
#define VMCB_Z bit (75)         Intercept SYSENTER */
#define VMCB_a bit (76)         Intercept SYSEXIT */
#define VMCB_b bit (77)         Intercept MRESUME */
#define VMCB_c bit (78)         Intercept VMRUN */
#define VMCB_d bit (79)         Intercept VMMCALL */
#define VMCB_e bit (80)         Intercept VMLOAD */
#define VMCB_f bit (81)         Intercept VMSAVE */
#define VMCB_g bit (82)         Intercept STGI */
#define VMCB_h bit (83)         Intercept CLGI */
#define VMCB_i bit (84)         Intercept SKINIT */
#define VMCB_j bit (85)         Intercept GETSEC */
#define VMCB_k bit (86)         Intercept INVEPT */
#define VMCB_l bit (87)         Intercept INVVPID */
#define VMCB_m bit (88)         Intercept VMREAD */
#define VMCB_n bit (89)         Intercept VMWRITE */
#define VMCB_o bit (90)         Intercept VMK */
#define VMCB_p bit (91)         Intercept VMKX */
#define VMCB_q bit (92)         Intercept VMFUNC */

/* SVM Exit Codes */
#define SVM_EXIT_READ_CR0       0x001
#define SVM_EXIT_WRITE_CR0     0x002
#define SVM_EXIT_READ_CR1      0x003
#define SVM_EXIT_WRITE_CR1     0x004
#define SVM_EXIT_READ_CR2      0x005
#define SVM_EXIT_WRITE_CR2     0x006
#define SVM_EXIT_READ_CR3      0x007
#define SVM_EXIT_WRITE_CR3     0x008
#define SVM_EXIT_READ_CR4      0x009
#define SVM_EXIT_WRITE_CR4     0x00A
#define SVM_EXIT_READ_CR5      0x00B
#define SVM_EXIT_WRITE_CR5     0x00C
#define SVM_EXIT_READ_CR6      0x00D
#define SVM_EXIT_WRITE_CR6     0x00E
#define SVM_EXIT_READ_CR7      0x00F
#define SVM_EXIT_WRITE_CR7     0x010
#define SVM_EXIT_READ_CR8      0x011
#define SVM_EXIT_WRITE_CR8     0x012
#define SVM_EXIT_READ_CR9      0x013
#define SVM_EXIT_WRITE_CR9     0x014
#define SVM_EXIT_READ_CR10     0x015
#define SVM_EXIT_WRITE_CR10    0x016
#define SVM_EXIT_READ_CR11     0x017
#define SVM_EXIT_WRITE_CR11    0x018
#define SVM_EXIT_READ_CR12     0x019
#define SVM_EXIT_WRITE_CR12    0x01A
#define SVM_EXIT_READ_CR13     0x01B
#define SVM_EXIT_WRITE_CR13    0x01C
#define SVM_EXIT_READ_CR14     0x01D
#define SVM_EXIT_WRITE_CR14    0x01E
#define SVM_EXIT_READ_CR15     0x01F
#define SVM_EXIT_WRITE_CR15    0x020
#define SVM_EXIT_READ_DR0      0x021
#define SVM_EXIT_WRITE_DR0     0x022
#define SVM_EXIT_READ_DR1      0x023
#define SVM_EXIT_WRITE_DR1     0x024
#define SVM_EXIT_READ_DR2      0x025
#define SVM_EXIT_WRITE_DR2     0x026
#define SVM_EXIT_READ_DR3      0x027
#define SVM_EXIT_WRITE_DR3     0x028
#define SVM_EXIT_READ_DR4      0x029
#define SVM_EXIT_WRITE_DR4     0x02A
#define SVM_EXIT_READ_DR5      0x02B
#define SVM_EXIT_WRITE_DR5     0x02C
#define SVM_EXIT_READ_DR6      0x02D
#define SVM_EXIT_WRITE_DR6     0x02E
#define SVM_EXIT_READ_DR7      0x02F
#define SVM_EXIT_WRITE_DR7     0x030
#define SVM_EXIT_EXCP0         0x031
#define SVM_EXIT_EXCP1         0x032
#define SVM_EXIT_EXCP2         0x033
#define SVM_EXIT_EXCP3         0x034
#define SVM_EXIT_EXCP4         0x035
#define SVM_EXIT_EXCP5         0x036
#define SVM_EXIT_EXCP6         0x037
#define SVM_EXIT_EXCP7         0x038
#define SVM_EXIT_EXCP8         0x039
#define SVM_EXIT_EXCP9         0x03A
#define SVM_EXIT_EXCP10        0x03B
#define SVM_EXIT_EXCP11        0x03C
#define SVM_EXIT_EXCP12        0x03D
#define SVM_EXIT_EXCP13        0x03E
#define SVM_EXIT_EXCP14        0x03F
#define SVM_EXIT_EXCP15        0x040
#define SVM_EXIT_INTR          0x060
#define SVM_EXIT_NMI           0x061
#define SVM_EXIT_SMI           0x062
#define SVM_EXIT_INIT          0x063
#define SVM_EXIT_VINTR         0x064
#define SVM_EXIT_CPUID         0x072
#define SVM_EXIT_HPRED         0x073
#define SVM_EXIT_LDTR          0x074
#define SVM_EXIT_LTR          0x075
#define SVM_EXIT_INVEPT       0x020
#define SVM_EXIT_INVVPID      0x021
#define SVM_EXIT_INVLPG       0x076
#define SVM_EXIT_WBINVD       0x077
#define SVM_EXIT_MONITOR      0x078
#define SVM_EXIT_MWAIT        0x079
#define SVM_EXIT_RDTSC        0x07C
#define SVM_EXIT_RDTSCP       0x07D
#define SVM_EXIT_POPA         0x07E
#define SVM_EXIT_GETSEC       0x080
#define SVM_EXIT_RDPMC        0x081
#define SVM_EXIT_RDTSCP       0x082
#define SVM_EXIT_SMCI         0x083
#define SVM_EXIT_SMCM         0x084
#define SVM_EXIT_RSM          0x085
#define SVM_EXIT_RDMSR        0x086
#define SVM_EXIT_WRMSR        0x087
#define SVM_EXIT_FERR_FREEZE  0x088
#define SVM_EXIT_PAUSE        0x089
#define SVM_EXIT_HLT          0x08A
#define SVM_EXIT_INVD         0x08B
#define SVM_EXIT_WBINVD       0x08C
#define SVM_EXIT_MONITOR      0x08D
#define SVM_EXIT_MWAIT        0x08E
#define SVM_EXIT_XSETBV       0x08F
#define SVM_EXIT_EFER_WRITE   0x090
#define SVM_EXIT_CR0_WRITE    0x091
#define SVM_EXIT_CR1_WRITE    0x092
#define SVM_EXIT_CR2_WRITE    0x093
#define SVM_EXIT_CR3_WRITE    0x094
#define SVM_EXIT_CR4_WRITE    0x095
#define SVM_EXIT_CR5_WRITE    0x096
#define SVM_EXIT_CR6_WRITE    0x097
#define SVM_EXIT_CR7_WRITE    0x098
#define SVM_EXIT_CR8_WRITE    0x099
#define SVM_EXIT_CR9_WRITE    0x09A
#define SVM_EXIT_CR10_WRITE   0x09B
#define SVM_EXIT_CR11_WRITE   0x09C
#define SVM_EXIT_CR12_WRITE   0x09D
#define SVM_EXIT_CR13_WRITE   0x09E
#define SVM_EXIT_CR14_WRITE   0x09F
#define SVM_EXIT_CR15_WRITE   0x0A0
#define SVM_EXIT_INVLPGA      0x0C1
#define SVM_EXIT_SKINIT       0x0C2
#define SVM_EXIT_INVLPGA      0x0C3
#define SVM_EXIT_NPF          0x400
#define SVM_EXIT_AVIC_INCOMPLETE_IPI  0x401
#define SVM_EXIT_AVIC_NOaccELERATION  0x402
#define SVM_EXIT_VMGEXIT      0x403

/* SVM MSRs */
#define MSR_VM_HSAVE_PA       0xC0010117
#define MSR_SVM_EPBC          0xC0010119
#define MSR_SVM_VM_CR         0xC001011A
#define MSR_SVM_VM_HSAVE_PA_MSR 0xC0010117

/* VMCB Control Area */
#define VMCB_OFFSET_INTERCEPT           0x00
#define VMCB_OFFSET_IOIO_MAP            0x18
#define VMCB_OFFSET_MSR_MAP              0x20
#define VMCB_OFFSET_TSC_SCALE            0x28
#define VMCB_OFFSET_TSC_OFFSET           0x30
#define VMCB_OFFSET_GUEST_ASID           0x38
#define VMCB_OFFSET_TLB_CONTROL          0x3C
#define VMCB_OFFSET_VIRQ                 0x40
#define VMCB_OFFSET_EXITCODE             0x44
#define VMCB_OFFSET_EXITINFO1            0x48
#define VMCB_OFFSET_EXITINFO2            0x50
#define VMCB_OFFSET_EXITINTINFO          0x58
#define VMCB_OFFSET_NESTED_PAGING_ROOT   0x60
#define VMCB_OFFSET_LBR_VIRTUALIZATION   0x68
#define VMCB_OFFSET_VMPL                 0x70

#endif /* __amd64__ */

/*============================================================================
 * Hypercall Structures
 *============================================================================*/

/**
 * Hypercall Numbers
 */
typedef enum {
    /* VM Lifecycle Management */
    HYPERCALL_VM_CREATE = 0x1000,
    HYPERCALL_VM_DESTROY,
    HYPERCALL_VM_START,
    HYPERCALL_VM_STOP,
    HYPERCALL_VM_PAUSE,
    HYPERCALL_VM_RESUME,
    HYPERCALL_VM_RESET,
    
    /* Memory Management */
    HYPERCALL_VM_ALLOC_MEM = 0x1100,
    HYPERCALL_VM_FREE_MEM,
    HYPERCALL_VM_MAP_MEM,
    HYPERCALL_VM_UNMAP_MEM,
    HYPERCALL_VM_SET_EPTP,
    
    /* CPU Management */
    HYPERCALL_VCPU_CREATE = 0x1200,
    HYPERCALL_VCPU_DESTROY,
    HYPERCALL_VCPU_START,
    HYPERCALL_VCPU_STOP,
    HYPERCALL_VCPU_AFFINITY,
    HYPERCALL_VCPU_MIGRATE,
    
    /* I/O and Device Management */
    HYPERCALL_VM_REGISTER_PASSTHROUGH = 0x1300,
    HYPERCALL_VM_UNREGISTER_PASSTHROUGH,
    HYPERCALL_VM_CREATE_IRQCHIP,
    HYPERCALL_VM_CREATE_PIC,
    HYPERCALL_VM_CREATE_IOAPIC,
    HYPERCALL_VM_CREATE_LAPIC,
    
    /* Interrupt Management */
    HYPERCALL_VM_INJECT_INTERRUPT = 0x1400,
    HYPERCALL_VM_SET_IRQ_LINE,
    HYPERCALL_VM_GET_IRQ_STATUS,
    HYPERCALL_VM_SET_LAPIC_REG,
    HYPERCALL_VM_GET_LAPIC_REG,
    
    /* Migration and Checkpoint */
    HYPERCALL_VM_SNAPSHOT = 0x1500,
    HYPERCALL_VM_RESTORE,
    HYPERCALL_VM_LIVE_MIGRATE_START,
    HYPERCALL_VM_LIVE_MIGRATE_FINISH,
    HYPERCALL_VM_CHECKPOINT_SAVE,
    HYPERCALL_VM_CHECKPOINT_LOAD,
    
    /* Information */
    HYPERCALL_VM_GET_INFO = 0x1600,
    HYPERCALL_VCPU_GET_STATE,
    HYPERCALL_VCPU_SET_STATE,
    HYPERCALL_VM_GET_STATISTICS,
    HYPERCALL_VCPU_GET_STATISTICS,
    
    /* Nested Virtualization */
    HYPERCALL_NVM_VMEXIT_TO_HYPERVISOR = 0x1700,
    HYPERCALL_NVM_GUEST_VMENTRY,
    HYPERCALL_NVM_SETUP_VMEXIT_HANDLER,
    
    /* Debug */
    HYPERCALL_DEBUG_READ = 0x1F00,
    HYPERCALL_DEBUG_WRITE,
    HYPERCALL_BENCHMARK_START,
    HYPERCALL_BENCHMARK_END,
} hypercall_number_t;

/**
 * Hypercall Parameter Structure
 */
typedef struct {
    u64 rax;
    u64 rbx;
    u64 rcx;
    u64 rdx;
    u64 rsi;
    u64 rdi;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
} hypercall_params_t;

/**
 * Hypercall Result
 */
typedef struct {
    u64 status;
    u64 ret0;
    u64 ret1;
    u64 ret2;
    u64 ret3;
} hypercall_result_t;

/*============================================================================
 * Memory Management Structures
 *============================================================================*/

/**
 * Memory Region Types
 */
typedef enum {
    MEM_REGION_RAM = 0,
    MEM_REGION_ROM,
    MEM_REGION_MMIO,
    MEM_REGION_IOMEM,
    MEM_REGION_DEVICE,
    MEM_REGION_SHARED,
    MEM_REGION_VOLATILE,
} memory_region_type_t;

/**
 * Memory Region
 */
typedef struct memory_region {
    guest_addr_t guest_phys;
    phys_addr_t host_phys;
    u64 size;
    memory_region_type_t type;
    u32 flags;
    char *name;
    struct memory_region *next;
} memory_region_t;

/* Memory Region Flags */
#define MEM_REGION_READABLE     BIT(0)
#define MEM_REGION_WRITABLE     BIT(1)
#define MEM_REGION_EXECUTABLE    BIT(2)
#define MEM_REGION_CACHED        BIT(3)
#define MEM_REGION_WCOHERENT     BIT(4)
#define MEM_REGION_IOCOHERENT    BIT(5)
#define MEM_REGION_ZERO          BIT(6)
#define MEM_REGION_PASSTHROUGH   BIT(7)

/*============================================================================
 * EPT/NPT Structures
 *============================================================================*/

/**
 * EPT Page Table Entry
 */
typedef struct {
    u64 present          : 1;
    u64 write            : 1;
    u64 executable       : 1;
    u64 memory_type      : 3;    /* 0=UC, 1=WC, 4=WT, 5=WP, 6=WB */
    u64 ignore_pat       : 1;
    u64 accessed         : 1;
    u64 dirty            : 1;
    u64 access_pagesize  : 1;   /* 1 = this is a 1GB page */
    u64 global           : 1;
    u64 available        : 2;
    u64 physical_address : 40;   /* Bits 51:12 of physical address */
    u64 available2       : 12;
} ept_pte_t;

/**
 * EPT Page-Middle-Level Entry
 */
typedef struct {
    u64 present          : 1;
    u64 write            : 1;
    u64 executable       : 1;
    u64 memory_type      : 3;
    u64 ignore_pat       : 1;
    u64 accessed         : 1;
    u64 dirty            : 1;
    u64 big_pagesize     : 1;
    u64 available        : 3;
    u64 physical_address : 40;
    u64 available2       : 12;
} ept_pme_t;

/**
 * EPT Page-Upper-Level Entry
 */
typedef struct {
    u64 present          : 1;
    u64 write            : 1;
    u64 executable       : 1;
    u64 memory_type      : 3;
    u64 ignore_pat       : 1;
    u64 accessed         : 1;
    u64 dirty            : 1;
    u64 big_pagesize     : 1;
    u64 available        : 3;
    u64 physical_address : 40;
    u64 available2       : 12;
} ept_pue_t;

/**
 * EPT Page-Directory-Pointer-Table Entry
 */
typedef struct {
    u64 present          : 1;
    u64 write            : 1;
    u64 executable       : 1;
    u64 memory_type      : 3;
    u64 ignore_pat       : 1;
    u64 accessed         : 1;
    u64 dirty            : 1;
    u64 big_pagesize     : 1;
    u64 available        : 3;
    u64 physical_address : 40;
    u64 available2       : 12;
} ept_pdpe_t;

/**
 * EPT Page-Map-Level-4 Entry
 */
typedef struct {
    u64 present          : 1;
    u64 write            : 1;
    u64 executable       : 1;
    u64 memory_type      : 3;
    u64 ignore_pat       : 1;
    u64 accessed         : 1;
    u64 dirty            : 1;
    u64 big_pagesize     : 1;
    u64 available        : 3;
    u64 physical_address : 40;
    u64 available2       : 12;
} ept_pml4e_t;

/**
 * Extended Page Table Pointer (EPTP)
 */
typedef struct {
    u64 memory_type          : 3;
    u64 page_walk_length     : 3;    /* Value minus 1 */
    u64 enable_accessed_dirty : 1;
    u64 enable_dirty_bit     : 1;
    u64 enable_pml5          : 1;
    u64 available            : 5;
    u64 pml4_physical_address: 40;
    u64 available2           : 12;
} eptp_t;

/*============================================================================
 * VM Configuration Structures
 *============================================================================*/

/**
 * Virtual CPU Configuration
 */
typedef struct {
    u32 vcpu_id;
    u32 apic_id;
    u32 socket_id;
    u32 core_id;
    u32 thread_id;
    u64 guest_rip;
    u64 guest_rsp;
    u64 guest_cr3;
    u32 cpu_count;
    affinity_t affinity;
} vcpu_config_t;

/**
 * Virtual Machine Configuration
 */
typedef struct {
    char name[64];
    u32 id;
    u32 vcpu_count;
    u64 memory_size;
    u32 memory_region_count;
    memory_region_t *memory_regions;
    vcpu_config_t *vcpu_configs;
    u64 kernel_entry;
    u64 initrd_addr;
    u64 initrd_size;
    char kernel_cmdline[256];
    u64 eptp;
    bool nested_virtualization;
    bool unrestricted_guest;
    bool long_mode;
    bool pae;
    bool apic;
    bool x2apic;
    bool vpid_enabled;
    bool ept_enabled;
    bool preemption_timer;
} vm_config_t;

/*============================================================================
 * Device and I/O Structures
 *============================================================================*/

/**
 * PCI Configuration Space
 */
typedef struct {
    u16 vendor_id;
    u16 device_id;
    u16 command;
    u16 status;
    u8 revision_id;
    u8 prog_if;
    u8 subclass;
    u8 class_code;
    u8 cache_line_size;
    u8 latency_timer;
    u8 header_type;
    u8 bist;
    u32 bar[6];
    u32 cardbus_cis_ptr;
    u16 subsystem_vendor_id;
    u16 subsystem_id;
    u32 expansion_rom_base_addr;
    u8 capabilities_ptr;
    u8 reserved[7];
    u8 interrupt_line;
    u8 interrupt_pin;
    u8 min_grant;
    u8 max_latency;
} pci_config_space_t;

/**
 * I/O Port Access
 */
typedef struct {
    u16 port;
    u32 size;      /* 1, 2, 4, 8 */
    u64 value;
    bool is_write;
} io_access_t;

/**
 * MSI Address Format
 */
typedef struct {
    u64 destination_mode : 1;    /* 0 = physical, 1 = logical */
    u64 redirection_hint : 1;
    u64 reserved1        : 2;
    u64 destination_id   : 8;
    u64 reserved2        : 4;
    u64 base_address     : 36;
    u64 reserved3        : 12;
} msi_address_t;

/**
 * MSI Data Format
 */
typedef struct {
    u16 vector           : 8;
    u16 delivery_mode    : 3;
    u16 trigger_mode     : 1;
    u16 reserved1        : 1;
    u16 level            : 1;
    u16 reserved2        : 1;
    u16 assert           : 1;
    u16 reserved3        : 2;
} msi_data_t;

/*============================================================================
 * Interrupt Controller Structures
 *============================================================================*/

/**
 * Interrupt Type
 */
typedef enum {
    INT_TYPE_INT         = 0,
    INT_TYPE_NMI         = 2,
    INT_TYPE_SMI         = 3,
    INT_TYPE_EXT_INT     = 4,
    INT_TYPE_HARDWARE    = 5,
} interrupt_type_t;

/**
 * Interrupt Delivery Info
 */
typedef struct {
    u32 vector           : 8;
    u32 type             : 3;
    u32 delivery_pending : 1;
    u32 destination_mode : 1;
    u32 delivery_mode    : 3;
    u32 level_triggered  : 1;
    u32 assertion        : 1;
    u32 reserved         : 2;
    u32 destination_id   : 8;
    u32 reserved2        : 4;
} interrupt_info_t;

/**
 * Local APIC Registers
 */
typedef enum {
    LAPIC_ID             = 0x0020,
    LAPIC_VERSION        = 0x0030,
    LAPIC_TPR            = 0x0080,
    LAPIC_APR            = 0x0090,
    LAPIC_PPR            = 0x00A0,
    LAPIC_EOI             = 0x00B0,
    LAPIC_LDR            = 0x00D0,
    LAPIC_DFR            = 0x00E0,
    LAPIC_SIVR           = 0x00F0,
    LAPIC_ISR0           = 0x0100,
    LAPIC_ISR1           = 0x0110,
    LAPIC_ISR2           = 0x0120,
    LAPIC_ISR3           = 0x0130,
    LAPIC_ISR4           = 0x0140,
    LAPIC_ISR5           = 0x0150,
    LAPIC_ISR6           = 0x0160,
    LAPIC_ISR7           = 0x0170,
    LAPIC_TMR0           = 0x0180,
    LAPIC_TMR1           = 0x0190,
    LAPIC_TMR2           = 0x01A0,
    LAPIC_TMR3           = 0x01B0,
    LAPIC_TMR4           = 0x01C0,
    LAPIC_TMR5           = 0x01D0,
    LAPIC_TMR6           = 0x01E0,
    LAPIC_TMR7           = 0x01F0,
    LAPIC_IRR0           = 0x0200,
    LAPIC_IRR1           = 0x0210,
    LAPIC_IRR2           = 0x0220,
    LAPIC_IRR3           = 0x0230,
    LAPIC_IRR4           = 0x0240,
    LAPIC_IRR5           = 0x0250,
    LAPIC_IRR6           = 0x0260,
    LAPIC_IRR7           = 0x0270,
    LAPIC_ERROR_STATUS   = 0x0280,
    LAPIC_ICR0           = 0x0300,
    LAPIC_ICR1           = 0x0310,
    LAPIC_LVT_TIMER      = 0x0320,
    LAPIC_LVT_THERMAL    = 0x0330,
    LAPIC_LVT_PERF       = 0x0340,
    LAPIC_LVT_LINT0      = 0x0350,
    LAPIC_LVT_LINT1      = 0x0360,
    LAPIC_LVT_ERROR      = 0x0370,
    LAPIC_INITIAL_COUNT  = 0x0380,
    LAPIC_CURRENT_COUNT  = 0x0390,
    LAPIC_DIVIDE_CFG     = 0x03E0,
} lapic_register_t;

/*============================================================================
 * Scheduler Structures
 *============================================================================*/

/**
 * Scheduler Policy
 */
typedef enum {
    SCHED_FIFO = 0,
    SCHED_RR,           /* Round Robin */
    SCHED_CFS,          /* Completely Fair Scheduler */
    SCHED_BVT,          /* Borrowed Virtual Time */
    SCHED_WALT,         /* Window-Assisted Load Tracking */
} sched_policy_t;

/**
 * VM Priority
 */
typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_NORMAL,
    PRIORITY_HIGH,
    PRIORITY_REALTIME,
} vm_priority_t;

/**
 * CPU Affinity
 */
typedef struct {
    u64 mask[16];        /* Up to 1024 CPUs */
    u32 nr_cpus;
} affinity_t;

/**
 * Scheduling Entity
 */
typedef struct sched_entity {
    u64 se_id;
    u32 vm_id;
    u32 vcpu_id;
    u64 runtime;         /* Time spent on CPU */
    u64 vruntime;        /* Virtual runtime for CFS */
    u64 deadline;        /* For real-time scheduling */
    u64 period;          /* For real-time scheduling */
    u64 budget;         /* For real-time scheduling */
    sched_policy_t policy;
    vm_priority_t priority;
    affinity_t allowed_cpus;
    u32 weight;
    bool preemptible;
    struct sched_entity *next;
    struct sched_entity *prev;
} sched_entity_t;

/**
 * CPU Statistics
 */
typedef struct {
    u64 idle_time;
    u64 busy_time;
    u64 user_time;
    u64 kernel_time;
    u64 irq_time;
    u64 softirq_time;
    u64 steal_time;
    u64 guest_time;
    u64 guest_nice_time;
    u64 context_switches;
    u64 interrupts;
    u64 soft_interrupts;
} cpu_stats_t;

/*============================================================================
 * Migration Structures
 *============================================================================*/

/**
 * Migration State
 */
typedef enum {
    MIGRATION_STATE_INITIATED = 0,
    MIGRATION_STATE_PRECOPY,
    MIGRATION_STATE_STOP_AND_COPY,
    MIGRATION_STATE_SETUP,
    MIGRATION_STATE_TRANSFER,
    MIGRATION_STATE_RESUME,
    MIGRATION_STATE_COMPLETED,
    MIGRATION_STATE_FAILED,
    MIGRATION_STATE_CANCELLED,
} migration_state_t;

/**
 * Migration Configuration
 */
typedef struct {
    char target_host[256];
    u16 target_port;
    u32 max_bandwidth;        /* bytes per second, 0 = unlimited */
    bool auto_converge;        /* Enable automatic convergence */
    bool compression;         /* Enable memory compression */
    bool paused;              /* Start with paused VM */
    u64 downtime_quantum;      /* Maximum allowed downtime in ns */
    u32 max_convergence_iterations;
} migration_config_t;

/**
 * Migration Statistics
 */
typedef struct {
    migration_state_t state;
    u64 total_pages;
    u64 transferred_pages;
    u64 dirty_pages;
    u64 remaining_pages;
    u64 duplicate_pages;
    u64 compression_ratio;
    u64 transferred_bytes;
    u64 downtime_ns;
    u64 elapsed_time_ns;
    double progress_percent;
    double bandwidth_mbps;
} migration_stats_t;

/*============================================================================
 * Checkpoint Structures
 *============================================================================*/

/**
 * Checkpoint Header
 */
typedef struct {
    char magic[8];            /* "NXVMCHKP" */
    u32 version;
    u32 checksum;
    u64 timestamp;
    u64 vm_id;
    u32 vcpu_count;
    u32 memory_regions;
    u64 total_size;
    u64 memory_size;
    char vm_name[64];
    vm_state_t state;
} checkpoint_header_t;

/**
 * Checkpoint Section Types
 */
typedef enum {
    CHECKPOINT_SECTION_HEADER = 0,
    CHECKPOINT_SECTION_VM_CONFIG,
    CHECKPOINT_SECTION_VCPU_STATE,
    CHECKPOINT_SECTION_MEMORY,
    CHECKPOINT_SECTION_DEVICE,
    CHECKPOINT_SECTION_MSR,
    CHECKPOINT_SECTION_FPU,
    CHECKPOINT_SECTION_XSAVE,
} checkpoint_section_type_t;

/*============================================================================
 * Nested Virtualization Structures
 *============================================================================*/

/**
 * L2 VM State (for nested virtualization)
 */
typedef struct {
    void *vmcs;               /* L2 VMCS pointer */
    cpu_state_t guest_state;  /* L2 guest state */
    u64 vmexit_handler;       /* Handler for L2 VMEXITs */
    u64 vmexit_reason;
    u64 vmexit_qualification;
    u64 vmexit_instruction_len;
    bool running;
} l2_state_t;

/**
 * Nested VMX Controls
 */
typedef struct {
    u64 vmcs_link_pointer;
    u64 vmxon_pointer;
    bool l1_guest_mode;
} nested_vmx_t;

/**
 * SVM Nested State (L1/L2 nesting)
 */
typedef struct {
    void *vmcb;               /* L2 VMCB */
    cpu_state_t guest_state;
    bool nested_running;
} nested_svm_t;

/*============================================================================
 * Utility Macros
 *============================================================================*/

#define BIT(n) (1UL << (n))
#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define PAGE_ALIGN(x) ALIGN(x, 4096)
#define SIZE_ALIGN(x, a) ALIGN(x, a)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

/* Page size definitions */
#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)
#define PAGE_MASK       (~(PAGE_SIZE - 1))

#define HUGE_PAGE_SHIFT     21
#define HUGE_PAGE_SIZE      (1UL << HUGE_PAGE_SHIFT)
#define HUGE_PAGE_MASK      (~(HUGE_PAGE_SIZE - 1))

#define MEG_PAGE_SHIFT      21
#define MEG_PAGE_SIZE       (1UL << MEG_PAGE_SHIFT)

#define GIG_PAGE_SHIFT      30
#define GIG_PAGE_SIZE       (1UL << GIG_PAGE_SHIFT)

/* Guest physical to host physical conversion */
#define GPA_TO_HPA(gpa, vm) ((vm)->ept_mappings[gpa])

#ifdef __cplusplus
}
#endif

#endif /* NEXUSVM_TYPES_H */
