/**
 * NexusVM - Type-1 Hypervisor
 * Main Entry Point
 */

#include <hypervisor/hypervisor.h>
#include <utils/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

/* Command line options */
typedef struct {
    bool verbose;
    bool daemon;
    char *config_file;
    char *vm_name;
    bool list_vms;
    bool create_vm;
    bool destroy_vm;
    bool start_vm;
    bool stop_vm;
    u32 vm_memory;
    u32 vm_vcpus;
} nexusvm_options_t;

static nexusvm_options_t g_options;

/* Signal handler */
static volatile bool g_running = true;

static void signal_handler(int sig)
{
    (void)sig;
    NEXUSVM_LOG_INFO("Received signal, shutting down...\n");
    g_running = false;
}

/* Print usage */
static void print_usage(const char *prog)
{
    printf("NexusVM - Type-1 Hypervisor\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --verbose           Enable verbose output\n");
    printf("  -d, --daemon            Run as daemon\n");
    printf("  -c, --config <file>     Specify configuration file\n");
    printf("  -l, --list              List all VMs\n");
    printf("  --create <name>         Create a new VM\n");
    printf("  --destroy <name>        Destroy a VM\n");
    printf("  --start <name>         Start a VM\n");
    printf("  --stop <name>          Stop a VM\n");
    printf("  --memory <MB>          VM memory in MB (default: 1024)\n");
    printf("  --vcpus <N>            Number of vCPUs (default: 2)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --create myvm --memory 2048 --vcpus 4\n", prog);
    printf("  %s --start myvm\n", prog);
    printf("  %s --list\n", prog);
}

/* Parse command line */
static void parse_args(int argc, char **argv)
{
    memset(&g_options, 0, sizeof(g_options));
    g_options.vm_memory = 1024;
    g_options.vm_vcpus = 2;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"daemon", no_argument, 0, 'd'},
        {"config", required_argument, 0, 'c'},
        {"list", no_argument, 0, 'l'},
        {"create", required_argument, 0, 1000},
        {"destroy", required_argument, 0, 1001},
        {"start", required_argument, 0, 1002},
        {"stop", required_argument, 0, 1003},
        {"memory", required_argument, 0, 1004},
        {"vcpus", required_argument, 0, 1005},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hvd:c:l", 
                            long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case 'v':
                g_options.verbose = true;
                break;
            case 'd':
                g_options.daemon = true;
                break;
            case 'c':
                g_options.config_file = optarg;
                break;
            case 'l':
                g_options.list_vms = true;
                break;
            case 1000: /* --create */
                g_options.create_vm = true;
                g_options.vm_name = optarg;
                break;
            case 1001: /* --destroy */
                g_options.destroy_vm = true;
                g_options.vm_name = optarg;
                break;
            case 1002: /* --start */
                g_options.start_vm = true;
                g_options.vm_name = optarg;
                break;
            case 1003: /* --stop */
                g_options.stop_vm = true;
                g_options.vm_name = optarg;
                break;
            case 1004: /* --memory */
                g_options.vm_memory = atoi(optarg);
                break;
            case 1005: /* --vcpus */
                g_options.vm_vcpus = atoi(optarg);
                break;
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }
}

/* List all VMs */
static void list_vms(void)
{
    u32 vm_ids[256];
    u32 count = vm_enumerate(vm_ids, 256);
    
    if (count == 0) {
        printf("No VMs found.\n");
        return;
    }
    
    printf("ID   Name                    State       vCPUs  Memory\n");
    printf("-------------------------------------------------------------\n");
    
    for (u32 i = 0; i < count; i++) {
        nexusvm_t *vm = vm_get_by_id(vm_ids[i]);
        if (vm) {
            const char *state_str = "Unknown";
            switch (vm->state) {
                case VM_STATE_CREATED: state_str = "Created"; break;
                case VM_STATE_INITIALIZED: state_str = "Initialized"; break;
                case VM_STATE_RUNNING: state_str = "Running"; break;
                case VM_STATE_PAUSED: state_str = "Paused"; break;
                case VM_STATE_SUSPENDED: state_str = "Suspended"; break;
                case VM_STATE_MIGRATING: state_str = "Migrating"; break;
                case VM_STATE_SHUTDOWN: state_str = "Shutdown"; break;
                default: break;
            }
            
            printf("%-4u %-24s %-11s %-6u %-7lu MB\n",
                   vm->id, vm->name, state_str, vm->vcpu_count,
                   vm->memory_size / (1024 * 1024));
        }
    }
}

