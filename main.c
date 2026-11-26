#include <stdio.h>
#include <stdlib.h>

void setup_brk();
void dismiss_brk();
void* memory_alloc(unsigned long int bytes, unsigned char strategy);
int memory_free(void *pointer);						

extern void *original_brk;	/* Você precisa ter a variável global que armazena o valor de brk como um extern aqui.
				No código de teste estou chamandando de original_brk, utilizem esse nome também. */

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

static void executar_testes_extras(void);

int main() { 

	printf("============================== ROTINAS DE TESTE ==============================\n");
	
	setup_brk();
	void *initial_brk = original_brk;
	void *f_pnt, *s_pnt, *t_pnt, *fo_pnt;

	f_pnt = memory_alloc(100, 0);
	printf("==>> ALOCANDO UM ESPAÇO DE 100 BYTES WORST FIT:\n");
	printf("\tLOCAL: %s\n", f_pnt-9 == initial_brk ? "CORRETO!" : "INCORRETO!");
	printf("\tIND. DE USO: %s\n", *((char*) (f_pnt-9)) == 1 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (f_pnt-8)) == 100 ? "CORRETO!" : "INCORRETO!");

	printf("==>> DESALOCANDO UM ESPAÇO DE 100 BYTES:\n");
	memory_free(f_pnt);
	printf("\tIND. DE USO: %s\n", *((char*) (f_pnt-9)) == 0 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (f_pnt-8)) == 100 ? "CORRETO!" : "INCORRETO!");

	s_pnt = memory_alloc(50, 0);
	t_pnt = memory_alloc(100, 0);
	printf("==>> ALOCANDO UM ESPAÇO DE 50 BYTES WORST FIT:\n");
	printf("\tLOCAL: %s\n", s_pnt-9 == initial_brk ? "CORRETO!" : "INCORRETO!");
	printf("\tIND. DE USO: %s\n", *((char*) (s_pnt-9)) == 1 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (s_pnt-8)) == 50 ? "CORRETO!" : "INCORRETO!");
	printf("==>> ALOCANDO UM ESPAÇO DE 100 BYTES WORST FIT:\n");
	printf("\tLOCAL: %s\n", s_pnt+100 == t_pnt-9 ? "CORRETO!" : "INCORRETO!");
	printf("\tIND. DE USO: %s\n", *((char*) (t_pnt-9)) == 1 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (t_pnt-8)) == 100 ? "CORRETO!" : "INCORRETO!");
	printf("==> VERIFICANDO A FRAGMENTAÇÃO DE MEMÓRIA:\n");
	printf("\tIND. DE USO: %s\n", *((char*) (s_pnt+50)) == 0 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (s_pnt+51)) == 41 ? "CORRETO!" : "INCORRETO!");
	
	printf("==>> DESALOCANDO TUDO:\n");
	memory_free(s_pnt);
	memory_free(t_pnt);
	printf("\tIND. DE USO: %s\n", *((char*) (s_pnt-9)) == 0 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (s_pnt-8)) == 50 ? "CORRETO!" : "INCORRETO!");
	printf("\tIND. DE USO: %s\n", *((char*) (t_pnt-9)) == 0 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (t_pnt-8)) == 100 ? "CORRETO!" : "INCORRETO!");
	
	printf("==>> ALOCANDO UM ESPAÇO DE 40 BYTES BEST FIT:\n");
	fo_pnt = memory_alloc(40, 1);
	printf("\LOCAL: %s\n", (s_pnt+59) == fo_pnt ? "CORRETO!" : "INCORRETO!");
	printf("\tIND. DE USO: %s\n", *((char*) (fo_pnt-9)) == 1 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (fo_pnt-8)) == 41 ? "CORRETO!" : "INCORRETO!");

	printf("==>> DESALOCANDO UM ESPAÇO DE 40 BYTES:\n");
	memory_free(fo_pnt);
	printf("\tIND. DE USO: %s\n", *((char*) (fo_pnt-9)) == 0 ? "CORRETO!" : "INCORRETO!");
	printf("\tTAMANHO: %s\n", *((long long*) (fo_pnt-8)) == 41 ? "CORRETO!" : "INCORRETO!");

	printf("==>> DESALOCANDO A PILHA (ILEGAL):\n");
	unsigned long long stack_var = 0;
	unsigned int alloc_return = memory_free((void*) &stack_var);
	if (!alloc_return) printf("\tO RETORNO DA LIBERAÇÃO FOI NULL!\n");
	else printf("\tO RETORNO DA LIBERAÇÃO NÃO FOI NULL!\n");

	executar_testes_extras();
	return 0;
}

