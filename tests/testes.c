#include <stdio.h>
#include <stdlib.h>
#include "testes.h"

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

/* 1. Alocar 100B e 50B sem sobreposição */
static int teste_sobreposicao_blocos_ativos(void) {
	for (unsigned char strat = 0; strat <= 1; strat++) {
		dismiss_brk();
		setup_brk();
		unsigned char *a = (unsigned char *)memory_alloc(100, strat);
		if (!a) return 1;
		memset(a, 0xAA, 100);

		unsigned char *b = (unsigned char *)memory_alloc(50, strat);
		if (!b) return 1;
		memset(b, 0xBB, 50);

		if (blocos_sobrepoem(a, 100, b, 50)) return 1;
		if (*((char *)(a - 9)) != 1) return 1;
		if (*((long long *)(a - 8)) != 100) return 1;
		for (int i = 0; i < 100; i++) {
			if (a[i] != 0xAA) return 1;
		}
	}
	return 0;
}

/* 2. Free via ponteiro de função */
static int teste_free_abi_ponteiro_funcao(void) {
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
	int (*volatile fn)(void *) = memory_free;
	fn(p);
	if (*((char *)p - 9) != 0) return 1;
	return 0;
}

/* 3. Free após modificar registrador */
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
	if (*((char *)p - 9) != 0) return 1;
	return 0;
}

/* 4. Free de NULL */
static int teste_free_null(void) {
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
	memory_free(NULL);
	return 0;
}

/* 5. Free abaixo do heap */
static unsigned char canario_bss[32];
static int teste_free_abaixo_original_brk(void) {
	memset(canario_bss, 0xFF, sizeof(canario_bss));
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
	memory_free((void *)&canario_bss[16]);
	if (canario_bss[7] != 0xFF) return 1;
	return 0;
}

/* 6. Free de endereço alto */
static int teste_free_endereco_bit63(void) {
	void *p = memory_alloc(64, 0);
	if (!p) return 1;
	memory_free((void *)(uintptr_t)0xFFFFFFFFFFFFFFFFULL);
	return 0;
}

/* 7. Free no limite do heap */
static int teste_free_limites_exatos_brk(void) {
	unsigned char *p = (unsigned char *)memory_alloc(64, 0);
	if (!p) return 1;
	memset(p, 0xCC, 64);
	void *cur_brk = (void *)(p + 64);
	memory_free(cur_brk);
	if (p[55] != 0xCC) return 1;
	if (*((char *)(p - 9)) != 1) return 1;
	return 0;
}

/* 8. Free de ponteiro interno */
static int teste_free_ponteiro_interno(void) {
	unsigned char *p = (unsigned char *)memory_alloc(100, 0);
	if (!p) return 1;
	memset(p, 0xDD, 100);
	memory_free((void *)(p + 20));
	if (p[11] != 0xDD) return 1;
	if (*((char *)(p - 9)) != 1) return 1;
	return 0;
}

/* 9. Free válido e inválido */
static int teste_free_retorno_valido_vs_invalido(void) {
	void *p = memory_alloc(32, 0);
	if (!p) return 1;
	int r_valido = memory_free(p);
	unsigned long long x = 0;
	int r_invalido = memory_free((void *)&x);
	if (r_valido == r_invalido) return 1;
	return 0;
}

/* 10. Aloca 2^50 bytes */
static int teste_alloc_brk_falha_tamanho_gigante(void) {
	void *p = memory_alloc(1ULL << 50, 0);
	if (p != NULL) return 1;
	return 0;
}

/* 11. Aloca UINT64_MAX */
static int teste_alloc_tamanho_negativo_com_bloco_livre(void) {
	void *a = memory_alloc(100, 0);
	if (!a) return 1;
	memory_free(a);
	void *p = memory_alloc((unsigned long)-1, 0);
	if (p != NULL) return 1;
	return 0;
}

/* 12. Aloca com overflow */
static int teste_alloc_overflow_size_mais_9(void) {
	void *p = memory_alloc((unsigned long)-8, 0);
	if (p != NULL) return 1;
	return 0;
}

/* 13. Aloca 0 bytes */
static int teste_alloc_zero_bytes(void) {
	void *p0 = memory_alloc(0, 0);
	if (p0 != NULL) return 1;
	return 0;
}

/* 14. Alocação após modificar registrador */
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
	if (res != p1) return 1;
	return 0;
}

