#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include "hash_utils.h"

#define MAX_WORKERS 16
#define RESULT_FILE "password_found.txt"

/**
 * Calcula o tamanho total do espaço de busca
 */
unsigned long long calculate_search_space(int charset_len, int password_len) {
    unsigned long long total = 1;
    for (int i = 0; i < password_len; i++) {
        total *= (unsigned long long)charset_len;
    }
    return total;
}

/**
 * Converte um índice numérico para uma senha
 */
void index_to_password(unsigned long long index, const char *charset, int charset_len,
                       int password_len, char *output) {
    for (int i = password_len - 1; i >= 0; i--) {
        output[i] = charset[index % charset_len];
        index /= charset_len;
    }
    output[password_len] = '\0';
}

/**
 * Função principal do coordenador
 */
int main(int argc, char *argv[]) {
    // TODO 1: Validar argumentos de entrada
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <hash_md5> <tamanho> <charset> <num_workers>\n", argv[0]);
        fprintf(stderr, "Ex: %s \"900150983cd24fb0d6963f7d28e17f72\" 3 \"abc\" 4\n", argv[0]);
        return 1;
    }

    // Parsing dos argumentos (após validação)
    const char *target_hash = argv[1];
    int password_len = atoi(argv[2]);
    const char *charset = argv[3];
    int num_workers = atoi(argv[4]);
    int charset_len = (int)strlen(charset);

    // Validações dos parâmetros
    if (password_len < 1 || password_len > 10) {
        fprintf(stderr, "Erro: password_len deve estar entre 1 e 10 (valor atual: %d)\n", password_len);
        return 1;
    }
    if (num_workers < 1 || num_workers > MAX_WORKERS) {
        fprintf(stderr, "Erro: num_workers deve estar entre 1 e %d (valor atual: %d)\n", MAX_WORKERS, num_workers);
        return 1;
    }
    if (charset_len < 1) {
        fprintf(stderr, "Erro: charset não pode ser vazio\n");
        return 1;
    }
    if ((int)strlen(target_hash) != 32) {
        fprintf(stderr, "Aviso: o hash alvo não tem 32 caracteres hex (verifique se é MD5).\n");
    }

    printf("=== Mini-Projeto 1: Quebra de Senhas Paralelo ===\n");
    printf("Hash MD5 alvo: %s\n", target_hash);
    printf("Tamanho da senha: %d\n", password_len);
    printf("Charset: %s (tamanho: %d)\n", charset, charset_len);
    printf("Número de workers: %d\n", num_workers);

    // Calcular espaço de busca total
    unsigned long long total_space = calculate_search_space(charset_len, password_len);
    printf("Espaço de busca total: %llu combinações\n\n", total_space);

    // Remover arquivo de resultado anterior se existir
    unlink(RESULT_FILE);

    // Registrar tempo de início
    time_t start_time = time(NULL);

    // TODO 2: Dividir o espaço de busca entre os workers
    unsigned long long passwords_per_worker = total_space / (unsigned long long)num_workers;
    unsigned long long remaining = total_space % (unsigned long long)num_workers;

    // Arrays para armazenar PIDs dos workers
    pid_t workers[MAX_WORKERS];
    for (int i = 0; i < MAX_WORKERS; ++i) workers[i] = -1;

    // TODO 3: Criar os processos workers usando fork()
    printf("Iniciando workers...\n");

    for (int i = 0; i < num_workers; i++) {
        // calcular quantos elementos este worker vai processar
        unsigned long long chunk = passwords_per_worker + (i < (int)remaining ? 1ULL : 0ULL);

        // calcular start e end (end é exclusivo)
        unsigned long long start_idx = 0;
        if (i == 0) start_idx = 0;
        else {
            // soma dos chunks anteriores = i * passwords_per_worker + min(i, remaining)
            start_idx = (unsigned long long)i * passwords_per_worker + (unsigned long long)(i < (int)remaining ? i : remaining);
        }
        unsigned long long end_idx = start_idx + chunk; // exclusive

        // proteger caso chunk == 0 (quando total_space < num_workers)
        if (chunk == 0) {
            start_idx = end_idx = 0;
        }

        // converter indices para string
        char start_str[32], end_str[32], pwlen_str[8];
        snprintf(start_str, sizeof(start_str), "%llu", (unsigned long long)start_idx);
        snprintf(end_str, sizeof(end_str), "%llu", (unsigned long long)end_idx);
        snprintf(pwlen_str, sizeof(pwlen_str), "%d", password_len);

        pid_t pid = fork();
        if (pid < 0) {
            // erro ao forkar
            perror("fork");
            workers[i] = -1;
            continue;
        } else if (pid == 0) {
            // processo filho: executar worker via execl
            // Assumimos que o binário 'worker' aceita:
            // worker <hash_target> <password_len> <charset> <start_idx> <end_idx>
            execl("./worker", "worker", target_hash, pwlen_str, charset, start_str, end_str, (char *)NULL);
            // se execl retornar, houve erro
            perror("execl");
            _exit(1);
        } else {
            // processo pai: armazenar PID
            workers[i] = pid;
            printf("Worker %d criado: PID=%d intervalo: %s (inclusive) .. %s (exclusive)\n", i, pid, start_str, end_str);
        }
    }

    printf("\nTodos os workers foram iniciados. Aguardando conclusão...\n");

    // TODO 8: Aguardar todos os workers terminarem usando wait()
    int finished_workers = 0;
    while (finished_workers < num_workers) {
        int status;
        pid_t w = wait(&status);
        if (w == -1) {
            if (errno == ECHILD) break; // sem filhos restantes
            perror("wait");
            break;
        }

        // identificar qual worker corresponde a este pid (opcional)
        int worker_id = -1;
        for (int j = 0; j < num_workers; ++j) {
            if (workers[j] == w) { worker_id = j; break; }
        }

        if (WIFEXITED(status)) {
            int exitcode = WEXITSTATUS(status);
            if (worker_id >= 0)
                printf("Worker %d (PID %d) terminou normalmente com código %d\n", worker_id, w, exitcode);
            else
                printf("Filho PID %d terminou normalmente com código %d\n", w, exitcode);
        } else if (WIFSIGNALED(status)) {
            int signum = WTERMSIG(status);
            if (worker_id >= 0)
                printf("Worker %d (PID %d) terminou por sinal %d\n", worker_id, w, signum);
            else
                printf("Filho PID %d terminou por sinal %d\n", w, signum);
        } else {
            printf("Filho PID %d terminou com status não tratado\n", w);
        }

        finished_workers++;
    }

    // Registrar tempo de fim
    time_t end_time = time(NULL);
    double elapsed_time = difftime(end_time, start_time);

    printf("\n=== Resultado ===\n");

    // TODO 9: Verificar se algum worker encontrou a senha
    int fd = open(RESULT_FILE, O_RDONLY);
    if (fd < 0) {
        printf("Nenhum worker criou '%s'. Senha não encontrada.\n", RESULT_FILE);
        printf("Tempo total: %.2f segundos\n", elapsed_time);
        return 0;
    }

    // ler conteúdo
    char buffer[512];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (n <= 0) {
        printf("Arquivo '%s' presente, mas vazio.\n", RESULT_FILE);
        printf("Tempo total: %.2f segundos\n", elapsed_time);
        return 0;
    }
    buffer[n] = '\0';
    // remover newline
    char *nl = strchr(buffer, '\n');
    if (nl) *nl = '\0';

    // validar a senha lida calculando o hash e comparando
    char chk[33];
    compute_md5(buffer, chk);
    if (strcmp(chk, target_hash) == 0) {
        printf("✓ Senha encontrada: %s\n", buffer);
    } else {
        printf("Arquivo de resultado contém '%s', mas o hash não confere (hash calculado: %s)\n", buffer, chk);
    }

    printf("Tempo total: %.2f segundos\n", elapsed_time);
    return 0;
}