static const char *nome_sinal(int sig) {
	switch (sig) {
		case SIGSEGV: return "SIGSEGV";
		case SIGBUS:  return "SIGBUS";
		case SIGALRM: return "SIGALRM";
		case SIGABRT: return "SIGABRT";
		case SIGFPE:  return "SIGFPE";
		case SIGILL:  return "SIGILL";
		default:      return "OUTRO";
	}
}

static int blocos_sobrepoem(void *p1, unsigned long s1, void *p2, unsigned long s2) {
	uintptr_t a_ini = (uintptr_t)p1 - 9;
	uintptr_t a_fim = (uintptr_t)p1 + (uintptr_t)s1;
	uintptr_t b_ini = (uintptr_t)p2 - 9;
	uintptr_t b_fim = (uintptr_t)p2 + (uintptr_t)s2;
	return (a_ini < b_fim) && (b_ini < a_fim);
}

static void rodar_teste(const char *nome, int (*fn)(void), int *passou, int *falhou, int *quebrou) {
	fflush(stdout);
	pid_t pid = fork();
	if (pid < 0) {
		printf("[FALHOU]  %s (erro no fork)\n", nome);
		(*falhou)++;
		return;
	}
	if (pid == 0) {
		alarm(5);
		dismiss_brk();
		setup_brk();
		int rc = fn();
		fflush(stdout);
		_exit(rc);
	}
	int status = 0;
	if (waitpid(pid, &status, 0) < 0) {
		printf("[FALHOU]  %s (erro no waitpid)\n", nome);
		(*falhou)++;
		return;
	}
	if (WIFSIGNALED(status)) {
		int sig = WTERMSIG(status);
		printf("[QUEBROU] %s -> sinal %d (%s)\n", nome, sig, nome_sinal(sig));
		(*quebrou)++;
	} else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		printf("[PASSOU]  %s\n", nome);
		(*passou)++;
	} else {
		int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		printf("[FALHOU]  %s (codigo %d)\n", nome, code);
		(*falhou)++;
	}
}

/* 1. Sobreposicao de blocos ativos (Bug: cmpq $1, (%r8) em vez de cmpb $1, (%r8)) */
static int teste_sobreposicao_blocos_ativos(void) {
	for (unsigned char strat = 0; strat <= 1; strat++) {
		dismiss_brk();
		setup_brk();
		unsigned char *a = (unsigned char *)memory_alloc(100, strat);
		if (!a) {
			printf("\t  -> memory_alloc(100, %u) retornou NULL\n", strat);
			return 1;
		}
		memset(a, 0xAA, 100);

		unsigned char *b = (unsigned char *)memory_alloc(50, strat);
		if (!b) {
			printf("\t  -> memory_alloc(50, %u) retornou NULL\n", strat);
			return 1;
		}
		memset(b, 0xBB, 50);

		if (blocos_sobrepoem(a, 100, b, 50)) {
			printf("\t  -> estrategia %u: bloco b (%p) sobrepos bloco ativo a (%p)!\n",
			       strat, (void *)b, (void *)a);
			return 1;
		}
		if (*((char *)(a - 9)) != 1 || *((long long *)(a - 8)) != 100) {
			printf("\t  -> estrategia %u: cabecalho de a corrompido (uso=%d, tam=%lld)\n",
			       strat, (int)*((char *)(a - 9)), *((long long *)(a - 8)));
			return 1;
		}
		for (int i = 0; i < 100; i++) {
			if (a[i] != 0xAA) {
				printf("\t  -> estrategia %u: payload de a corrompido no byte %d (0x%02X)\n",
				       strat, i, a[i]);
				return 1;
			}
		}
	}
	return 0;
}

