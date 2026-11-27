# Gerenciador de Memória

Alocador de heap em **Assembly x86-64** com estratégias **Best-Fit** e **Worst-Fit**, integrado a testes em C.

## Como rodar

### Testes padrão (professor)

```bash
make          # compila main.c + gerenciador.s → saida
./saida       # executa apenas os testes do professor
```

### Testes extras

```bash
make debug    # recompila tudo com -DDEBUG e inclui tests/testes.c
./saida       # professor + 33 testes extras no final
```

### Limpar artefatos

```bash
make clean    # remove *.o, tests/testes.o e saida
```

---

### Por que o Makefile usa `-fno-pie` e `-no-pie`

O `gerenciador.s` acessa variáveis globais por **endereçamento absoluto**:

```asm
movq original_brk, %r8
cmpq current_brk, %rdi
```

Isso gera relocação `R_X86_64_32S`, **incompatível com binários PIE** (o default do GCC no Linux moderno). Sem as flags, o linker falha com:

```
/usr/bin/ld: gerenciador.o: relocalização R_X86_64_32S contra ".bss"
não pode ser usada ao fazer um objecto PIE; recompile com -fPIE
```

