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

    cmpq current_brk, %rdi
    jae _invalido
    cmpq original_brk, %rdi
    jbe _invalido
    movq original_brk, %r8
    addq $9, %r8
    _free_l:
    cmpq %rdi, %r8
    jae _fim_free
    addq -8(%r8), %r8
    addq $9, %r8
    jmp _free_l

    _fim_free:
    cmpq %rdi, %r8
    jne _invalido

    movb $0, -9(%rdi)
    movq $0, %rax
    _invalido:
    popq %rbp
    ret

dismiss_brk:
    pushq %rbp
    movq %rsp, %rbp
    movq $12, %rax
    movq original_brk, %rdi
    syscall
    movq %rax, current_brk
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
    movq $_best_fit, %rcx
    cmpq $1, %rsi
    je _loop
    movq %rdi, %r11
    movq $_worst_fit, %rcx

    _loop:
    cmpq %r8, %r9
    jbe _fim_l
    cmpb $1, (%r8)
    je _continuar
    cmpq %rdi, 1(%r8)
    jb _continuar
    cmpq %r11, 1(%r8)
    jmp *%rcx
    _best_fit:
    jae _continuar
    jmp _guardar
    _worst_fit:
    jb _continuar
    _guardar:
    movq %r8, %r10
    movq 1(%r8), %r11
    _continuar:
    addq 1(%r8), %r8
    addq $9, %r8
    jmp _loop

    _fim_l:
    cmpq %r10, %r9
    jbe _alocar
    subq %rdi, %r11
    cmpq $9, %r11
    jbe _fim
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