/* 2. memory_free via ponteiro de funcao (Bug ABI: le %rax em vez de %rdi) */
static int teste_free_abi_ponteiro_funcao(void) {
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
	int (*volatile fn)(void *) = memory_free;
	fn(p);
	if (*((char *)p - 9) != 0) {
		printf("\t  -> bloco nao foi marcado como livre\n");
		return 1;
	}
	return 0;
}

/* 3. memory_free com %rax sujo (independente do nivel de otimizacao) */
static int teste_free_abi_rax_sujo(void) {
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
#ifdef __x86_64__
	int ret = 0;
	__asm__ volatile (
		"movq $0x1234, %%rax\n\t"
		"movq %1, %%rdi\n\t"
		"call memory_free\n\t"
		"movl %%eax, %0\n\t"
		: "=r"(ret)
		: "r"(p)
		: "rax", "rdi", "rcx", "rdx", "rsi", "r8", "r9", "r10", "r11", "memory", "cc"
	);
	(void)ret;
#else
	memory_free(p);
#endif
	if (*((char *)p - 9) != 0) {
		printf("\t  -> bloco nao foi marcado como livre (uso=%d)\n", (int)*((char *)p - 9));
		return 1;
	}
	return 0;
}

/* 4. memory_free(NULL) */
static int teste_free_null(void) {
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
	memory_free(NULL);
	return 0;
}

/* 5. memory_free abaixo de original_brk (corrompe .bss/.data) */
static unsigned char canario_bss[32];
static int teste_free_abaixo_original_brk(void) {
	memset(canario_bss, 0xFF, sizeof(canario_bss));
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
	memory_free((void *)&canario_bss[16]);
	if (canario_bss[7] != 0xFF) {
		printf("\t  -> corrompeu memoria estatica (.bss) em ptr-9! canario_bss[7]=0x%02X\n",
		       canario_bss[7]);
		return 1;
	}
	return 0;
}

/* 6. memory_free com endereco alto (bit 63 = 1; jg com sinal falha) */
static int teste_free_endereco_bit63(void) {
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
	memory_free((void *)(uintptr_t)0xFFFFFFFFFFFFFFFFULL);
	return 0;
}

/* 7. memory_free exatamente em current_brk (cmpq permite igualdade e escreve em payload) */
static int teste_free_limites_exatos_brk(void) {
	unsigned char *p = (unsigned char *)memory_alloc(64, 0);
	if (!p) return 1;
	memset(p, 0xCC, 64);
	void *cur_brk = (void *)(p + 64);
	memory_free(cur_brk);
	if (p[55] != 0xCC) {
		printf("\t  -> memory_free(current_brk) corrompeu payload p[55] = 0x%02X (esperado 0xCC)\n",
		       p[55]);
		return 1;
	}
	if (*((char *)(p - 9)) != 1) {
		printf("\t  -> bloco alterado indevidamente\n");
		return 1;
	}
	return 0;
}

/* 8. memory_free em ponteiro interno ao bloco (falta validar fronteira de bloco) */
static int teste_free_ponteiro_interno(void) {
	unsigned char *p = (unsigned char *)memory_alloc(100, 0);
	if (!p) return 1;
	memset(p, 0xDD, 100);
	memory_free((void *)(p + 20));
	if (p[11] != 0xDD) {
		printf("\t  -> memory_free(p+20) zerou byte p[11] do payload (0x%02X)\n", p[11]);
		return 1;
	}
	if (*((char *)(p - 9)) != 1) {
		printf("\t  -> bloco marcado como livre com ponteiro interno\n");
		return 1;
	}
	return 0;
}

/* 9. Retorno de memory_free para ponteiro valido vs invalido */
static int teste_free_retorno_valido_vs_invalido(void) {
	void *p = memory_alloc(32, 0);
	if (!p) return 1;
	int r_valido = memory_free(p);
	unsigned long long x = 0;
	int r_invalido = memory_free((void *)&x);
	if (r_valido == r_invalido) {
		printf("\t  -> retorno identico (%d) para ponteiro valido e invalido\n", r_valido);
		return 1;
	}
	return 0;
}

