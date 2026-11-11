.section.bss
.lcomm original_brk 8
.lcomm current_brk 8

.section.text
.global _start

memory_free:
    pushq %rbp
    movq %rsp, %rbp
    movq $0, -9(%rax)
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

best_fit:
    pushq %rbp
    movq %rsp, %rbp

    popq %rbp
    ret


_start:
    pushq %rbp
    movq %rsp, %rbp
    call setup_brk
    call dismiss_brk
    movq $12, %rax
    movq original_brk, %rdi
    addq $9, %rdi
    syscall
    call memory_free
    movq $0, %rdi
    movq $60, %rax
    popq %rbp
    syscall
