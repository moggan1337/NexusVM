/**
 * NexusVM I/O Emulation
 */

#include <hypervisor/io/io_emul.h>
#include <hypervisor/hypervisor.h>
#include <utils/log.h>
#include <string.h>
#include <stdlib.h>

/* I/O handler entry */
typedef struct io_handler {
    u16 port;
    u16 count;
    nexusvm_result_t (*handler)(nexusvcpu_t *, u16, u32, u64 *);
    void *context;
    struct io_handler *next;
} io_handler_t;

/* MMIO handler entry */
typedef struct mmio_handler {
    u64 start;
    u64 end;
    nexusvm_result_t (*handler)(nexusvcpu_t *, u64, u32, u64 *);
    void *context;
    struct mmio_handler *next;
} mmio_handler_t;

static io_handler_t *g_io_handlers = NULL;
static mmio_handler_t *g_mmio_handlers = NULL;
static bool g_io_initialized = false;

/*============================================================================
 * I/O Emulation Initialization
 *============================================================================*/

nexusvm_result_t io_emul_init(void)
{
    NEXUSVM_LOG_INFO("Initializing I/O emulation\n");
    
    g_io_handlers = NULL;
    g_mmio_handlers = NULL;
    g_io_initialized = true;
    
    /* Register default handlers */
    /* CMOS/RTC - ports 0x70-0x77 */
    io_register_handler(0x70, 8, cmos_read, cmos_write, NULL);
    
    /* PS/2 Keyboard - ports 0x60, 0x64 */
    io_register_handler(0x60, 2, ps2_keyboard_read, ps2_keyboard_write, NULL);
    
    /* PIT - ports 0x40-0x43 */
    io_register_handler(0x40, 4, pit_read, pit_write, NULL);
    
    /* COM1 - ports 0x3F8-0x3FF */
    io_register_handler(0x3F8, 8, uart_read, uart_write, NULL);
    
    /* VGA - ports 0x3C0-0x3DA */
    io_register_handler(0x3C0, 0x3DA - 0x3C0 + 1, vga_read, vga_write, NULL);
    
    NEXUSVM_LOG_INFO("I/O emulation initialized\n");
    
    return NEXUSVM_OK;
}

void io_emul_shutdown(void)
{
    /* Free handlers */
    io_handler_t *io = g_io_handlers;
    while (io) {
        io_handler_t *next = io->next;
        free(io);
        io = next;
    }
    
    mmio_handler_t *mmio = g_mmio_handlers;
    while (mmio) {
        mmio_handler_t *next = mmio->next;
        free(mmio);
        mmio = next;
    }
    
    g_io_initialized = false;
    NEXUSVM_LOG_INFO("I/O emulation shutdown\n");
}

/*============================================================================
 * I/O Port Access
 *============================================================================*/