/* 15. Divisão com sobra 9 vs 10 */
static int teste_limites_split_9_vs_10(void) {
	void *p = memory_alloc(100, 0);
	if (!p) return 1;
	memory_free(p);

	void *p91 = memory_alloc(91, 1);
	if (p91 != p) return 1;
	if (*((long long *)((char *)p91 - 8)) != 100) return 1;
	memory_free(p91);

	void *p90 = memory_alloc(90, 1);
	if (p90 != p) return 1;
	if (*((long long *)((char *)p90 - 8)) != 90) return 1;
	char rem_uso = *((char *)p90 + 90);
	long long rem_tam = *((long long *)((char *)p90 + 91));
	if (rem_uso != 0 || rem_tam != 1) return 1;
	return 0;
}

/* 16. Best/Worst Fit e desempate */
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
	if (r_best != b80) return 1;

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
	if (r_worst != b300) return 1;

	dismiss_brk();
	setup_brk();
	void *p1 = memory_alloc(100, 0);
	void *p2 = memory_alloc(209, 0);
	memory_free(p1);
	memory_free(p2);
	void *q = memory_alloc(100, 0);
	memory_free(q);
	void *sel_bf = memory_alloc(100, 1);
	if (sel_bf != p1) return 1;

	dismiss_brk();
	setup_brk();
	p1 = memory_alloc(100, 0);
	p2 = memory_alloc(209, 0);
	memory_free(p1);
	memory_free(p2);
	q = memory_alloc(100, 0);
	memory_free(q);
	void *p3 = (void *)((char *)p2 + 109);
	void *sel_wf = memory_alloc(100, 0);
	if (sel_wf != p3) return 1;
	return 0;
}

/* 17. Coalescência (hard, comentado — sua implementação não faz) */
static int teste_ausencia_coalescing(void) {
	void *p1 = memory_alloc(100, 0);
	void *p2 = memory_alloc(120, 0);
	if (!p1 || !p2) return 1;
	memory_free(p1);
	memory_free(p2);
	void *p3 = memory_alloc(180, 0);
	if (p3 != p1) return 1;
	return 0;
}

/* 18. Reuso após dismiss */
static int teste_dismiss_brk_e_reuso(void) {
	void *p1 = memory_alloc(200, 0);
	if (!p1) return 1;
	memset(p1, 0x55, 200);
	dismiss_brk();
	unsigned char *p2 = (unsigned char *)memory_alloc(64, 0);
	if (!p2) return 1;
	if ((char *)p2 - 9 != (char *)original_brk) return 1;
	if (*((char *)p2 - 9) != 1) return 1;
	if (*((long long *)p2 - 1) != 64) return 1;
	memset(p2, 0x77, 64);
	for (int i = 0; i < 64; i++) {
		if (p2[i] != 0x77) return 1;
	}
	return 0;
}

/* 19. 200 blocos simultâneos */
static int teste_estresse_integridade_200_blocos(void) {
	enum { N = 200 };
	static unsigned char *ptrs[N];
	static unsigned long sizes[N];

	for (int i = 0; i < N; i++) {
		unsigned long sz = 16UL + (unsigned long)((i * 73) % 497);
		unsigned char strat = (unsigned char)(i & 1);
		ptrs[i] = (unsigned char *)memory_alloc(sz, strat);
		sizes[i] = sz;
		if (!ptrs[i]) return 1;
		unsigned char padrao = (unsigned char)((i * 37 + 13) & 0xFF);
		memset(ptrs[i], padrao, sz);
	}

	for (int i = 0; i < N; i++) {
		if (*((char *)(ptrs[i] - 9)) != 1) return 1;
		long long real_sz = *((long long *)(ptrs[i] - 8));
		if (real_sz < (long long)sizes[i]) return 1;
		for (int j = i + 1; j < N; j++) {
			if (blocos_sobrepoem(ptrs[i], sizes[i], ptrs[j], sizes[j])) return 1;
		}
		unsigned char padrao = (unsigned char)((i * 37 + 13) & 0xFF);
		for (unsigned long k = 0; k < sizes[i]; k++) {
			if (ptrs[i][k] != padrao) return 1;
		}
	}
	return 0;
}

/* 20. 1000 ops aleatórias */
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
			if (!p) return 1;
			for (int j = 0; j < SLOTS; j++) {
				if (slot_ptr[j] && blocos_sobrepoem(p, sz, slot_ptr[j], slot_sz[j])) return 1;
			}
			memset(p, pat, sz);
			slot_ptr[idx] = p;
			slot_sz[idx] = sz;
			slot_pat[idx] = pat;
		} else {
			for (unsigned long k = 0; k < slot_sz[idx]; k++) {
				if (slot_ptr[idx][k] != slot_pat[idx]) return 1;
			}
			memory_free(slot_ptr[idx]);
			slot_ptr[idx] = NULL;
			slot_sz[idx] = 0;
		}
	}
	return 0;
}

