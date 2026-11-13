.globl original_brk
.globl setup_brk
.globl dismiss_brk
.globl memory_alloc
.globl memory_free

.section .bss
.lcomm original_brk 8
.lcomm current_brk 8

.section .text

memory_free:
    pushq %rbp
    movq %rsp, %rbp
    cmpq current_brk, %rax
    jg _invalido
    movb $0, -9(%rax)
    _invalido:
    movq $0, %rax
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

memory_alloc:
    pushq %rbp
    movq %rsp, %rbp

    movq original_brk, %r8
    movq current_brk, %r9
    movq current_brk, %r10

    movq $0x7FFFFFFFFFFFFFFF, %r11
    cmpq $1, %rsi
    je _loop
    movq %rdi, %r11

    _loop:
    cmpq %r8, %r9
    jle _fim_l
    cmpq $1, (%r8)
    je _continuar
    cmpq %rdi, 1(%r8)
    jl _continuar
    cmpq $0, %rsi
    je _worst
    cmpq %r11, 1(%r8)
    jge _continuar
    jmp _guardar
    _worst:
    cmpq %r11, 1(%r8)
    jl _continuar
    _guardar:
    movq %r8, %r10
    movq 1(%r8), %r11
    _continuar:
    addq 1(%r8), %r8
    addq $9, %r8
    jmp _loop

    _fim_l:
    cmpq %r10, %r9
    jle _alocar
    subq %rdi, %r11
    cmpq $9, %r11
    jle _fim
    subq $9, %r11
    movb $0, 9(%r10, %rdi, 1)
    movq %r11, 10(%r10, %rdi, 1)
    movq %rdi, 1(%r10)
    jmp _fim

    _alocar:
    movq $12, %rax
    addq $9, %rdi
    pushq %rdi
    addq %r10, %rdi
    syscall
    movq %rax, current_brk
    popq %rdi
    subq %rdi, %rax
    movq %rax, %r10
    subq $9, %rdi
    movq %rdi, 1(%r10)

    _fim:
    movb $1, (%r10)
    movq %r10, %rax
    addq $9, %rax

    popq %rbp
    ret