/* 10. Falha da syscall brk ao pedir tamanho impossivel */
static int teste_alloc_brk_falha_tamanho_gigante(void) {
	void *p = memory_alloc(1ULL << 50, 0);
	if (p != NULL) {
		printf("\t  -> deveria retornar NULL para 2^50 bytes, retornou %p\n", p);
		return 1;
	}
	return 0;
}

/* 11. Tamanho negativo (signed) com bloco livre existente (Bug: jl com sinal em gerenciador.s:66) */
static int teste_alloc_tamanho_negativo_com_bloco_livre(void) {
	void *a = memory_alloc(100, 0);
	if (!a) return 1;
	memory_free(a);
	void *p = memory_alloc((unsigned long)-1, 0);
	if (p != NULL) {
		printf("\t  -> retornou %p para tamanho UINT64_MAX (reaproveitou bloco de 100 bytes!)\n", p);
		return 1;
	}
	return 0;
}

/* 12. Integer overflow em addq $9, %rdi */
static int teste_alloc_overflow_size_mais_9(void) {
	void *p = memory_alloc((unsigned long)-8, 0);
	if (p != NULL) {
		printf("\t  -> overflow em size+9: memory_alloc(-8) retornou %p\n", p);
		(void)memory_alloc(16, 0);
		return 1;
	}
	return 0;
}

/* 13. Alocacao de 0 bytes */
static int teste_alloc_zero_bytes(void) {
	void *p0 = memory_alloc(0, 0);
	if (p0 != NULL) {
		printf("\t  -> memory_alloc(0, 0) retornou %p (nao-NULL; tam=%lld)\n",
		       p0, *((long long *)((char *)p0 - 8)));
		dismiss_brk();
		setup_brk();
		void *b = memory_alloc(100, 0);
		memory_free(b);
		void *z = memory_alloc(0, 1);
		if (z != NULL) {
			printf("\t  -> memory_alloc(0, 1) sobre bloco de 100B retornou %p (tam=%lld)\n",
			       z, *((long long *)((char *)z - 8)));
		}
		return 1;
	}
	return 0;
}

/* 14. Strategy em %sil com bits superiores de %rsi sujos (Bug: cmpq $1, %rsi) */
static int teste_strategy_rsi_bits_superiores_sujos(void) {
	void *p1 = memory_alloc(80, 0);
	void *p2 = memory_alloc(300, 0);
	if (!p1 || !p2) return 1;
	memory_free(p1);
	memory_free(p2);

	void *res = NULL;
#ifdef __x86_64__
	__asm__ volatile (
		"movq $50, %%rdi\n\t"
		"movq $0xDEAD000000000001, %%rsi\n\t"
		"call memory_alloc\n\t"
		"movq %%rax, %0\n\t"
		: "=r"(res)
		:
		: "rax", "rdi", "rsi", "rcx", "rdx", "r8", "r9", "r10", "r11", "memory", "cc"
	);
#else
	res = memory_alloc(50, 1);
#endif
	if (res != p1) {
		printf("\t  -> esperado p1=%p (Best Fit, %%sil=1), obteve %p (p2=%p)\n", p1, res, p2);
		return 1;
	}
	return 0;
}

/* 15. Limiar exato do split: sobra == 9 vs sobra == 10 */
static int teste_limites_split_9_vs_10(void) {
	void *p = memory_alloc(100, 0);
	if (!p) return 1;
	memory_free(p);

	/* sobra = 100 - 91 = 9: NAO deve dividir */
	void *p91 = memory_alloc(91, 1);
	if (p91 != p || *((long long *)((char *)p91 - 8)) != 100) {
		printf("\t  -> sobra=9 dividiu ou realocou indevidamente (tam=%lld)\n",
		       *((long long *)((char *)p91 - 8)));
		return 1;
	}
	memory_free(p91);

	/* sobra = 100 - 90 = 10: DEVE dividir em 90 + header(9) + 1 */
	void *p90 = memory_alloc(90, 1);
	if (p90 != p || *((long long *)((char *)p90 - 8)) != 90) {
		printf("\t  -> sobra=10 nao ajustou tamanho do bloco para 90 (tam=%lld)\n",
		       *((long long *)((char *)p90 - 8)));
		return 1;
	}
	char rem_uso = *((char *)p90 + 90);
	long long rem_tam = *((long long *)((char *)p90 + 91));
	if (rem_uso != 0 || rem_tam != 1) {
		printf("\t  -> bloco restante incorreto (uso=%d, tam=%lld; esperado uso=0, tam=1)\n",
		       (int)rem_uso, rem_tam);
		return 1;
	}
	return 0;
}

