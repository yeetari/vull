.file "x86_64_sysv.S"
.text

.extern __sanitizer_finish_switch_fiber
.extern __sanitizer_start_switch_fiber
.extern vull_free_tasklet

.globl vull_make_context
vull_make_context:
    // Set instruction pointer.
    mov %rsi, -8(%rdi)

    // Set stack pointer.
    lea -72(%rdi), %r8
    and $~0xf, %r8
    sub $8, %r8
    mov %r8, -16(%rdi)
    ret

.globl vull_load_context
vull_load_context:
#ifdef __SANITIZE_ADDRESS__
    // Save arguments.
    push %rdi
    push %rsi

    mov 16(%rdi), %rdx // stack_size
    mov 8(%rdi), %rsi // stack_top
    xor %rdi, %rdi // null to signal that ASAN should delete the old fake stack
    sub %rdx, %rsi // stack_top - stack_size = stack_bottom
    call __sanitizer_start_switch_fiber

    // Restore as if ASAN call wasn't there.
    pop %rsi
    pop %rdi
#endif

    // Load stack_top into r8.
    mov 8(%rdi), %r8

    // Load GP registers.
    mov -64(%r8), %r12
    mov -56(%r8), %r13
    mov -48(%r8), %r14
    mov -40(%r8), %r15
    mov -32(%r8), %rbx
    mov -24(%r8), %rbp
    mov -16(%r8), %rsp

    // Switched stack to new tasklet, now safe to destroy old tasklet.
    push %r8
    push %rdi
    mov %rsi, %rdi
    call vull_free_tasklet

#ifdef __SANITIZE_ADDRESS__
    pop %rdi
    push %rdi
    mov 24(%rdi), %rdi // fake_stack
    xor %rsi, %rsi
    xor %rdx, %rdx
    call __sanitizer_finish_switch_fiber
#endif

    pop %rdi
    pop %r8

    // rdi already contains the tasklet 'this' pointer.
    jmp *-8(%r8)

.globl vull_swap_context
vull_swap_context:
#ifdef __SANITIZE_ADDRESS__
    // Save arguments.
    push %rdi
    push %rsi

    lea 24(%rdi), %rdi // pointer to 'from' fake_stack pointer
    mov 16(%rsi), %rdx // 'to' stack_size
    mov 8(%rsi), %rsi // 'to' stack_top
    sub %rdx, %rsi
    call __sanitizer_start_switch_fiber

    pop %rsi
    pop %rdi
#endif

    // Load 'from' stack_top into r8.
    mov 8(%rdi), %r8

    // Save instruction pointer (return address).
    mov (%rsp), %r9
    mov %r9, -8(%r8)

    // Save stack pointer.
    lea 8(%rsp), %r9
    mov %r9, -16(%r8)

    // Save 'from' GP registers.
    mov %r12, -64(%r8)
    mov %r13, -56(%r8)
    mov %r14, -48(%r8)
    mov %r15, -40(%r8)
    mov %rbx, -32(%r8)
    mov %rbp, -24(%r8)

    // Load 'to' stack_top into r8.
    mov 8(%rsi), %r8

    // Load 'to' context.
    mov -64(%r8), %r12
    mov -56(%r8), %r13
    mov -48(%r8), %r14
    mov -40(%r8), %r15
    mov -32(%r8), %rbx
    mov -24(%r8), %rbp
    mov -16(%r8), %rsp

#ifdef __SANITIZE_ADDRESS__
    push %rsi
    push %r8

    mov 24(%rsi), %rdi // fake_stack
    xor %rsi, %rsi
    xor %rdx, %rdx
    call __sanitizer_finish_switch_fiber

    // Restore as if ASAN calls were not there.
    pop %r8
    pop %rsi
#endif

    // Set 'this' pointer and jump to next context.
    mov %rsi, %rdi
    jmp *-8(%r8)

.section .note.GNU-stack,"",%progbits
