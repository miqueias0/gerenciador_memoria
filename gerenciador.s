.globl original_brk
.globl setup_brk
.globl dismiss_brk
.globl memory_alloc
.globl memory_free

.section .bss
.lcomm original_brk 8
.lcomm current_brk 8

.section .text
#.global _start

memory_free:
    pushq %rbp
    movq %rsp, %rbp
    movb $0, -9(%rax)
    popq %rbp
    ret

dismiss_brk:
    pushq %rbp
    movq %rsp, %rbp
    movq $12, %rax
    movq original_brk, %rdi
    syscall
    popq %rbp
    ret

setup_brk:
    pushq %rbp
    movq %rsp, %rbp
    movq $12, %rax
    movq $0, %rdi
    syscall
    movq %rax, original_brk
    movq %rax, current_brk
    popq %rbp
    ret

#memory_alloc(unsigned long int bytes, unsigned char strategy)
#memory_alloc(%rdi, %rsi)

best_fit:
    pushq %rbp
    movq %rsp, %rbp
    pushq %r12
    pushq %rbx

    movq original_brk, %rax
    movq original_brk, %rdx
    movq current_brk, %rbx
    movq %rdi, %r12

    _loop:
    cmpq %rax, %rbx
    jle _fim_loop
    cmpb $1, (%rax)
    je _continuar
    movq 1(%rax), %r8
    cmpq 1(%rdx), %r8
    jl _continuar
    movq %rax, %rdx
    _continuar:
    addq 1(%rax), %rax
    addq $9, %rax
    jmp _loop
    _fim_loop:
    cmpq %rbx, %rdx
    jge _alocar
    cmpq 1(%rdx), %rdi
    jg _alocar

    movb $1, (%rdx)
    movq 1(%rdx), %r12
    subq %rdi, %r12
    movq %rdi, 1(%rdx)
    addq $9, %rdx
    movq %rdx, %rax
    addq %rdi, %rdx
    movb $0, (%rdx)
    subq $9, %r12
    movq %r12, 1(%rdx)
    jmp _fim

    _alocar:
    addq $9, %rdi
    addq %rbx, %rdi
    movq $12, %rax
    syscall
    movq %rax, current_brk
    subq %r12, %rax
    movb $1, -9(%rax)
    movq %r12, -8(%rax)

    _fim:
    popq %rbx
    popq %r12
    popq %rbp
    ret

worst_fit:
    pushq %rbp
    movq %rsp, %rbp
    pushq %r12
    pushq %rbx

    movq original_brk, %rax
    movq current_brk, %rbx
    movq %rdi, %r12

    _loop_2:
    cmpq %rax, %rbx
    jle _fim_loop_2
    cmpb $1, (%rax)
    je _continuar_2
    cmpq %rdi, 1(%rax)
    je _fim_loop_2
    addq $10, %r12
    cmpq %r12, 1(%rax)
    jge _fim_loop_2
    _continuar_2:
    movq %rdi, %r12
    addq 1(%rax), %rax
    addq $9, %rax
    jmp _loop_2

    _fim_loop_2:
    movq %rdi, %r12
    cmpq %rbx, %rax
    jge _alocar_2

    movb $1, (%rax)
    movq 1(%rax), %r12
    subq %rdi, %r12
    movq %rdi, 1(%rax)
    addq $9, %rax
    movq %rax, %r8
    addq %rdi, %r8
    movb $0, (%r8)
    subq $9, %r12
    movq %r12, 1(%r8)
    jmp _fim_2

    _alocar_2:
    addq $9, %rdi
    addq %rbx, %rdi
    movq $12, %rax
    syscall
    movq %rax, current_brk
    subq %r12, %rax
    movb $1, -9(%rax)
    movq %r12, -8(%rax)

    _fim_2:
    popq %rbx
    popq %r12
    popq %rbp
    ret

memory_alloc:
    pushq %rbp
    movq %rsp, %rbp

    cmpq $0, %rsi
    je _worst
    call best_fit
    popq %rbp
    ret
    _worst:
    call worst_fit
    popq %rbp
    ret

#memory_alloc(100, 0);
#memory_free(f_pnt);
#memory_alloc(50, 0);
#memory_alloc(100, 0);

_start:
    pushq %rbp
    movq %rsp, %rbp

    call setup_brk

    movq $100, %rdi
    movq $0, %rsi
    call memory_alloc

    call memory_free

    movq $50, %rdi
    movq $0, %rsi
    call memory_alloc

    movq $100, %rdi
    movq $0, %rsi
    call memory_alloc

    call dismiss_brk

    popq %rbp

    movq $60, %rax
    movq $0, %rdi
    syscall