/* 16. Selecao Best Fit vs Worst Fit com multiplos blocos e desempate */
static int teste_best_fit_vs_worst_fit_multiplos_blocos(void) {
	void *b50 = memory_alloc(50, 0);
	void *b80 = memory_alloc(80, 0);
	void *b120 = memory_alloc(120, 0);
	void *b300 = memory_alloc(300, 0);
	if (!b50 || !b80 || !b120 || !b300) return 1;
	memory_free(b50);
	memory_free(b80);
	memory_free(b120);
	memory_free(b300);

	void *r_best = memory_alloc(60, 1);
	if (r_best != b80) {
		printf("\t  -> Best Fit escolheu %p (esperado b80=%p)\n", r_best, b80);
		return 1;
	}

	dismiss_brk();
	setup_brk();
	b50 = memory_alloc(50, 0);
	b80 = memory_alloc(80, 0);
	b120 = memory_alloc(120, 0);
	b300 = memory_alloc(300, 0);
	memory_free(b50);
	memory_free(b80);
	memory_free(b120);
	memory_free(b300);

	void *r_worst = memory_alloc(60, 0);
	if (r_worst != b300) {
		printf("\t  -> Worst Fit escolheu %p (esperado b300=%p)\n", r_worst, b300);
		return 1;
	}

	/* Desempate com tres blocos livres de 100 bytes */
	dismiss_brk();
	setup_brk();
	void *p1 = memory_alloc(100, 0);
	void *p2 = memory_alloc(209, 0);
	memory_free(p1);
	memory_free(p2);
	void *q = memory_alloc(100, 0);
	memory_free(q);
	void *p3 = (void *)((char *)p2 + 109);
	void *sel_bf = memory_alloc(100, 1);
	if (sel_bf != p1) {
		printf("\t  -> Desempate Best Fit escolheu %p (esperado primeiro p1=%p)\n", sel_bf, p1);
		return 1;
	}

	dismiss_brk();
	setup_brk();
	p1 = memory_alloc(100, 0);
	p2 = memory_alloc(209, 0);
	memory_free(p1);
	memory_free(p2);
	q = memory_alloc(100, 0);
	memory_free(q);
	p3 = (void *)((char *)p2 + 109);
	void *sel_wf = memory_alloc(100, 0);
	if (sel_wf != p3) {
		printf("\t  -> Desempate Worst Fit escolheu %p (esperado ultimo p3=%p)\n", sel_wf, p3);
		return 1;
	}
	return 0;
}

/* 17. Coalescing de blocos livres adjacentes */
static int teste_ausencia_coalescing(void) {
	void *p1 = memory_alloc(100, 0);
	void *p2 = memory_alloc(120, 0);
	if (!p1 || !p2) return 1;
	memory_free(p1);
	memory_free(p2);
	void *p3 = memory_alloc(180, 0);
	if (p3 != p1) {
		printf("\t  -> blocos livres adjacentes (100+9+120=229) nao coalesceram; p3=%p > p2=%p\n",
		       p3, p2);
		return 1;
	}
	return 0;
}

