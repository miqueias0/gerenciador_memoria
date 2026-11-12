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

    movq original_brk, %r8
    movq current_brk, %r9

    _loop:
    cmpq %r8, %r9
    jle _alocar
    cmpb $1, (%r8)
    je _continua
    cmpq %rdi, 1(%r8)
    jge _fim_l
    _continua:
    addq 1(%r8), %r8
    addq $9, %r8
    jmp _loop

    _fim_l:
    movq 1(%r8), %r9
    subq %rdi, %r9
    cmpq $9, %r9
    jle _fim
    subq $9, %r9
    movb $0, 9(%r8,%rdi,1)
    movq %r9, 10(%r8,%rdi,1)
    movq %rdi, 1(%r8)
    jmp _fim

    _alocar:
    movq $12, %rax
    addq $9, %rdi
    pushq %rdi
    addq %r9, %rdi
    syscall
    movq %rax, current_brk
    popq %rdi
    subq %rdi, %rax
    subq $9, %rdi
    movq %rax, %r10
    movq %rdi, 1(%r8)

    _fim:
    movb $1, (%r8)
    movq %r8, %rax
    addq $9, %rax

    popq %rbp
    ret

worst_fit:
    pushq %rbp
    movq %rsp, %rbp

    movq original_brk, %r8
    movq original_brk, %r10
    movq current_brk, %r9

    movq %rdi, %r11

    _loop_2:
    cmpq %r8, %r9
    jle _fim_l_2
    cmpb $1, (%r8)
    je _continuar_2
    cmpq 1(%r8), %r11
    jge _continuar_2
    movq %r8, %r10
    movq 1(%r10), %r11
    _continuar_2:
    addq 1(%r8), %r8
    addq $9, %r8
    jmp _loop_2

    _fim_l_2:
    cmpq %r10, %r9
    jle _alocar_2
    cmpq %rdi, 1(%r10)
    jl _alocar_2
    subq %rdi, %r11
    cmpq $9, %r11
    jle _fim_2
    subq $9, %r11
    movb $0, 9(%r10, %rdi, 1)
    movq %r11, 10(%r10, %rdi, 1)
    movq %rdi, 1(%r10)
    jmp _fim_2

    _alocar_2:
    movq $12, %rax
    addq $9, %rdi
    pushq %rdi
    addq %r9, %rdi
    syscall
    movq %rax, current_brk
    popq %rdi
    subq %rdi, %rax
    subq $9, %rdi
    movq %rax, %r10
    movq %rdi, 1(%r10)

    _fim_2:
    movb $1, (%r10)
    movq %rdi, 1(%r10)
    movq %r10, %rax
    addq $9, %rax

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
#memory_free(s_pnt);
#memory_free(t_pnt);
#memory_alloc(40, 1);;

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
    pushq %rax

    movq $100, %rdi
    movq $0, %rsi
    call memory_alloc

    call memory_free
    popq %rax
    call memory_free

    movq $40, %rdi
    movq $1, %rsi
    call memory_alloc

    call dismiss_brk

    popq %rbp

    movq $60, %rax
    movq $0, %rdi
    syscall
