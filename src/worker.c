#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include "hash_utils.h"

#define RESULT_FILE "password_found.txt"
#define PROGRESS_INTERVAL 100000

/**
 * Incrementa uma senha lexicograficamente.
 */
int increment_password(char *password, const char *charset, int charset_len, int password_len) {
    for (int pos = password_len - 1; pos >= 0; pos--) {
        char *p = strchr(charset, password[pos]);
        if (!p) return 0; // caractere inválido

        int idx = (int)(p - charset);
        if (idx + 1 < charset_len) {
            password[pos] = charset[idx + 1];
            return 1; // incremento bem sucedido
        } else {
            password[pos] = charset[0]; // overflow: resetar posição
        }
    }
    return 0; // chegou ao limite
}

/**
 * Compara duas senhas lexicograficamente.
 */
int password_compare(const char *a, const char *b) {
    return strcmp(a, b);
}

/**
 * Verifica se o arquivo de resultado já existe.
 */
int check_result_exists() {
    return access(RESULT_FILE, F_OK) == 0;
}

/**
 * Salva resultado de forma atômica.
 */
void save_result(int worker_id, const char *password) {
    int fd = open(RESULT_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd >= 0) {
        char buffer[256];
        int len = snprintf(buffer, sizeof(buffer), "%d:%s\n", worker_id, password);
        write(fd, buffer, len);
        close(fd);
    }
}

/**
 * Função principal do worker.
 */
int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Uso interno: %s <hash> <start> <end> <charset> <len> <id>\n", argv[0]);
        return 1;
    }

    const char *target_hash = argv[1];
    char *start_password = argv[2];
    const char *end_password = argv[3];
    const char *charset = argv[4];
    int password_len = atoi(argv[5]);
    int worker_id = atoi(argv[6]);
    int charset_len = strlen(charset);

    printf("[Worker %d] Iniciado: %s até %s\n", worker_id, start_password, end_password);

    char current_password[11];
    strcpy(current_password, start_password);

    char computed_hash[33];
    long long passwords_checked = 0;
    time_t start_time = time(NULL);

    while (1) {
        if (passwords_checked % PROGRESS_INTERVAL == 0) {
            if (check_result_exists()) {
                printf("[Worker %d] Outro worker encontrou a senha. Encerrando.\n", worker_id);
                break;
            }
        }

        md5_string(current_password, computed_hash);

        if (strcmp(computed_hash, target_hash) == 0) {
            printf("[Worker %d] Senha encontrada: %s\n", worker_id, current_password);
            save_result(worker_id, current_password);
            break;
        }

        if (!increment_password(current_password, charset, charset_len, password_len)) {
            printf("[Worker %d] Fim do intervalo.\n", worker_id);
            break;
        }

        if (password_compare(current_password, end_password) > 0) {
            break;
        }

        passwords_checked++;
    }

    time_t end_time = time(NULL);
    double total_time = difftime(end_time, start_time);

    printf("[Worker %d] Finalizado. Total: %lld senhas em %.2f segundos", worker_id, passwords_checked, total_time);
    if (total_time > 0) {
        printf(" (%.0f senhas/s)", passwords_checked / total_time);
    }
    printf("\n");

    return 0;
}