/* 18. Ciclo de vida dismiss_brk e reuso */
static int teste_dismiss_brk_e_reuso(void) {
	void *p1 = memory_alloc(200, 0);
	if (!p1) return 1;
	memset(p1, 0x55, 200);
	dismiss_brk();
	unsigned char *p2 = (unsigned char *)memory_alloc(64, 0);
	if (!p2) return 1;
	if ((char *)p2 - 9 != (char *)original_brk) {
		printf("\t  -> apos dismiss_brk, p2-9 (%p) != original_brk (%p)\n",
		       (void *)((char *)p2 - 9), original_brk);
		return 1;
	}
	if (*((char *)p2 - 9) != 1 || *((long long *)p2 - 1) != 64) {
		printf("\t  -> cabecalho incorreto apos dismiss_brk\n");
		return 1;
	}
	memset(p2, 0x77, 64);
	for (int i = 0; i < 64; i++) {
		if (p2[i] != 0x77) return 1;
	}
	return 0;
}

/* 19. Estresse de integridade com 200 blocos simultaneos e tamanhos variados */
static int teste_estresse_integridade_200_blocos(void) {
	enum { N = 200 };
	static unsigned char *ptrs[N];
	static unsigned long sizes[N];

	for (int i = 0; i < N; i++) {
		unsigned long sz = 16UL + (unsigned long)((i * 73) % 497);
		unsigned char strat = (unsigned char)(i & 1);
		ptrs[i] = (unsigned char *)memory_alloc(sz, strat);
		sizes[i] = sz;
		if (!ptrs[i]) {
			printf("\t  -> alocacao %d falhou (sz=%lu)\n", i, sz);
			return 1;
		}
		unsigned char padrao = (unsigned char)((i * 37 + 13) & 0xFF);
		memset(ptrs[i], padrao, sz);
	}

	for (int i = 0; i < N; i++) {
		if (*((char *)(ptrs[i] - 9)) != 1) {
			printf("\t  -> bloco %d perdeu flag em uso\n", i);
			return 1;
		}
		long long real_sz = *((long long *)(ptrs[i] - 8));
		if (real_sz < (long long)sizes[i]) {
			printf("\t  -> bloco %d tem tamanho %lld < %lu\n", i, real_sz, sizes[i]);
			return 1;
		}
		for (int j = i + 1; j < N; j++) {
			if (blocos_sobrepoem(ptrs[i], sizes[i], ptrs[j], sizes[j])) {
				printf("\t  -> blocos %d (%p, %luB) e %d (%p, %luB) se sobrepoem!\n",
				       i, (void *)ptrs[i], sizes[i], j, (void *)ptrs[j], sizes[j]);
				return 1;
			}
		}
		unsigned char padrao = (unsigned char)((i * 37 + 13) & 0xFF);
		for (unsigned long k = 0; k < sizes[i]; k++) {
			if (ptrs[i][k] != padrao) {
				printf("\t  -> bloco %d corrompido no offset %lu\n", i, k);
				return 1;
			}
		}
	}
	return 0;
}

/* 20. Estresse aleatorio (Fuzzing deterministico com LCG, 1000 iteracoes) */
static int teste_estresse_aleatorio_fuzz(void) {
	enum { SLOTS = 64, ITERS = 1000 };
	static unsigned char *slot_ptr[SLOTS];
	static unsigned long slot_sz[SLOTS];
	static unsigned char slot_pat[SLOTS];
	memset(slot_ptr, 0, sizeof(slot_ptr));
	memset(slot_sz, 0, sizeof(slot_sz));
	memset(slot_pat, 0, sizeof(slot_pat));

	uint32_t rng = 12345u;
	for (int it = 0; it < ITERS; it++) {
		rng = rng * 1664525u + 1013904223u;
		int idx = (int)(rng % SLOTS);
		if (slot_ptr[idx] == NULL) {
			rng = rng * 1664525u + 1013904223u;
			unsigned long sz = 1UL + (unsigned long)(rng % 512u);
			unsigned char strat = (unsigned char)((rng >> 16) & 1u);
			unsigned char pat = (unsigned char)((it * 29 + idx) & 0xFF);
			unsigned char *p = (unsigned char *)memory_alloc(sz, strat);
			if (!p) {
				printf("\t  -> fuzz iter %d: memory_alloc(%lu, %u) retornou NULL\n",
				       it, sz, strat);
				return 1;
			}
			for (int j = 0; j < SLOTS; j++) {
				if (slot_ptr[j] && blocos_sobrepoem(p, sz, slot_ptr[j], slot_sz[j])) {
					printf("\t  -> fuzz iter %d: nova alocacao (%p, %luB) sobrepos slot %d (%p, %luB)\n",
					       it, (void *)p, sz, j, (void *)slot_ptr[j], slot_sz[j]);
					return 1;
				}
			}
			memset(p, pat, sz);
			slot_ptr[idx] = p;
			slot_sz[idx] = sz;
			slot_pat[idx] = pat;
		} else {
			for (unsigned long k = 0; k < slot_sz[idx]; k++) {
				if (slot_ptr[idx][k] != slot_pat[idx]) {
					printf("\t  -> fuzz iter %d: slot %d corrompido antes do free (offset %lu)\n",
					       it, idx, k);
					return 1;
				}
			}
			memory_free(slot_ptr[idx]);
			slot_ptr[idx] = NULL;
			slot_sz[idx] = 0;
		}
	}
	return 0;
}

