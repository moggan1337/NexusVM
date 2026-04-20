# NexusVM Build System

CC = gcc
CXX = g++
AR = ar
RANLIB = ranlib

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

# Flags
CFLAGS = -Wall -Wextra -O2 -g
CFLAGS += -I$(INCLUDE_DIR)
CFLAGS += -I$(INCLUDE_DIR)/hypervisor
CFLAGS += -I$(INCLUDE_DIR)/kernel
CFLAGS += -I$(INCLUDE_DIR)/mmu
CFLAGS += -I$(INCLUDE_DIR)/cpu
CFLAGS += -I$(INCLUDE_DIR)/io
CFLAGS += -I$(INCLUDE_DIR)/vmx
CFLAGS += -I$(INCLUDE_DIR)/amd-v
CFLAGS += -I$(INCLUDE_DIR)/sched
CFLAGS += -I$(INCLUDE_DIR)/migration
CFLAGS += -I$(INCLUDE_DIR)/checkpoint
CFLAGS += -I$(INCLUDE_DIR)/nested
CFLAGS += -I$(INCLUDE_DIR)/pci
CFLAGS += -I$(INCLUDE_DIR)/utils
CFLAGS += -D__x86_64__
CFLAGS += -fno-strict-aliasing
CFLAGS += -fno-common

LDFLAGS = -lpthread -ldl -lm

# Source files
HYPERVISOR_SRC = $(SRC_DIR)/hypervisor/hypervisor.c
VMX_SRC = $(SRC_DIR)/hypervisor/vmx/vmx.c
SVM_SRC = $(SRC_DIR)/hypervisor/amd-v/svm.c
EPT_SRC = $(SRC_DIR)/hypervisor/mmu/ept.c
SCHED_SRC = $(SRC_DIR)/hypervisor/sched/scheduler.c
MIGRATION_SRC = $(SRC_DIR)/hypervisor/migration/migration.c
CHECKPOINT_SRC = $(SRC_DIR)/hypervisor/checkpoint/checkpoint.c
NESTED_SRC = $(SRC_DIR)/hypervisor/nested/nested_virt.c
IO_SRC = $(SRC_DIR)/hypervisor/io/io_emul.c
UTILS_SRC = $(SRC_DIR)/utils/log.c

OBJECTS = $(HYPERVISOR_SRC:.c=.o) \
          $(VMX_SRC:.c=.o) \
          $(SVM_SRC:.c=.o) \
          $(EPT_SRC:.c=.o) \
          $(SCHED_SRC:.c=.o) \
          $(MIGRATION_SRC:.c=.o) \
          $(CHECKPOINT_SRC:.c=.o) \
          $(NESTED_SRC:.c=.o) \
          $(IO_SRC:.c=.o) \
          $(UTILS_SRC:.c=.o)

ASM_OBJECTS = $(SRC_DIR)/vmx/asm.S.o

TARGET = nexusvm

.PHONY: all clean install docs test

all: $(TARGET)

$(TARGET): $(OBJECTS) $(ASM_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SRC_DIR)/vmx/asm.S.o: $(SRC_DIR)/vmx/asm.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(HYPERVISOR_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(VMX_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(SVM_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(EPT_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(SCHED_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(MIGRATION_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(CHECKPOINT_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(NESTED_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(IO_SRC:.c=.o): $(INCLUDE_DIR)/hypervisor/*.h
$(UTILS_SRC:.c=.o): $(INCLUDE_DIR)/utils/*.h

clean:
	rm -f $(OBJECTS) $(ASM_OBJECTS) $(TARGET)
	rm -rf $(BUILD_DIR)

install: $(TARGET)
	install -D $(TARGET) /usr/local/bin/$(TARGET)

test: $(TARGET)
	@echo "Running tests..."
	@echo "Note: Full tests require root privileges for VMX operations"

docs:
	@echo "Building documentation..."
	@mkdir -p docs/html
	# Add documentation build commands here

.DEFAULT_GOAL := all
