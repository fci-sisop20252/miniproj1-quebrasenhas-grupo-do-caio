#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include "hash_utils.h"

#define MAX_WORKERS 16
#define RESULT_FILE "password_found.txt"

long long calculate_search_space(int charset_len, int password_len) {
    long long total = 1;
    for (int i = 0; i < password_len; i++) {
        total *= charset_len;
    }
    return total;
}

void index_to_password(long long index, const char *charset, int charset_len, 
                       int password_len, char *output) {
    for (int i = password_len - 1; i >= 0; i--) {
        output[i] = charset[index % charset_len];
        index /= charset_len;
    }
    output[password_len] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <hash_md5> <tamanho> <charset> <num_workers>\n", argv[0]);
        return 1;
    }

    const char *target_hash = argv[1];
    int password_len = atoi(argv[2]);
    const char *charset = argv[3];
    int num_workers = atoi(argv[4]);
    int charset_len = strlen(charset);

    if (password_len < 1 || password_len > 10) {
        printf("Erro: tamanho da senha deve estar entre 1 e 10\n");
        return 1;
    }
    if (num_workers < 1 || num_workers > MAX_WORKERS) {
        printf("Erro: número de workers deve estar entre 1 e %d\n", MAX_WORKERS);
        return 1;
    }
    if (charset_len == 0) {
        printf("Erro: charset não pode ser vazio\n");
        return 1;
    }

    printf("=== Mini-Projeto 1: Quebra de Senhas Paralelo ===\n");
    printf("Hash MD5 alvo: %s\n", target_hash);
    printf("Tamanho da senha: %d\n", password_len);
    printf("Charset: %s (tamanho: %d)\n", charset, charset_len);
    printf("Número de workers: %d\n", num_workers);

    long long total_space = calculate_search_space(charset_len, password_len);
    printf("Espaço de busca total: %lld combinações\n\n", total_space);

    unlink(RESULT_FILE);

    time_t start_time = time(NULL);

    long long passwords_per_worker = total_space / num_workers;
    long long remaining = total_space % num_workers;

    pid_t workers[MAX_WORKERS];

    printf("Iniciando workers...\n");

    for (int i = 0; i < num_workers; i++) {
        long long start_index = i * passwords_per_worker + (i < remaining ? i : remaining);
        long long end_index = start_index + passwords_per_worker - 1;
        if (i < remaining) end_index++;

        char start_pass[password_len + 1];
        char end_pass[password_len + 1];

        index_to_password(start_index, charset, charset_len, password_len, start_pass);
        index_to_password(end_index, charset, charset_len, password_len, end_pass);

        pid_t pid = fork();
        if (pid < 0) {
            perror("Erro no fork");
            exit(1);
        }
        if (pid == 0) {
            execl("./worker", "./worker", target_hash, start_pass, end_pass, charset, NULL);
            perror("Erro no execl");
            exit(1);
        } else {
            workers[i] = pid;
        }
    }

    int finished_workers = 0;
    while (finished_workers < num_workers) {
        int status;
        pid_t pid = wait(&status);
        if (pid > 0) finished_workers++;
    }

    time_t end_time = time(NULL);
    double elapsed_time = difftime(end_time, start_time);

    printf("\n=== Resultado ===\n");

    FILE *f = fopen(RESULT_FILE, "r");
    if (f) {
        char line[128], found_pass[128];
        if (fgets(line, sizeof(line), f)) {
            sscanf(line, "%*d:%s", found_pass);
            char check_hash[33];
            md5_string(found_pass, check_hash);
            if (strcmp(check_hash, target_hash) == 0) {
                printf("Senha encontrada: %s\n", found_pass);
            } else {
                printf("Arquivo de resultado corrompido.\n");
            }
        }
        fclose(f);
    } else {
        printf("Senha não encontrada.\n");
    }

    printf("Tempo total: %.2f segundos\n", elapsed_time);

    return 0;
}