/* 21. Estresse de fragmentacao extrema (split repetido de 20.000B em blocos de 10B) */
static int teste_estresse_fragmentacao_extrema(void) {
	enum { COUNT = 500 };
	static unsigned char *ptrs[COUNT];

	void *big = memory_alloc(20000, 0);
	if (!big) return 1;
	memory_free(big);

	for (int i = 0; i < COUNT; i++) {
		ptrs[i] = (unsigned char *)memory_alloc(10, 1);
		if (!ptrs[i]) {
			printf("\t  -> falha ao alocar 10B na iteracao %d\n", i);
			return 1;
		}
		memset(ptrs[i], (unsigned char)(i & 0xFF), 10);
		for (int j = 0; j < i; j++) {
			if (blocos_sobrepoem(ptrs[i], 10, ptrs[j], 10)) {
				printf("\t  -> bloco %d (%p) sobrepos bloco %d (%p) durante split\n",
				       i, (void *)ptrs[i], j, (void *)ptrs[j]);
				return 1;
			}
		}
	}
	for (int i = 0; i < COUNT; i++) {
		unsigned char esp = (unsigned char)(i & 0xFF);
		for (int k = 0; k < 10; k++) {
			if (ptrs[i][k] != esp) {
				printf("\t  -> corrupcao no bloco %d byte %d\n", i, k);
				return 1;
			}
		}
	}
	return 0;
}

/* 22. Estresse de volume (2000 alocacoes sequenciais + liberacao + dismiss_brk) */
static int teste_estresse_volume_2000_alocacoes(void) {
	enum { COUNT = 2000 };
	static unsigned char *ptrs[COUNT];
	static unsigned long sizes[COUNT];

	for (int i = 0; i < COUNT; i++) {
		unsigned long sz = 32UL;
		ptrs[i] = (unsigned char *)memory_alloc(sz, 0);
		sizes[i] = sz;
		if (!ptrs[i]) {
			printf("\t  -> alocacao %d falhou\n", i);
			return 1;
		}
		if (i > 0 && blocos_sobrepoem(ptrs[i - 1], sizes[i - 1], ptrs[i], sizes[i])) {
			printf("\t  -> volume iter %d: sobreposicao entre bloco %d (%p) e %d (%p)\n",
			       i, i - 1, (void *)ptrs[i - 1], i, (void *)ptrs[i]);
			return 1;
		}
		ptrs[i][0] = (unsigned char)(i & 0xFF);
		ptrs[i][sz - 1] = (unsigned char)((i >> 3) & 0xFF);
	}
	for (int i = 0; i < COUNT; i++) {
		if (ptrs[i][0] != (unsigned char)(i & 0xFF) ||
		    ptrs[i][sizes[i] - 1] != (unsigned char)((i >> 3) & 0xFF)) {
			printf("\t  -> volume: payload corrompido no bloco %d\n", i);
			return 1;
		}
		memory_free(ptrs[i]);
	}
	dismiss_brk();
	setup_brk();
	void *check = memory_alloc(64, 0);
	if (!check || (char *)check - 9 != (char *)original_brk) {
		printf("\t  -> falha ao reiniciar heap apos 2000 alocacoes\n");
		return 1;
	}
	return 0;
}

