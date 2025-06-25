// src/main.c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h> // Para atoi
#include "fs_core.h"
#include "commands.h"
#include "fs_types.h"

#define DISK_PATH "meu_sistema.disk"

void prompt() {
    char path_buffer[1024];
    // Chama uma nova função do core para obter o caminho completo
    fs_get_current_path(path_buffer, sizeof(path_buffer));
    printf("fs:%s$ ", path_buffer);
}

void shell() {
    char linha[1024];
    printf("Shell do sistema de arquivos iniciado. Digite 'exit' para sair.\n");

    while (1) {
        prompt();
        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            printf("exit\n");
            break;
        }

        // Remove o \n do final da linha
        linha[strcspn(linha, "\n")] = 0;

        char *cmd = strtok(linha, " \t");
        if (!cmd) continue; // Linha em branco

        if (strcmp(cmd, "exit") == 0) {
            break;
        } else if (strcmp(cmd, "ls") == 0) {
            cmd_ls();
        } else if (strcmp(cmd, "df") == 0) {
            cmd_df();
        } else if (strcmp(cmd, "mkdir") == 0) {
            char *arg1 = strtok(NULL, " \t");
            if (arg1) cmd_mkdir(arg1); else printf("Uso: mkdir <nome_dir>\n");
        } else if (strcmp(cmd, "cd") == 0) {
            char *arg1 = strtok(NULL, " \t");
            if (arg1) cmd_cd(arg1); else printf("Uso: cd <nome_dir>\n");
        } else if (strcmp(cmd, "rmdir") == 0) {
            char *arg1 = strtok(NULL, " \t");
            if (arg1) cmd_rmdir(arg1); else printf("Uso: rmdir <nome_dir>\n");
        } else if (strcmp(cmd, "rm") == 0) {
            char *arg1 = strtok(NULL, " \t");
            if (arg1) cmd_rm(arg1); else printf("Uso: rm <nome_arq>\n");
        } else if (strcmp(cmd, "stat") == 0) {
            char *arg1 = strtok(NULL, " \t");
            if (arg1) cmd_stat(arg1); else printf("Uso: stat <nome_item>\n");
        } else if (strcmp(cmd, "cat") == 0) {
            char *arg1 = strtok(NULL, " \t");
            if (arg1) cmd_cat(arg1); else printf("Uso: cat <nome_arq>\n");
        } else if (strcmp(cmd, "import") == 0) {
            char *arg1 = strtok(NULL, " \t");
            char *arg2 = strtok(NULL, " \t");
            if (arg1 && arg2) cmd_import(arg1, arg2); else printf("Uso: import <caminho_real> <nome_dest>\n");
        } else if (strcmp(cmd, "rename") == 0) {
            char *arg1 = strtok(NULL, " \t");
            char *arg2 = strtok(NULL, " \t");
            if (arg1 && arg2) cmd_rename(arg1, arg2); else printf("Uso: rename <antigo> <novo>\n");
        } else if (strcmp(cmd, "mv") == 0) {
            char *arg1 = strtok(NULL, " \t");
            char *arg2 = strtok(NULL, " \t");
            if (arg1 && arg2) cmd_mv(arg1, arg2); else printf("Uso: mv <origem> <dir_destino>\n");
        } else if (strcmp(cmd, "echo") == 0) {
            char *text_start = strtok(NULL, "\""); // Pega o texto entre aspas
            if (text_start) {
                char *op = strtok(NULL, " \t"); // Pega o operador > ou >>
                if (op) {
                    char *filename = strtok(NULL, " \t"); // Pega o nome do arquivo
                    if (filename) {
                        // Verifica se o operador é válido
                        if (strcmp(op, ">") == 0 || strcmp(op, ">>") == 0) {
                            cmd_echo(text_start, op, filename);
                        } else {
                            printf("Operador de redirecionamento inválido: %s\n", op);
                        }
                    } else {
                        printf("Uso: echo \"texto\" >/>> <arquivo>\n");
                    }
                } else {
                    // Se não houver operador, apenas imprime na tela
                    printf("%s\n", text_start);
                }
            } else {
                printf("Uso: echo \"texto\" >/>> <arquivo>\n");
            }
        } else if (strcmp(cmd, "set") == 0) {
            char* arg1 = strtok(NULL, " \t");
            char* arg2 = strtok(NULL, " \t");
            if (arg1 && arg2) {
                cmd_set(arg1, arg2);
            } else {
                printf("Uso: set <parametro> <valor>\n");
            }
        } else {
            printf("Comando desconhecido: %s\n", cmd);
        }
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso:\n");
        fprintf(stderr, "  %s create <tamanho_disco_kb> <tamanho_bloco_kb>\n", argv[0]);
        fprintf(stderr, "  %s run\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "create") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Uso: %s create <tamanho_disco_kb> <tamanho_bloco_kb>\n", argv[0]);
            return 1;
        }
        uint32_t total_size = atoi(argv[2]);
        uint32_t block_size = atoi(argv[3]);
        if (fs_format(DISK_PATH, total_size, block_size) != 0) {
            fprintf(stderr, "ERRO FATAL: Falha ao formatar o disco.\n");
            return 1;
        }

    } else if (strcmp(argv[1], "run") == 0) {
        if (fs_mount(DISK_PATH) != 0) {
            fprintf(stderr, "ERRO FATAL: Falha ao montar o disco. O arquivo '%s' existe e foi formatado corretamente com o comando 'create'?\n", DISK_PATH);
            return 1;
        }
        printf("Disco '%s' montado com sucesso. Bem-vindo!\n", DISK_PATH);
        shell();
        fs_unmount();
        printf("Disco desmontado. Encerrando.\n");

    } else {
        fprintf(stderr, "Comando desconhecido: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