nexusvm_result_t io_port_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value)
{
    if (!vcpu || !value) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    *value = 0;
    
    /* Find handler */
    io_handler_t *handler = g_io_handlers;
    while (handler) {
        if (port >= handler->port && port < handler->port + handler->count) {
            return handler->handler(vcpu, port, size, value);
        }
        handler = handler->next;
    }
    
    /* No handler - return 0xFF for reads (common behavior) */
    switch (size) {
        case 1: *value = 0xFF; break;
        case 2: *value = 0xFFFF; break;
        case 4: *value = 0xFFFFFFFF; break;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t io_port_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    /* Find handler */
    io_handler_t *handler = g_io_handlers;
    while (handler) {
        if (port >= handler->port && port < handler->port + handler->count) {
            /* Handler returns NEXUSVM_OK if handled, we use a different approach */
            return handler->handler(vcpu, port, size, (u64 *)value);
        }
        handler = handler->next;
    }
    
    /* No handler - ignore write */
    return NEXUSVM_OK;
}

/*============================================================================
 * Port I/O Handlers
 *============================================================================*/

nexusvm_result_t io_register_handler(u16 port, u16 count,
                                      nexusvm_result_t (*handler)(nexusvcpu_t *, u16, u32, u64 *),
                                      void *context)
{
    io_handler_t *entry = (io_handler_t *)malloc(sizeof(io_handler_t));
    if (!entry) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    entry->port = port;
    entry->count = count;
    entry->handler = handler;
    entry->context = context;
    entry->next = g_io_handlers;
    g_io_handlers = entry;
    
    return NEXUSVM_OK;
}

nexusvm_result_t io_unregister_handler(u16 port)
{
    io_handler_t **prev = &g_io_handlers;
    io_handler_t *entry = g_io_handlers;
    
    while (entry) {
        if (entry->port == port) {
            *prev = entry->next;
            free(entry);
            return NEXUSVM_OK;
        }
        prev = &entry->next;
        entry = entry->next;
    }
    
    return NEXUSVM_ERR_NOT_FOUND;
}

/*============================================================================
 * Common I/O Port Emulation
 *============================================================================*/

/* CMOS state */
static u8 cmos_rtc[128] = {0};
static u8 cmos_index = 0;

nexusvm_result_t cmos_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value)
{
    if (port == 0x70) {
        /* CMOS index register */
        *value = cmos_index;
    } else if (port == 0x71) {
        /* CMOS data register */
        if (cmos_index < 128) {
            *value = cmos_rtc[cmos_index];
        } else {
            *value = 0xFF;
        }
    } else {
        *value = 0xFF;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t cmos_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value)
{
    if (port == 0x70) {
        cmos_index = value & 0x7F;
    } else if (port == 0x71) {
        if (cmos_index < 128) {
            cmos_rtc[cmos_index] = value;
        }
    }
    
    return NEXUSVM_OK;
}

/* PS/2 Keyboard state */
static u8 ps2_status = 0;
static u8 ps2_output = 0;

nexusvm_result_t ps2_keyboard_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value)
{
    if (port == 0x60) {
        *value = ps2_output;
        ps2_output = 0;
    } else if (port == 0x64) {
        *value = ps2_status;
    } else {
        *value = 0xFF;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t ps2_keyboard_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value)
{
    if (port == 0x60) {
        /* Keyboard command byte */
        ps2_status = value;
    } else if (port == 0x64) {
        /* Keyboard controller command */
    }
    
    return NEXUSVM_OK;
}

/* PIT state */
static u16 pit_counter[3] = {0, 0, 0};
static u8 pit_mode[3] = {0, 0, 0};

nexusvm_result_t pit_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value)
{
    int counter = -1;
    
    if (port >= 0x40 && port <= 0x42) {
        counter = port - 0x40;
    }
    
    if (counter >= 0) {
        *value = pit_counter[counter];
    } else {
        *value = 0;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t pit_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value)
{
    int counter = -1;
    
    if (port >= 0x40 && port <= 0x42) {
        counter = port - 0x40;
    } else if (port == 0x43) {
        /* PIT mode/command register */
        int c = (value >> 6) & 3;
        pit_mode[c] = value;
        return NEXUSVM_OK;
    }
    
    if (counter >= 0) {
        pit_counter[counter] = value;
    }
    
    return NEXUSVM_OK;
}

/* UART state */
static u8 uart_regs[8] = {0};

nexusvm_result_t uart_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value)
{
    int reg = port & 7;
    
    if (reg < 8) {
        *value = uart_regs[reg];
    } else {
        *value = 0xFF;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t uart_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value)
{
    int reg = port & 7;
    
    if (reg < 8) {
        uart_regs[reg] = value;
    }
    
    return NEXUSVM_OK;
}

/* VGA state */
static u8 vga_attr_index = 0;
static u8 vga_attr_data = 0;
static u8 vga_misc_output = 0;

nexusvm_result_t vga_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value)
{
    if (port >= 0x3C0 && port <= 0x3DF) {
        *value = (port == 0x3DA) ? 0x00 : 0x00;
    } else {
        *value = 0x00;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t vga_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value)
{
    if (port == 0x3C0 || port == 0x3C2) {
        vga_attr_index = value;
    } else if (port == 0x3DA) {
        vga_misc_output = value;
    }
    
    return NEXUSVM_OK;
}

/*============================================================================
 * PCI Configuration Access
 *============================================================================*/

nexusvm_result_t pci_config_read(u8 bus, u8 dev, u8 func, u8 reg, u32 size, u32 *value)
{
    if (!value) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    *value = 0xFFFFFFFF; /* Default: no device */
    
    /* Simple PCI device enumeration */
    /* In a real hypervisor, this would query actual PCI topology */
    
    return NEXUSVM_OK;
}

nexusvm_result_t pci_config_write(u8 bus, u8 dev, u8 func, u8 reg, u32 size, u32 value)
{
    return NEXUSVM_OK;
}

/*============================================================================
 * MMIO Access
 *============================================================================*/

nexusvm_result_t mmio_read(nexusvcpu_t *vcpu, u64 addr, u32 size, u64 *value)
{
    if (!vcpu || !value) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    *value = 0;
    
    mmio_handler_t *handler = g_mmio_handlers;
    while (handler) {
        if (addr >= handler->start && addr < handler->end) {
            return handler->handler(vcpu, addr, size, value);
        }
        handler = handler->next;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t mmio_write(nexusvcpu_t *vcpu, u64 addr, u32 size, u64 value)
{
    if (!vcpu) {
        return NEXUSVM_ERR_INVALID_PARAM;
    }
    
    mmio_handler_t *handler = g_mmio_handlers;
    while (handler) {
        if (addr >= handler->start && addr < handler->end) {
            return handler->handler(vcpu, addr, size, (u64 *)value);
        }
        handler = handler->next;
    }
    
    return NEXUSVM_OK;
}

nexusvm_result_t mmio_register_handler(u64 start, u64 end,
                                       nexusvm_result_t (*handler)(nexusvcpu_t *, u64, u32, u64 *),
                                       void *context)
{
    mmio_handler_t *entry = (mmio_handler_t *)malloc(sizeof(mmio_handler_t));
    if (!entry) {
        return NEXUSVM_ERR_NO_MEMORY;
    }
    
    entry->start = start;
    entry->end = end;
    entry->handler = handler;
    entry->context = context;
    entry->next = g_mmio_handlers;
    g_mmio_handlers = entry;
    
    return NEXUSVM_OK;
}

/*============================================================================
 * Interrupt Request (IRQ) Management
 *============================================================================*/

/* IRQ state per VM */
static bool g_irq_lines[256] = {false};

void io_irq_set(nexusvm_t *vm, u32 irq, bool level)
{
    if (irq < 256) {
        g_irq_lines[irq] = level;
        
        if (level) {
            NEXUSVM_LOG_DEBUG("IRQ %u asserted\n", irq);
        }
    }
}

bool io_irq_get(nexusvm_t *vm, u32 irq)
{
    if (irq < 256) {
        return g_irq_lines[irq];
    }
    return false;
}

void io_generate_interrupt(nexusvcpu_t *vcpu, u8 vector)
{
    if (!vcpu) return;
    
    NEXUSVM_LOG_DEBUG("Generating interrupt vector %u\n", vector);
    
    /* Would inject interrupt into vCPU via VMCS/VMCB */
}