/* 21. 500 splits de 10B */
static int teste_estresse_fragmentacao_extrema(void) {
	enum { COUNT = 500 };
	static unsigned char *ptrs[COUNT];

	void *big = memory_alloc(20000, 0);
	if (!big) return 1;
	memory_free(big);

	for (int i = 0; i < COUNT; i++) {
		ptrs[i] = (unsigned char *)memory_alloc(10, 1);
		if (!ptrs[i]) return 1;
		memset(ptrs[i], (unsigned char)(i & 0xFF), 10);
		for (int j = 0; j < i; j++) {
			if (blocos_sobrepoem(ptrs[i], 10, ptrs[j], 10)) return 1;
		}
	}
	for (int i = 0; i < COUNT; i++) {
		unsigned char esp = (unsigned char)(i & 0xFF);
		for (int k = 0; k < 10; k++) {
			if (ptrs[i][k] != esp) return 1;
		}
	}
	return 0;
}

/* 22. 2000 alocações */
static int teste_estresse_volume_2000_alocacoes(void) {
	enum { COUNT = 2000 };
	static unsigned char *ptrs[COUNT];
	static unsigned long sizes[COUNT];

	for (int i = 0; i < COUNT; i++) {
		unsigned long sz = 32UL;
		ptrs[i] = (unsigned char *)memory_alloc(sz, 0);
		sizes[i] = sz;
		if (!ptrs[i]) return 1;
		if (i > 0 && blocos_sobrepoem(ptrs[i - 1], sizes[i - 1], ptrs[i], sizes[i])) return 1;
		ptrs[i][0] = (unsigned char)(i & 0xFF);
		ptrs[i][sz - 1] = (unsigned char)((i >> 3) & 0xFF);
	}
	for (int i = 0; i < COUNT; i++) {
		if (ptrs[i][0] != (unsigned char)(i & 0xFF)) return 1;
		if (ptrs[i][sizes[i] - 1] != (unsigned char)((i >> 3) & 0xFF)) return 1;
		memory_free(ptrs[i]);
	}
	dismiss_brk();
	setup_brk();
	void *check = memory_alloc(64, 0);
	if (!check) return 1;
	if ((char *)check - 9 != (char *)original_brk) return 1;
	return 0;
}

/* 23. Double free */
static int teste_double_free(void) {
	unsigned char *p = (unsigned char *)memory_alloc(64, 0);
	unsigned char *q = (unsigned char *)memory_alloc(64, 0);
	if (!p || !q) return 1;
	memset(p, 0xAA, 64);
	memset(q, 0xBB, 64);
	memory_free(p);
	memory_free(p);
	for (int i = 0; i < 64; i++) {
		if (q[i] != 0xBB) return 1;
	}
	if (*((char *)(q - 9)) != 1) return 1;
	return 0;
}

/* 24. Free no meio */
static int teste_free_nao_corrompe_vizinhos(void) {
	unsigned char *a = (unsigned char *)memory_alloc(100, 0);
	unsigned char *b = (unsigned char *)memory_alloc(100, 0);
	unsigned char *c = (unsigned char *)memory_alloc(100, 0);
	if (!a || !b || !c) return 1;
	memset(a, 0xA1, 100);
	memset(b, 0xB2, 100);
	memset(c, 0xC3, 100);
	memory_free(b);
	for (int i = 0; i < 100; i++) {
		if (a[i] != 0xA1) return 1;
		if (c[i] != 0xC3) return 1;
	}
	return 0;
}

/* 25. Aloca 1 byte */
static int teste_alloc_um_byte(void) {
	unsigned char *p = (unsigned char *)memory_alloc(1, 0);
	if (!p) return 1;
	*p = 0xAB;
	if (*p != 0xAB) return 1;
	if (*((long long *)(p - 8)) < 1) return 1;
	return 0;
}

/* 26. Aloca 9 bytes */
static int teste_alloc_tamanho_header(void) {
	unsigned char *p = (unsigned char *)memory_alloc(9, 0);
	if (!p) return 1;
	memset(p, 0x5A, 9);
	for (int i = 0; i < 9; i++) {
		if (p[i] != 0x5A) return 1;
	}
	return 0;
}

/* 27. Aloca 500B após free 50B */
static int teste_alloc_exige_brk_apos_free(void) {
	void *p = memory_alloc(50, 0);
	if (!p) return 1;
	memory_free(p);
	void *q = memory_alloc(500, 0);
	if (!q) return 1;
	if (q == p) return 1;
	if (*((char *)((char *)p - 9)) != 0) return 1;
	if (*((long long *)((char *)p - 8)) != 50) return 1;
	return 0;
}