/* Create a VM */
static int create_vm(const char *name, u32 memory_mb, u32 vcpus)
{
    vm_config_t config;
    memset(&config, 0, sizeof(config));
    
    strncpy(config.name, name, sizeof(config.name) - 1);
    config.vcpu_count = vcpus;
    config.memory_size = (u64)memory_mb * 1024 * 1024;
    config.kernel_entry = 0x100000;
    config.long_mode = true;
    config.apic = true;
    
    nexusvm_t *vm;
    nexusvm_result_t result = vm_create(&config, &vm);
    
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to create VM: %d\n", result);
        return 1;
    }
    
    NEXUSVM_LOG_INFO("VM '%s' created successfully (ID: %u)\n", name, vm->id);
    return 0;
}

/* Start a VM */
static int start_vm(const char *name)
{
    nexusvm_t *vm = vm_get_by_name(name);
    if (!vm) {
        NEXUSVM_LOG_ERROR("VM '%s' not found\n", name);
        return 1;
    }
    
    nexusvm_result_t result = vm_start(vm);
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to start VM: %d\n", result);
        return 1;
    }
    
    NEXUSVM_LOG_INFO("VM '%s' started successfully\n", name);
    return 0;
}

/* Stop a VM */
static int stop_vm(const char *name)
{
    nexusvm_t *vm = vm_get_by_name(name);
    if (!vm) {
        NEXUSVM_LOG_ERROR("VM '%s' not found\n", name);
        return 1;
    }
    
    nexusvm_result_t result = vm_stop(vm);
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to stop VM: %d\n", result);
        return 1;
    }
    
    NEXUSVM_LOG_INFO("VM '%s' stopped successfully\n", name);
    return 0;
}

/* Destroy a VM */
static int destroy_vm(const char *name)
{
    nexusvm_t *vm = vm_get_by_name(name);
    if (!vm) {
        NEXUSVM_LOG_ERROR("VM '%s' not found\n", name);
        return 1;
    }
    
    nexusvm_result_t result = vm_destroy(vm);
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to destroy VM: %d\n", result);
        return 1;
    }
    
    NEXUSVM_LOG_INFO("VM '%s' destroyed successfully\n", name);
    return 0;
}

/* Main function */
int main(int argc, char **argv)
{
    parse_args(argc, argv);
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize hypervisor */
    NEXUSVM_LOG_INFO("NexusVM Type-1 Hypervisor v0.1.0\n");
    NEXUSVM_LOG_INFO("=================================\n\n");
    
    nexusvm_result_t result = hypervisor_init();
    if (result != NEXUSVM_OK) {
        NEXUSVM_LOG_ERROR("Failed to initialize hypervisor: %d\n", result);
        return 1;
    }
    
    int ret = 0;
    
    /* Handle commands */
    if (g_options.list_vms) {
        list_vms();
    } else if (g_options.create_vm && g_options.vm_name) {
        ret = create_vm(g_options.vm_name, g_options.vm_memory, g_options.vm_vcpus);
    } else if (g_options.start_vm && g_options.vm_name) {
        ret = start_vm(g_options.vm_name);
    } else if (g_options.stop_vm && g_options.vm_name) {
        ret = stop_vm(g_options.vm_name);
    } else if (g_options.destroy_vm && g_options.vm_name) {
        ret = destroy_vm(g_options.vm_name);
    } else {
        /* Interactive or daemon mode */
        if (g_options.daemon) {
            NEXUSVM_LOG_INFO("Running in daemon mode...\n");
            /* Daemonize would go here */
        } else {
            NEXUSVM_LOG_INFO("No command specified. Use --help for usage.\n");
            list_vms();
        }
    }
    
    /* Main loop when running interactively */
    while (g_running && !g_options.daemon) {
        sleep(1);
    }
    
    /* Cleanup */
    hypervisor_shutdown();
    
    return ret;
}
