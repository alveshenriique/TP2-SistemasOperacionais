#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "fs_core.h"
#include "commands.h"
#include "fs_types.h"

void prompt() {
    if (!isatty(fileno(stdin))) return;

    char caminho[1024];
    Node* n = fs_get_atual();

    if (n == fs_get_raiz()) {
        strcpy(caminho, "/");
    } else {
        // Array para armazenar os nomes das partes do caminho, em ordem inversa
        const char* parts[128]; // Limite de 128 níveis de profundidade
        int depth = 0;

        // Sobe na árvore a partir do nó atual, guardando os nomes
        while (n != fs_get_raiz() && depth < 128) {
            parts[depth++] = n->nome;
            n = n->pai;
        }

        // Constrói a string do caminho na ordem correta
        caminho[0] = '\0';
        size_t current_len = 0;
        for (int i = depth - 1; i >= 0; i--) {
            // Adiciona a barra '/'
            strncat(caminho, "/", sizeof(caminho) - current_len - 1);
            current_len++;

            // Adiciona o nome do diretório
            strncat(caminho, parts[i], sizeof(caminho) - current_len - 1);
            current_len = strlen(caminho);
        }
    }
    
    printf("fs:%s$ ", caminho);
}

void shell() {
    char linha[256];
    char cmd[32], arg1[128], arg2[128];
    int interativo = isatty(fileno(stdin));

    while (1) {
        if (interativo) {
            prompt();
        }
        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            // EOF (Ctrl+D)
            break; 
        }

        if (!interativo) {
            printf(COLOR_CMD ">> %s" COLOR_RESET, linha);
        }

        int args_lidos = sscanf(linha, "%s %s %s", cmd, arg1, arg2);
        
        if (args_lidos <= 0) continue; // Linha em branco

        if (strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "mkdir") == 0 && args_lidos > 1) cmd_mkdir(arg1);
        else if (strcmp(cmd, "ls") == 0) cmd_ls();
        else if (strcmp(cmd, "cd") == 0 && args_lidos > 1) cmd_cd(arg1);
        else if (strcmp(cmd, "import") == 0 && args_lidos > 2) cmd_import(arg1, arg2);
        else if (strcmp(cmd, "cat") == 0 && args_lidos > 1) cmd_cat(arg1);
        else if (strcmp(cmd, "rename") == 0 && args_lidos > 2) cmd_rename(arg1, arg2);
        else if (strcmp(cmd, "mv") == 0 && args_lidos > 2) cmd_mv(arg1, arg2);
        else if (strcmp(cmd, "rm") == 0 && args_lidos > 1) cmd_rm(arg1);
        else if (strcmp(cmd, "rmdir") == 0 && args_lidos > 1) cmd_rmdir(arg1);
        else {
            printf(COLOR_ERROR "Comando desconhecido ou argumentos insuficientes: %s\n" COLOR_RESET, cmd);
        }

        if (!interativo && strlen(cmd) > 0) printf("\n");
    }
    printf(COLOR_INFO "\nEncerrando shell.\n" COLOR_RESET);
}

int main() {
    fs_init();
    shell();
    return 0;
}