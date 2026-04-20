# NexusVM VMX Assembly Routines
# These are needed for VMX operations that can't be done in C

.text
.globl vmx_on_asm
.type vmx_on_asm, @function

vmx_on_asm:
    # RDI contains pointer to VMXON region
    mov %rdi, %rax
    vmxon (%rax)
    jnc 1f
    mov $1, %rax
    ret
1:
    xor %rax, %rax
    ret

.globl vmx_off_asm
.type vmx_off_asm, @function

vmx_off_asm:
    vmxoff
    ret

.globl vmx_vmread_asm
.type vmx_vmread_asm, @function

vmx_vmread_asm:
    # RDI = encoding, RSI = value pointer
    mov %rdi, %rax
    vmread %rax, (%rsi)
    ret

.globl vmx_vmwrite_asm
.type vmx_vmwrite_asm, @function

vmx_vmwrite_asm:
    # RDI = encoding, RSI = value
    mov %rdi, %rax
    mov %rsi, %rdx
    vmwrite %rax, %rdx
    ret

.globl vmx_vmcall_asm
.type vmx_vmcall_asm, @function

vmx_vmcall_asm:
    # RDI = rax, RSI = rbx, RDX = rcx, RCX = rdx
    mov %rdi, %rax
    mov %rsi, %rbx
    mov %rdx, %rcx
    mov %rcx, %rdx
    vmcall
    ret

.globl vmx_vmlaunch_asm
.type vmx_vmlaunch_asm, @function

vmx_vmlaunch_asm:
    vmlaunch
    ret

.globl vmx_vmresume_asm
.type vmx_vmresume_asm, @function

vmx_vmresume_asm:
    vmresume
    ret

.globl vmx_vmread_safe_asm
.type vmx_vmread_safe_asm, @function

vmx_vmread_safe_asm:
    # RDI = encoding, RSI = error pointer
    mov %rdi, %rax
    vmread %rax, %rax
    setnc (%rsi)
    mov %rax, (%rsi)
    ret

.globl vmx_vmwrite_safe_asm
.type vmx_vmwrite_safe_asm, @function

vmx_vmwrite_safe_asm:
    # RDI = encoding, RSI = value
    mov %rdi, %rax
    mov %rsi, %rdx
    vmwrite %rax, %rdx
    setnc %al
    ret