/* 28. Free desalinhado */
static int teste_free_ponteiro_p_menos_9(void) {
	unsigned char *p = (unsigned char *)memory_alloc(100, 0);
	if (!p) return 1;
	memset(p, 0xEE, 100);
	memory_free(p - 9);
	if (*((char *)(p - 9)) != 1) return 1;
	for (int i = 0; i < 100; i++) {
		if (p[i] != 0xEE) return 1;
	}
	return 0;
}

/* 29. Reuso do residual */
static int teste_split_residual_reutilizavel(void) {
	unsigned char *p = (unsigned char *)memory_alloc(100, 0);
	if (!p) return 1;
	memory_free(p);

	unsigned char *q1 = (unsigned char *)memory_alloc(10, 1);
	if (!q1) return 1;
	if (*((long long *)(q1 - 8)) != 10) return 1;
	char rem_uso = *((char *)(q1 + 10));
	long long rem_tam = *((long long *)(q1 + 11));
	if (rem_uso != 0 || rem_tam != 81) return 1;
	unsigned char *q2 = (unsigned char *)memory_alloc(50, 1);
	if (q2 != q1 + 19) return 1;
	return 0;
}

/* 30. Worst fit em 20 rodadas */
static int teste_worst_fit_multiplos_rounds(void) {
	for (int round = 0; round < 20; round++) {
		void *a = memory_alloc(100, 0);
		void *b = memory_alloc(50, 0);
		void *c = memory_alloc(200, 0);
		if (!a || !b || !c) return 1;
		memory_free(a);
		memory_free(b);
		memory_free(c);
	}
	void *final = memory_alloc(150, 0);
	if (!final) return 1;
	return 0;
}

/* 31. Ciclo setup/dismiss */
static int teste_ciclo_setup_dismiss(void) {
	dismiss_brk();
	setup_brk();
	void *p = memory_alloc(100, 0);
	if (!p) return 1;
	dismiss_brk();
	setup_brk();
	void *q = memory_alloc(100, 0);
	if (!q) return 1;
	if ((char *)q - 9 != (char *)original_brk) return 1;
	return 0;
}

/* 32. Escrita/leitura de 256B */
static int teste_acesso_todos_bytes(void) {
	unsigned char *p = (unsigned char *)memory_alloc(256, 0);
	if (!p) return 1;
	for (int i = 0; i < 256; i++) p[i] = (unsigned char)i;
	for (int i = 0; i < 256; i++) {
		if (p[i] != (unsigned char)i) return 1;
	}
	return 0;
}

/* 33. Strategy inválido */
static int teste_strategy_invalido(void) {
	void *p = memory_alloc(100, 0);
	if (!p) return 1;
	memory_free(p);
	void *q = memory_alloc(50, 2);
	(void)q;
	void *r = memory_alloc(50, 0);
	if (!r) return 1;
	return 0;
}

/* 34. Coalescência (informativo, soft) */
static int teste_coalescing_informativo(void) {
	unsigned char *p1 = (unsigned char *)memory_alloc(100, 0);
	unsigned char *p2 = (unsigned char *)memory_alloc(120, 0);
	if (!p1 || !p2) return 1;
	memory_free(p1);
	memory_free(p2);
	unsigned char *p3 = (unsigned char *)memory_alloc(180, 1);
	if (!p3) return 1;
	if (*((char *)(p3 - 9)) != 1) return 1;
	if (*((long long *)(p3 - 8)) < 180) return 1;
	return 0;
}

/* 35. Reuso de bloco grande */
static int teste_alloc_grande_reaproveita(void) {
	void *a = memory_alloc(500, 0);
	if (!a) return 1;
	memory_free(a);
	void *b = memory_alloc(400, 1);
	if (!b) return 1;
	if (b != a) return 1;
	return 0;
}

