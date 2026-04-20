/**
 * NexusVM I/O Emulation
 */

#ifndef NEXUSVM_IO_EMUL_H
#define NEXUSVM_IO_EMUL_H

#include <hypervisor/types.h>
#include <hypervisor/hypervisor.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * I/O Emulation Initialization
 *============================================================================*/

/**
 * Initialize I/O emulation subsystem
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t io_emul_init(void);

/**
 * Shutdown I/O emulation
 */
void io_emul_shutdown(void);

/*============================================================================
 * I/O Port Access
 *============================================================================*/

/**
 * Handle I/O port read
 * @param vcpu vCPU performing read
 * @param port I/O port
 * @param size Size in bytes (1, 2, 4)
 * @param value Output value
 * @return NEXUSVM_OK if handled
 */
nexusvm_result_t io_port_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value);

/**
 * Handle I/O port write
 * @param vcpu vCPU performing write
 * @param port I/O port
 * @param size Size in bytes
 * @param value Value to write
 * @return NEXUSVM_OK if handled
 */
nexusvm_result_t io_port_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value);

/*============================================================================
 * Port I/O Handlers
 *============================================================================*/

/**
 * Register I/O port handler
 * @param port Start port
 * @param count Number of ports
 * @param handler Handler function
 * @param context Handler context
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t io_register_handler(u16 port, u16 count,
                                      nexusvm_result_t (*handler)(nexusvcpu_t *, u16, u32, u64 *),
                                      void *context);

/**
 * Unregister I/O port handler
 * @param port Start port
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t io_unregister_handler(u16 port);

/*============================================================================
 * Common I/O Port Emulation
 *============================================================================*/

/**
 * CMOS/RTC emulation
 */
nexusvm_result_t cmos_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value);
nexusvm_result_t cmos_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value);

/**
 * Keyboard controller emulation
 */
nexusvm_result_t ps2_keyboard_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value);
nexusvm_result_t ps2_keyboard_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value);

/**
 * PIT (Programmable Interval Timer) emulation
 */
nexusvm_result_t pit_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value);
nexusvm_result_t pit_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value);

/**
 * Serial port (UART) emulation
 */
nexusvm_result_t uart_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value);
nexusvm_result_t uart_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value);

/**
 * VGA emulation
 */
nexusvm_result_t vga_read(nexusvcpu_t *vcpu, u16 port, u32 size, u64 *value);
nexusvm_result_t vga_write(nexusvcpu_t *vcpu, u16 port, u32 size, u64 value);

/*============================================================================
 * PCI Configuration Access
 *============================================================================*/

/**
 * Handle PCI config space read
 * @param bus PCI bus
 * @param dev PCI device
 * @param func PCI function
 * @param reg Register offset
 * @param size Size in bytes
 * @param value Output value
 * @return NEXUSVM_OK if handled
 */
nexusvm_result_t pci_config_read(u8 bus, u8 dev, u8 func, u8 reg, u32 size, u32 *value);

/**
 * Handle PCI config space write
 * @param bus PCI bus
 * @param dev PCI device
 * @param func PCI function
 * @param reg Register offset
 * @param size Size in bytes
 * @param value Value to write
 * @return NEXUSVM_OK if handled
 */
nexusvm_result_t pci_config_write(u8 bus, u8 dev, u8 func, u8 reg, u32 size, u32 value);

/*============================================================================
 * MMIO Access
 *============================================================================*/

/**
 * Handle MMIO read
 * @param vcpu vCPU performing read
 * @param addr Memory address
 * @param size Size in bytes
 * @param value Output value
 * @return NEXUSVM_OK if handled
 */
nexusvm_result_t mmio_read(nexusvcpu_t *vcpu, u64 addr, u32 size, u64 *value);

/**
 * Handle MMIO write
 * @param vcpu vCPU performing write
 * @param addr Memory address
 * @param size Size in bytes
 * @param value Value to write
 * @return NEXUSVM_OK if handled
 */
nexusvm_result_t mmio_write(nexusvcpu_t *vcpu, u64 addr, u32 size, u64 value);

/**
 * Register MMIO region handler
 * @param start Start address
 * @param end End address
 * @param handler Handler function
 * @param context Handler context
 * @return NEXUSVM_OK on success
 */
nexusvm_result_t mmio_register_handler(u64 start, u64 end,
                                       nexusvm_result_t (*handler)(nexusvcpu_t *, u64, u32, u64 *),
                                       void *context);

/*============================================================================
 * Interrupt Request (IRQ) Management
 *============================================================================*/

/**
 * Raise IRQ line
 * @param vm VM
 * @param irq IRQ number
 * @param level True to assert, false to deassert
 */
void io_irq_set(nexusvm_t *vm, u32 irq, bool level);

/**
 * Get IRQ line status
 * @param vm VM
 * @param irq IRQ number
 * @return Current level
 */
bool io_irq_get(nexusvm_t *vm, u32 irq);

/**
 * Generate interrupt
 * @param vcpu vCPU to interrupt
 * @param vector Interrupt vector
 */
void io_generate_interrupt(nexusvcpu_t *vcpu, u8 vector);

#ifdef __cplusplus
}
#endif

#endif /* NEXUSVM_IO_EMUL_H */
