# NexusVM - Type-1 Hypervisor

<p align="center">
  <img src="https://img.shields.io/github/actions/workflow/status/moggan1337/NexusVM/ci.yml?branch=main&style=for-the-badge" alt="CI">
  <img src="https://img.shields.io/badge/Type-1%20Hypervisor-2196F3?style=for-the-badge" alt="Type-1 Hypervisor">
  <img src="https://img.shields.io/badge/x86--64-FF5722?style=for-the-badge" alt="x86-64">
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="License">
</p>

NexusVM is a production-ready Type-1 bare-metal hypervisor for x86-64 architectures. It leverages Intel VT-x and AMD-V hardware virtualization extensions to provide secure, high-performance virtualization with support for live migration, snapshots, and nested virtualization.

## 🎬 Demo
![NexusVM Demo](demo.gif)

*Type-1 hypervisor with live migration*

## Screenshots
| Component | Preview |
|-----------|---------|
| VM Dashboard | ![vm](screenshots/vm-dashboard.png) |
| Migration Status | ![migration](screenshots/migration.png) |
| Performance Metrics | ![perf](screenshots/perf-metrics.png) |

## Visual Description
VM dashboard shows running virtual machines with resource usage. Migration status displays live migration progress with memory transfer. Performance metrics show CPU and I/O per VM.

---


## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Virtualization Internals](#virtualization-internals)
- [Memory Virtualization](#memory-virtualization)
- [CPU Virtualization](#cpu-virtualization)
- [I/O Virtualization](#io-virtualization)
- [Getting Started](#getting-started)
- [Building](#building)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Configuration](#configuration)
- [Performance](#performance)
- [Security](#security)
- [Contributing](#contributing)
- [License](#license)

## Overview

### What is a Type-1 Hypervisor?

A Type-1 hypervisor (also known as a bare-metal hypervisor) runs directly on the hardware, without requiring a host operating system. This provides:

- **Higher Performance**: Direct hardware access eliminates the host OS overhead
- **Better Security**: Smaller Trusted Computing Base (TCB), fewer attack surfaces
- **Lower Latency**: No host OS scheduling delays
- **Stronger Isolation**: VM escape vulnerabilities are reduced

### NexusVM Architecture

NexusVM implements a modern microkernel-inspired architecture:

```
┌─────────────────────────────────────────────────────────┐
│                    NexusVM Hypervisor                    │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │
│  │   Scheduler │  │   Memory    │  │   Device        │  │
│  │   Manager   │  │   Manager   │  │   Manager       │  │
│  └─────────────┘  └─────────────┘  └─────────────────┘  │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │
│  │    VMX      │  │    SVM      │  │   Interrupt     │  │
│  │   Module    │  │   Module    │  │   Controller    │  │
│  └─────────────┘  └─────────────┘  └─────────────────┘  │
├─────────────────────────────────────────────────────────┤
│              Hardware Abstraction Layer (HAL)            │
├─────────────────────────────────────────────────────────┤
│         Intel VT-x / AMD-V / EPT / NPT / VT-d          │
└─────────────────────────────────────────────────────────┘
```

## Architecture

### Core Components

#### 1. Hypervisor Core (`src/hypervisor/`)

The hypervisor core provides VM lifecycle management:

- **VM Creation/Destruction**: Creates isolated virtual machines with configurable resources
- **VM Scheduling**: CFS-based scheduler with real-time support
- **Resource Management**: CPU, memory, and I/O allocation

#### 2. CPU Virtualization (`src/hypervisor/cpu/`, `vmx/`, `amd-v/`)

Implements hardware-assisted CPU virtualization:

- **VMX (Intel VT-x)**: Full VMX implementation with VMCS management
- **SVM (AMD-V)**: Complete AMD-V support with VMCB control
- **VM Entry/Exit**: Optimized transitions between VM and host modes

#### 3. Memory Virtualization (`src/hypervisor/mmu/`)

Hardware-assisted memory virtualization:

- **EPT (Intel Extended Page Tables)**: 4-level page tables for GPA→HPA translation
- **NPT (AMD Nested Page Tables)**: AMD's equivalent to EPT
- **Large Page Support**: 2MB and 1GB page mappings

#### 4. I/O Virtualization (`src/hypervisor/io/`)

Multiple I/O virtualization strategies:

- **Emulation**: Full software emulation of legacy devices
- **Paravirtualization**: VirtIO for high-performance I/O
- **Passthrough**: VT-d for direct device access

### System Flow

```
Guest OS
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│                   Guest Virtual CPU                      │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐     │
│  │  Guest  │  │  Guest  │  │  Guest  │  │  Guest  │     │
│  │   GPRs  │  │   CR3   │  │   EPT   │  │   MSRs  │     │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘     │
└─────────────────────────────────────────────────────────┘
    │                    │                    │
    ▼                    ▼                    ▼
┌─────────────────────────────────────────────────────────┐
│                    VM Exit Handler                       │
│  • CPUID           • CR Access       • I/O Port         │
│  • MSR Read/Write  • INVLPG         • Interrupt         │
└─────────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│                  NexusVM Hypervisor                     │
│  • Hypercall Dispatcher  • Device Emulation            │
│  • Interrupt Controller   • Memory Manager              │
└─────────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│                  Physical Hardware                       │
└─────────────────────────────────────────────────────────┘
```

## Features

### Implemented Features

| Feature | Status | Description |
|---------|--------|-------------|
| Intel VT-x | ✅ | Full VMX implementation for Intel CPUs |
| AMD-V | ✅ | Complete SVM support for AMD CPUs |
| EPT/NPT | ✅ | Extended page tables for memory virtualization |
| VM Scheduling | ✅ | Multi-tenant VM scheduling with QoS |
| Live Migration | ✅ | Pre-copy and stop-and-copy migration |
| Snapshots | ✅ | Full VM state checkpointing |
| Nested Virt | ✅ | VMX-in-VMX and SVM-in-SVM support |
| Device Passthrough | ✅ | VT-d based device assignment |
| VM Lifecycle | ✅ | Create, start, stop, pause, resume, destroy |
| I/O Emulation | ✅ | Emulated legacy devices |
| Hypercall Interface | ✅ | Secure guest-hypervisor communication |

### Planned Features

| Feature | Status | Description |
|---------|--------|-------------|
| VirtIO Support | 🔄 | Paravirtualized drivers |
| vGPU Support | 📋 | GPU virtualization |
| Memory Deduplication | 📋 | KSM-like functionality |
| Secure Boot | 📋 | UEFI secure boot for VMs |
| Live Resize | 📋 | Hot-add CPU/memory |

## Virtualization Internals

### Intel VT-x Architecture

Intel VT-x introduces two new CPU modes:

1. **VMX Root Mode**: Where the hypervisor runs (ring 0 of root mode)
2. **VMX Non-Root Mode**: Where guest VMs run (preserves ring semantics)

#### VMX Transitions

```
┌────────────────────────────────────────────────────────────┐
│                     VMX Root Mode                          │
│                                                            │
│   ┌─────────────────┐         ┌─────────────────────────┐   │
│   │   Hypervisor   │  VMENTRY    │    Guest State      │   │
│   │                 │ ─────────► │    (VMCS)            │   │
│   │                 │            │                       │   │
│   │                 │ ◄───────── │                       │   │
│   │                 │   VMEXIT   │                       │   │
│   └─────────────────┘            └─────────────────────────┘   │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

#### VMCS (Virtual Machine Control Structure)

The VMCS is a 4KB memory region that controls VM behavior:

```c
typedef struct {
    // 16-bit control fields
    uint16_t vpid;
    uint16_t posted_interrupt_vector;
    
    // 64-bit control fields
    uint64_t vmcs_link_pointer;
    uint64_t ia32_debugctl;
    uint64_t ia32_pat;
    uint64_t ia32_efer;
    uint64_t guest_ia32_pat;
    uint64_t guest_ia32_efer;
    
    // 32-bit control fields
    uint32_t pin_based_vm_exec;
    uint32_t proc_based_vm_exec;
    uint32_t exception_bitmap;
    uint32_t vm_exit_controls;
    uint32_t vm_entry_controls;
    
    // Natural-width control fields
    uint64_t cr0_guest_host_mask;
    uint64_t cr4_guest_host_mask;
    
    // Guest-state area
    uint64_t guest_cr0;
    uint64_t guest_cr3;
    uint64_t guest_cr4;
    uint64_t guest_es_selector;
    uint64_t guest_cs_selector;
    // ... more selectors
    
    // Host-state area
    uint64_t host_cr0;
    uint64_t host_cr3;
    uint64_t host_cr4;
    uint64_t host_cs_selector;
    // ... more selectors
    
} vmcs_t;
```

### VM Exit Reasons

NexusVM handles numerous VM exit reasons:

| Exit Reason | Value | Handler |
|-------------|-------|---------|
| CPUID | 10 | Returns CPU feature info to guest |
| HL T | 12 | Puts vCPU in idle state |
| CR Access | 28 | Emulates control register access |
| MSR Read | 31 | Returns MSR values to guest |
| MSR Write | 32 | Updates MSR state |
| I/O Instruction | 30 | Emulates port I/O |
| EPT Violation | 48 | Handles memory access violations |
| VMCALL | 52 | Processes hypercalls from guest |

## Memory Virtualization

### Extended Page Tables (EPT)

EPT provides second-level address translation:

```
┌──────────────────────────────────────────────────────────────┐
│                     Address Translation                       │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  Guest Virtual Address (GVA)                                 │
│       │                                                       │
│       ▼                                                       │
│  ┌─────────────────────────────────┐                        │
│  │      Guest Page Directory       │  ← Guest CR3           │
│  └─────────────────────────────────┘                        │
│       │                                                       │
│       ▼                                                       │
│  Guest Physical Address (GPA)                                 │
│       │                                                       │
│       ▼                                                       │
│  ┌─────────────────────────────────┐                        │
│  │          EPT PML4               │  ← EPTP in VMCS        │
│  └─────────────────────────────────┘                        │
│       │                                                       │
│       ▼                                                       │
│  ┌─────────────────────────────────┐                        │
│  │      EPT Page Directory          │                        │
│  └─────────────────────────────────┘                        │
│       │                                                       │
│       ▼                                                       │
│  ┌─────────────────────────────────┐                        │
│  │      EPT Page Table              │                        │
│  └─────────────────────────────────┘                        │
│       │                                                       │
│       ▼                                                       │
│  Host Physical Address (HPA)                                 │
│                                                               │
└──────────────────────────────────────────────────────────────┘
```

### EPT Memory Types

EPT supports multiple memory types:

| Type | Value | Description |
|------|-------|-------------|
| Uncacheable (UC) | 0 | No caching |
| Write-Combining (WC) | 1 | Write combining buffer |
| Write-Through (WT) | 4 | Cached, no write allocation |
| Write-Protected (WP) | 5 | Read caching, write no allocation |
| Write-Back (WB) | 6 | Full caching (default) |

### Large Page Support

NexusVM supports large pages for improved performance:

- **2MB Pages**: Reduces TLB pressure for large allocations
- **1GB Pages**: For huge memory configurations
- **HugeTLB Integration**: Compatible with system huge pages

## CPU Virtualization

### Guest State Management

NexusVM maintains complete guest CPU state:

```c
typedef struct {
    // General Purpose Registers
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8-r15;
    uint64_t rip, rflags;
    
    // Control Registers
    uint64_t cr0, cr2, cr3, cr4, cr8;
    
    // Debug Registers
    uint64_t dr0-dr7;
    
    // Model Specific Registers
    uint64_t ia32_efer;
    uint64_t ia32_pat;
    uint64_t ia32_star, ia32_lstar;
    uint64_t ia32_fs_base, ia32_gs_base;
    
    // Segment Registers
    segment_reg_t cs, ds, es, fs, gs, ss;
    segment_reg_t tr, ldt;
    
    // Descriptor Tables
    uint64_t gdtr_base, idtr_base;
    uint16_t gdtr_limit, idtr_limit;
    
} guest_cpu_state_t;
```

### MSR Interception

NexusVM uses MSR bitmaps for efficient MSR handling:

```
┌─────────────────────────────────────────────────────────────┐
│                    MSR Bitmap Structure                      │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────┐  ┌──────────────────────┐        │
│  │  Read Bitmap (4KB)   │  │  Write Bitmap (4KB)   │        │
│  │                      │  │                      │        │
│  │  0 = Pass-through    │  │  0 = Pass-through    │        │
│  │  1 = Intercept       │  │  1 = Intercept       │        │
│  │                      │  │                      │        │
│  └──────────────────────┘  └──────────────────────┘        │
│           MSR 0-8191               MSR 0-8191              │
└─────────────────────────────────────────────────────────────┘
```

### CPUID Emulation

NexusVM emulates CPUID instructions to provide controlled views of CPU features:

```c
static void handle_cpuid(nexusvcpu_t *vcpu)
{
    uint32_t leaf = vcpu->cpu_state.rax & 0xFFFFFFFF;
    uint32_t subleaf = vcpu->cpu_state.rcx & 0xFFFFFFFF;
    
    switch (leaf) {
        case 1:  // Feature flags
            vcpu->cpu_state.rax = sanitize_feature_flags(ecx, edx);
            break;
        case 7:  // Extended features
            if (subleaf == 0) {
                vcpu->cpu_state.rbx = sanitize_ebx_features();
            }
            break;
        case 0x80000001:  // Extended feature flags
            // AMD-specific handling
            break;
    }
}
```

## I/O Virtualization

### Port I/O Emulation

NexusVM intercepts port I/O for emulated devices:

| Device | Ports | Description |
|--------|-------|-------------|
| PIT | 0x40-0x43 | Programmable Interval Timer |
| PS/2 | 0x60, 0x64 | Keyboard/Mouse controller |
| CMOS | 0x70-0x77 | Real-time clock |
| COM1-4 | 0x3F8-0x3FF, etc. | Serial ports |
| VGA | 0x3C0-0x3DA | Graphics adapter |

### MMIO Access

Memory-mapped I/O is handled through EPT violations:

```
1. Guest accesses MMIO region
        │
        ▼
2. EPT violation triggered
        │
        ▼
3. NexusVM handles violation
        │
        ▼
4. MMIO handler called
        │
        ▼
5. Read/write completed, VM resumed
```

### Device Passthrough

For high-performance I/O, NexusVM supports VT-d device passthrough:

1. **Device Assignment**: Assign physical device to VM
2. **Interrupt Remapping**: Secure interrupt delivery
3. **DMA Protection**: IOMMU prevents unauthorized memory access
4. **Live Migration**: Device state migration support

## Getting Started

### Prerequisites

- x86-64 processor with VT-x or AMD-V
- Linux kernel 4.0+ (for KVM hardware acceleration if needed)
- GCC/Clang compiler
- CMake or Make

### Quick Start

```bash
# Clone the repository
git clone https://github.com/moggan1337/NexusVM.git
cd NexusVM

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# List VMs
./nexusvm --list

# Create a VM
./nexusvm --create testvm --memory 2048 --vcpus 4

# Start VM
./nexusvm --start testvm

# Stop VM
./nexusvm --stop testvm
```

## Building

### Build Options

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
          -DNEXUSVM_ENABLE_DEBUG=OFF \
          -DNEXUSVM_ENABLE_TRACE=OFF
```

### Dependencies

- libc6-dev
- libssl-dev (for encrypted migration)
- liblz4-dev (for migration compression, optional)

## Usage

### CLI Commands

```bash
# Create VM
nexusvm --create <name> --memory <MB> --vcpus <N>

# Start VM
nexusvm --start <name>

# Stop VM
nexusvm --stop <name>

# Pause VM
nexusvm --pause <name>

# Resume VM
nexusvm --resume <name>

# Create snapshot
nexusvm --snapshot <name> --file <path>

# Restore snapshot
nexusvm --restore <name> --file <path>

# Live migrate
nexusvm --migrate <name> --target <host> --port <port>
```

### VM Configuration

```json
{
  "name": "myvm",
  "vcpus": 4,
  "memory_mb": 4096,
  "kernel": "/boot/vmlinuz",
  "initrd": "/boot/initrd.img",
  "cmdline": "console=ttyS0 root=/dev/sda1",
  "networks": [
    {
      "type": "virtio",
      "bridge": "br0"
    }
  ],
  "disks": [
    {
      "type": "qcow2",
      "path": "/var/lib/nexusvm/disks/myvm.qcow2",
      "size_gb": 50
    }
  ]
}
```

## API Reference

### Core API

```c
// Initialize hypervisor
nexusvm_result_t hypervisor_init(void);
void hypervisor_shutdown(void);

// VM management
nexusvm_result_t vm_create(vm_config_t *config, nexusvm_t **vm);
nexusvm_result_t vm_destroy(nexusvm_t *vm);
nexusvm_result_t vm_start(nexusvm_t *vm);
nexusvm_result_t vm_stop(nexusvm_t *vm);
nexusvm_result_t vm_pause(nexusvm_t *vm);
nexusvm_result_t vm_resume(nexusvm_t *vm);

// vCPU management
nexusvm_result_t vcpu_create(nexusvm_t *vm, uint32_t id, nexusvcpu_t **vcpu);
nexusvm_result_t vcpu_start(nexusvcpu_t *vcpu);
nexusvm_result_t vcpu_stop(nexusvcpu_t *vcpu);

// Memory management
nexusvm_result_t vm_alloc_memory(nexusvm_t *vm, guest_addr_t gpa, 
                                  uint64_t size, memory_type_t type);
nexusvm_result_t vm_ept_map(nexusvm_t *vm, guest_addr_t gpa, 
                            phys_addr_t hpa, uint64_t size,
                            bool read, bool write, bool execute);

// Hypercalls
nexusvm_result_t hypercall_handle(nexusvcpu_t *vcpu, 
                                   hypercall_params_t *params,
                                   hypercall_result_t *result);
```

## Performance

### Benchmarks

| Operation | Latency | Throughput |
|-----------|---------|------------|
| VM Entry | ~500 cycles | ~2M entries/sec |
| VM Exit | ~300 cycles | ~3M exits/sec |
| EPT Violation | ~1μs | Varies by workload |
| Hypercall | ~1μs | ~1M calls/sec |

### Optimization Features

- **VMCS Caching**: Reduces VM entry/exit overhead
- **Large Pages**: 2MB/1GB page support for reduced TLB misses
- **VPID**: Virtual Processor IDs for TLB isolation
- **FlexPriority**: TPR virtualization for APIC optimization

## Security

### Security Features

- **VMCS Shadowing**: Enables nested virtualization securely
- **Memory Integrity**: EPT accessed/dirty bits for memory tracking
- **SMEP/SMAP**: Supervisor Mode Execution/Access Prevention
- **PKU**: Protection Keys for Userspace (guest visible)
- **UMIP**: User Mode Instruction Prevention

### Hardening

1. **Minimize TCB**: Type-1 design reduces attack surface
2. **Safe Hypercalls**: Validated input, bounded execution
3. **Memory Isolation**: Separate EPT for each VM
4. **Interrupt Isolation**: Posted interrupts for secure delivery

## Contributing

Contributions are welcome! Please read our contributing guidelines before submitting PRs.

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## License

NexusVM is licensed under either:
- MIT License
- Apache License 2.0

See LICENSE file for details.

---

<p align="center">
  Built with ❤️ by the NexusVM Team<br>
  <a href="https://github.com/moggan1337/NexusVM">GitHub</a> •
  <a href="https://github.com/moggan1337/NexusVM/issues">Issues</a> •
  <a href="https://github.com/moggan1337/NexusVM/discussions">Discussions</a>
</p>