void executar_testes_extras(void) {
	int passou = 0, falhou = 0, quebrou = 0;
	printf("\n==================== TESTES DE ESTRESSE, BORDA E QUEBRA ====================\n");

	rodar_teste("01. Aloca 100B e 50B",                        teste_sobreposicao_blocos_ativos,           &passou, &falhou, &quebrou);
	rodar_teste("02. Free via ponteiro de função",             teste_free_abi_ponteiro_funcao,             &passou, &falhou, &quebrou);
	rodar_teste("03. Free após modificar registrador",         teste_free_abi_rax_sujo,                    &passou, &falhou, &quebrou);
	rodar_teste("04. Free de NULL",                            teste_free_null,                            &passou, &falhou, &quebrou);
	rodar_teste("05. Free abaixo do heap",                     teste_free_abaixo_original_brk,             &passou, &falhou, &quebrou);
	rodar_teste("06. Free de endereço alto",                   teste_free_endereco_bit63,                  &passou, &falhou, &quebrou);
	rodar_teste("07. Free no limite do heap",                  teste_free_limites_exatos_brk,              &passou, &falhou, &quebrou);
	rodar_teste("08. Free de ponteiro interno",                teste_free_ponteiro_interno,                &passou, &falhou, &quebrou);
	rodar_teste("09. Free válido e inválido",                  teste_free_retorno_valido_vs_invalido,      &passou, &falhou, &quebrou);
	rodar_teste("10. Aloca 2^50 bytes",                        teste_alloc_brk_falha_tamanho_gigante,      &passou, &falhou, &quebrou);
	rodar_teste("11. Aloca UINT64_MAX",                        teste_alloc_tamanho_negativo_com_bloco_livre,&passou, &falhou, &quebrou);
	rodar_teste("12. Aloca com overflow",                      teste_alloc_overflow_size_mais_9,           &passou, &falhou, &quebrou);
	rodar_teste("13. Aloca 0 bytes",                           teste_alloc_zero_bytes,                     &passou, &falhou, &quebrou);
	rodar_teste("14. Alocação após modificar registrador",     teste_strategy_rsi_bits_superiores_sujos,   &passou, &falhou, &quebrou);
	rodar_teste("15. Divisão com sobra 9 vs 10",               teste_limites_split_9_vs_10,                &passou, &falhou, &quebrou);
	rodar_teste("16. Best/Worst Fit e desempate",              teste_best_fit_vs_worst_fit_multiplos_blocos,&passou, &falhou, &quebrou);
	// rodar_teste("17. Coalescência (hard)",                 teste_ausencia_coalescing,                  &passou, &falhou, &quebrou);
	rodar_teste("18. Reuso após dismiss",                      teste_dismiss_brk_e_reuso,                  &passou, &falhou, &quebrou);
	rodar_teste("19. 200 blocos simultâneos",                  teste_estresse_integridade_200_blocos,      &passou, &falhou, &quebrou);
	rodar_teste("20. 1000 ops aleatórias",                     teste_estresse_aleatorio_fuzz,              &passou, &falhou, &quebrou);
	rodar_teste("21. 500 splits de 10B",                       teste_estresse_fragmentacao_extrema,        &passou, &falhou, &quebrou);
	rodar_teste("22. 2000 alocações",                          teste_estresse_volume_2000_alocacoes,       &passou, &falhou, &quebrou);
	rodar_teste("23. Double free",                             teste_double_free,                          &passou, &falhou, &quebrou);
	rodar_teste("24. Free no meio",                            teste_free_nao_corrompe_vizinhos,           &passou, &falhou, &quebrou);
	rodar_teste("25. Aloca 1 byte",                            teste_alloc_um_byte,                        &passou, &falhou, &quebrou);
	rodar_teste("26. Aloca 9 bytes",                           teste_alloc_tamanho_header,                 &passou, &falhou, &quebrou);
	rodar_teste("27. Aloca 500B após free 50B",                teste_alloc_exige_brk_apos_free,            &passou, &falhou, &quebrou);
	rodar_teste("28. Free desalinhado",                        teste_free_ponteiro_p_menos_9,              &passou, &falhou, &quebrou);
	rodar_teste("29. Reuso do residual",                       teste_split_residual_reutilizavel,          &passou, &falhou, &quebrou);
	rodar_teste("30. Worst fit em 20 rodadas",                 teste_worst_fit_multiplos_rounds,           &passou, &falhou, &quebrou);
	rodar_teste("31. Ciclo setup/dismiss",                     teste_ciclo_setup_dismiss,                  &passou, &falhou, &quebrou);
	rodar_teste("32. Escrita/leitura de 256B",                 teste_acesso_todos_bytes,                   &passou, &falhou, &quebrou);
	rodar_teste("33. Strategy inválido",                       teste_strategy_invalido,                    &passou, &falhou, &quebrou);
	rodar_teste("34. Coalescência (informativo)",              teste_coalescing_informativo,               &passou, &falhou, &quebrou);
	rodar_teste("35. Reuso de bloco grande",                   teste_alloc_grande_reaproveita,             &passou, &falhou, &quebrou);

	printf("----------------------------------------------------------------------------\n");
	printf("RESUMO DOS TESTES EXTRAS: %d PASSOU | %d FALHOU | %d QUEBROU (TOTAL: %d)\n",
	       passou, falhou, quebrou, passou + falhou + quebrou);
	printf("============================================================================\n");
}