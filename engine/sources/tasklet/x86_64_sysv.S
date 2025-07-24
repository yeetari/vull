.file "x86_64_sysv.S"
.text

.globl vull_make_fiber
vull_make_fiber:
    // Set the instruction pointer to our entry point.
    mov %rsi, -8(%rdi)

    // Point the stack to the fiber memory top, minus the space needed for the saved registers, and aligned to 16 bytes.
    lea -72(%rdi), %r8
    and $~0xf, %r8
    sub $8, %r8
    mov %r8, -16(%rdi)
    ret

.globl vull_switch_fiber
vull_switch_fiber:
    // Skip saving if the from fiber is null.
    test %rdi, %rdi
    jz .Lskip

    // Pop the return address into the saved instruction pointer.
    pop -8(%rdi)

    // Save the stack pointer.
    mov %rsp, -16(%rdi)

    // Save the ABI-saved GP registers.
    mov %r12, -64(%rdi)
    mov %r13, -56(%rdi)
    mov %r14, -48(%rdi)
    mov %r15, -40(%rdi)
    mov %rbx, -32(%rdi)
    mov %rbp, -24(%rdi)

.Lskip:
    // Load the new GP registers and stack pointer.
    mov -64(%rsi), %r12
    mov -56(%rsi), %r13
    mov -48(%rsi), %r14
    mov -40(%rsi), %r15
    mov -32(%rsi), %rbx
    mov -24(%rsi), %rbp
    mov -16(%rsi), %rsp

    // Jump to the new fiber, or return to an existing one.
    jmp *-8(%rsi)

.section .note.GNU-stack,"",%progbits