static void executar_testes_extras(void) {
	int passou = 0, falhou = 0, quebrou = 0;
	printf("\n==================== TESTES DE ESTRESSE, BORDA E QUEBRA ====================\n");

	rodar_teste("01. Sobreposicao de blocos ativos (cmpq $1 vs byte)", teste_sobreposicao_blocos_ativos, &passou, &falhou, &quebrou);
	rodar_teste("02. memory_free via ponteiro de funcao (ABI %rax vs %rdi)", teste_free_abi_ponteiro_funcao, &passou, &falhou, &quebrou);
	rodar_teste("03. memory_free com %rax sujo (ABI %rax vs %rdi)", teste_free_abi_rax_sujo, &passou, &falhou, &quebrou);
	rodar_teste("04. memory_free(NULL)", teste_free_null, &passou, &falhou, &quebrou);
	rodar_teste("05. memory_free abaixo de original_brk (.bss)", teste_free_abaixo_original_brk, &passou, &falhou, &quebrou);
	rodar_teste("06. memory_free endereco bit 63 = 1 (jg com sinal)", teste_free_endereco_bit63, &passou, &falhou, &quebrou);
	rodar_teste("07. memory_free exatamente em current_brk", teste_free_limites_exatos_brk, &passou, &falhou, &quebrou);
	rodar_teste("08. memory_free ponteiro interno ao bloco", teste_free_ponteiro_interno, &passou, &falhou, &quebrou);
	rodar_teste("09. memory_free retorno valido vs invalido", teste_free_retorno_valido_vs_invalido, &passou, &falhou, &quebrou);
	rodar_teste("10. memory_alloc tamanho gigante (falha de brk)", teste_alloc_brk_falha_tamanho_gigante, &passou, &falhou, &quebrou);
	rodar_teste("11. memory_alloc tamanho negativo (jl com sinal)", teste_alloc_tamanho_negativo_com_bloco_livre, &passou, &falhou, &quebrou);
	rodar_teste("12. memory_alloc overflow em size + 9", teste_alloc_overflow_size_mais_9, &passou, &falhou, &quebrou);
	rodar_teste("13. memory_alloc(0 bytes)", teste_alloc_zero_bytes, &passou, &falhou, &quebrou);
	rodar_teste("14. Strategy %sil com bits superiores de %rsi sujos", teste_strategy_rsi_bits_superiores_sujos, &passou, &falhou, &quebrou);
	rodar_teste("15. Limiar exato do split (sobra 9 vs 10)", teste_limites_split_9_vs_10, &passou, &falhou, &quebrou);
	rodar_teste("16. Best Fit vs Worst Fit com multiplos blocos e desempate", teste_best_fit_vs_worst_fit_multiplos_blocos, &passou, &falhou, &quebrou);
	rodar_teste("17. Coalescing de blocos livres adjacentes", teste_ausencia_coalescing, &passou, &falhou, &quebrou);
	rodar_teste("18. Ciclo de vida dismiss_brk e reuso", teste_dismiss_brk_e_reuso, &passou, &falhou, &quebrou);
	rodar_teste("19. Estresse: integridade de 200 blocos simultaneos", teste_estresse_integridade_200_blocos, &passou, &falhou, &quebrou);
	rodar_teste("20. Estresse: fuzzing aleatorio deterministico (1000 ops)", teste_estresse_aleatorio_fuzz, &passou, &falhou, &quebrou);
	rodar_teste("21. Estresse: fragmentacao extrema (500 splits de 10B)", teste_estresse_fragmentacao_extrema, &passou, &falhou, &quebrou);
	rodar_teste("22. Estresse: volume (2000 alocacoes e liberacoes)", teste_estresse_volume_2000_alocacoes, &passou, &falhou, &quebrou);

	printf("----------------------------------------------------------------------------\n");
	printf("RESUMO DOS TESTES EXTRAS: %d PASSOU | %d FALHOU | %d QUEBROU (TOTAL: %d)\n",
	       passou, falhou, quebrou, passou + falhou + quebrou);
	printf("============================================================================\n");
}
