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
    # Rotina na chamada de procedimentos 
    pushq %rbp
    movq %rsp, %rbp

    # Verificação se o endereço está dentro da heap
    cmpq current_brk, %rdi
    jae _invalido
    cmpq original_brk, %rdi
    jbe _invalido

    # Verificação se o endereço é um endereço real de um bloco alocado 
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

    # Procedimento de marcar o bloco como livre
    movb $0, -9(%rdi)
    # Retorno do procedimento 
    movq $0, %rax
    jmp _ret_f
    _invalido:
    movq $1, %rax

    _ret_f:
    # Rotina no retorno de procedimentos
    popq %rbp
    ret

dismiss_brk:
    # Rotina na chamada de procedimentos 
    pushq %rbp
    movq %rsp, %rbp

    # Voltar o brk para o brk inicial 
    movq $12, %rax
    movq original_brk, %rdi
    syscall
    movq %rax, current_brk

    # Rotina no retorno de procedimentos
    popq %rbp
    ret

setup_brk:
    # Rotina na chamada de procedimentos
    pushq %rbp
    movq %rsp, %rbp

    # Salva os limites iniciais da heap nas variáveis globais
    movq $12, %rax
    movq $0, %rdi
    syscall
    movq %rax, original_brk
    movq %rax, current_brk

    # Rotina no retorno de procedimentos
    popq %rbp
    ret

memory_alloc:
    # Rotina na chamada de procedimentos
    pushq %rbp
    movq %rsp, %rbp

    # Se for negativo ou zero retorna NULL
    cmpq $0, %rdi
    jle _erroAlloc

    # Setup dos registradores utilizados
    # %r8 = 
    movq original_brk, %r8
    # %r9 = 
    movq current_brk, %r9
    # %r10 = 
    movq current_brk, %r10
    # %r11 = 

    # Setup para utilizar uma das estratégias de alocação, salvando o rótulo da estratégia em %rcx
    # %r11 guarda o menor/maior tamanho de bloco livre compatível encontrado até agora
    # Começa em 0x7FFFFFFFFFFFFFFF (maior valor possível com bit mais significativo igual a 0)
    # Para garantir que qualquer tamanho válido seja menor na primeira comparação do algoritmo best-fit
    movq $0x7FFFFFFFFFFFFFFF, %r11
    movq $_best_fit, %rcx
    cmpb $1, %sil
    je _loop
    # Usa o tamanho pedido em %rdi como limite inicial do worst-fit
    # blocos livres maiores que %r11 substituirão o melhor candidato atual
    movq %rdi, %r11
    movq $_worst_fit, %rcx

    # Loop para encontar o melhor bloco livre disponível
    _loop:
    cmpq %r8, %r9
    jbe _fim_l
    # Passa para o próximo caso esteja em uso ou menor do que precisamos 
    cmpb $1, (%r8)
    je _continuar
    cmpq %rdi, 1(%r8)
    jb _continuar
    # Comparação que é interpretada pela estratégia escolhida
    # Pula para o endereço do rótulo da estratégia escolhida
    # Tira a necessidade de ficar comparando qual a estratégia escolhida
    # Em cada iteração
    cmpq %r11, 1(%r8)
    jmp *%rcx
    _best_fit:
    jae _continuar
    jmp _guardar
    _worst_fit:
    jb _continuar
    # Garda o endereço que é o melhor até o momento
    _guardar:
    movq %r8, %r10
    movq 1(%r8), %r11

    # Prepara o loop para o próximo bloco 
    _continuar:
    addq 1(%r8), %r8
    addq $9, %r8
    jmp _loop

    # Fim do loop por acabar os blocos existentes 
    _fim_l:
    # Se for igual o brk atual significa que não achou um bloco utilizavel e precisa alocar 
    cmpq %r10, %r9
    jbe _alocar
    # Se o que sobrou de do novo bloco for pelo menos 10 cria um bloco novo após o que foi pedido, se não aloca o bloco com bytes a mais 
    subq %rdi, %r11
    cmpq $9, %r11
    jbe _fim
    subq $9, %r11
    movb $0, 9(%r10, %rdi, 1)
    movq %r11, 10(%r10, %rdi, 1)
    movq %rdi, 1(%r10)
    jmp _fim

    # Aloca memória extra caso não tenha nenhum bloco funcional disponível 
    _alocar:
    movq $12, %rax
    addq $9, %rdi
    pushq %rdi
    addq %r10, %rdi
    syscall

    # Verifica se alocou 
    popq %rdi
    cmpq %rax, current_brk
    je _erroAlloc

    # Se alocou arruma os endereços 
    movq %rax, current_brk
    subq %rdi, %rax
    movq %rax, %r10
    subq $9, %rdi
    movq %rdi, 1(%r10)
    jmp _fim

    # Se não alocou a memória retorna NULL
    _erroAlloc:
    movq $0, %rax
    jmp _ret

    # Marca o bloco como alocado e coloca o endereço de rotorno em %rax
    _fim:
    movb $1, (%r10)
    movq %r10, %rax
    addq $9, %rax

    # Rotina no retorno de procedimentos
    _ret:
    popq %rbp
    ret
