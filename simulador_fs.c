#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_NOME 64
#define MAX_CONTEUDO 1024
#define MAX_ARQUIVOS 128

// Cores ANSI para terminal
#define COLOR_RESET   "\033[0m"
#define COLOR_CMD     "\033[1;36m"
#define COLOR_SUCCESS "\033[1;32m"
#define COLOR_ERROR   "\033[1;31m"
#define COLOR_INFO    "\033[1;33m"

typedef enum { ARQUIVO, DIRETORIO } Tipo;

typedef struct Node {
    char nome[MAX_NOME];
    Tipo tipo;
    struct Node *pai;
    struct Node *filhos[MAX_ARQUIVOS];
    int qtd_filhos;
    char conteudo[MAX_CONTEUDO];
    time_t criado, modificado, acessado;
} Node;

Node *raiz = NULL;
Node *atual = NULL;

Node *criar_node(const char *nome, Tipo tipo, Node *pai) {
    Node *n = malloc(sizeof(Node));
    strncpy(n->nome, nome, MAX_NOME);
    n->tipo = tipo;
    n->pai = pai;
    n->qtd_filhos = 0;
    n->criado = n->modificado = n->acessado = time(NULL);
    if (tipo == ARQUIVO) n->conteudo[0] = '\0';
    return n;
}

void inicializar_fs() {
    raiz = criar_node("/", DIRETORIO, NULL);
    atual = raiz;
}

void prompt() {
    if (!isatty(fileno(stdin))) return; // Evita mostrar prompt em modo script
    Node *n = atual;
    char caminho[1024] = "";
    while (n != NULL) {
        char temp[1024];
        snprintf(temp, sizeof(temp), "%s%s", n == raiz ? "" : "/", n->nome);
        memmove(caminho + strlen(temp), caminho, strlen(caminho) + 1);
        memcpy(caminho, temp, strlen(temp));
        n = n->pai;
    }
    printf("fs:%s$ ", caminho);
}

Node *buscar(Node *dir, const char *nome) {
    for (int i = 0; i < dir->qtd_filhos; i++) {
        if (strcmp(dir->filhos[i]->nome, nome) == 0) return dir->filhos[i];
    }
    return NULL;
}

void cmd_mkdir(const char *nome) {
    if (buscar(atual, nome)) {
        printf(COLOR_ERROR "Erro: Diretório '%s' já existe.\n" COLOR_RESET, nome);
        return;
    }
    if (atual->qtd_filhos >= MAX_ARQUIVOS) {
        printf(COLOR_ERROR "Erro: Limite de arquivos atingido.\n" COLOR_RESET);
        return;
    }
    Node *n = criar_node(nome, DIRETORIO, atual);
    atual->filhos[atual->qtd_filhos++] = n;
    printf(COLOR_SUCCESS "Diretório '%s' criado com sucesso.\n" COLOR_RESET, nome);
}

void cmd_ls() {
    printf(COLOR_INFO "Conteúdo de %s:\n" COLOR_RESET, atual->nome);
    for (int i = 0; i < atual->qtd_filhos; i++) {
        printf("<%s>\t%s\n", atual->filhos[i]->tipo == DIRETORIO ? "DIR" : "FILE", atual->filhos[i]->nome);
    }
}

void cmd_cd(const char *nome) {
    if (strcmp(nome, "..") == 0) {
        if (atual->pai) {
            atual = atual->pai;
            printf(COLOR_SUCCESS "Voltou para o diretório pai.\n" COLOR_RESET);
        } else {
            printf(COLOR_ERROR "Erro: Já está no diretório raiz.\n" COLOR_RESET);
        }
        return;
    }
    Node *n = buscar(atual, nome);
    if (n && n->tipo == DIRETORIO) {
        atual = n;
        printf(COLOR_SUCCESS "Diretório alterado para '%s'.\n" COLOR_RESET, nome);
    } else {
        printf(COLOR_ERROR "Erro: Diretório não encontrado.\n" COLOR_RESET);
    }
}

void cmd_import(const char *caminho, const char *nome_dest) {
    FILE *fp = fopen(caminho, "r");
    if (!fp) {
        printf(COLOR_ERROR "Erro ao abrir arquivo real: '%s'.\n" COLOR_RESET, caminho);
        return;
    }
    Node *n = criar_node(nome_dest, ARQUIVO, atual);
    fread(n->conteudo, 1, MAX_CONTEUDO - 1, fp);
    fclose(fp);
    atual->filhos[atual->qtd_filhos++] = n;
    printf(COLOR_SUCCESS "Arquivo '%s' importado (%lu bytes).\n" COLOR_RESET, nome_dest, strlen(n->conteudo));
}

void cmd_cat(const char *nome) {
    Node *n = buscar(atual, nome);
    if (!n || n->tipo != ARQUIVO) {
        printf(COLOR_ERROR "Erro: Arquivo não encontrado.\n" COLOR_RESET);
        return;
    }
    n->acessado = time(NULL);
    printf("%s\n", n->conteudo);
}

void cmd_rename(const char *orig, const char *novo) {
    Node *n = buscar(atual, orig);
    if (!n) {
        printf(COLOR_ERROR "Erro: Item não encontrado.\n" COLOR_RESET);
        return;
    }
    strncpy(n->nome, novo, MAX_NOME);
    n->modificado = time(NULL);
    printf(COLOR_SUCCESS "Renomeado com sucesso: '%s' -> '%s'.\n" COLOR_RESET, orig, novo);
}

void cmd_mv(const char *nome, const char *dest) {
    Node *n = buscar(atual, nome);
    Node *d = buscar(atual, dest);
    if (!n || !d || d->tipo != DIRETORIO) {
        printf(COLOR_ERROR "Erro ao mover: verifique os nomes.\n" COLOR_RESET);
        return;
    }
    for (int i = 0; i < atual->qtd_filhos; i++) {
        if (atual->filhos[i] == n) {
            for (int j = i; j < atual->qtd_filhos - 1; j++)
                atual->filhos[j] = atual->filhos[j + 1];
            atual->qtd_filhos--;
            break;
        }
    }
    n->pai = d;
    d->filhos[d->qtd_filhos++] = n;
    printf(COLOR_SUCCESS "'%s' movido para '%s'.\n" COLOR_RESET, nome, dest);
}

void cmd_rm(const char *nome) {
    for (int i = 0; i < atual->qtd_filhos; i++) {
        if (strcmp(atual->filhos[i]->nome, nome) == 0 && atual->filhos[i]->tipo == ARQUIVO) {
            free(atual->filhos[i]);
            for (int j = i; j < atual->qtd_filhos - 1; j++)
                atual->filhos[j] = atual->filhos[j + 1];
            atual->qtd_filhos--;
            printf(COLOR_SUCCESS "Arquivo '%s' removido.\n" COLOR_RESET, nome);
            return;
        }
    }
    printf(COLOR_ERROR "Erro: Arquivo não encontrado.\n" COLOR_RESET);
}

void cmd_rmdir(const char *nome) {
    for (int i = 0; i < atual->qtd_filhos; i++) {
        if (strcmp(atual->filhos[i]->nome, nome) == 0 && atual->filhos[i]->tipo == DIRETORIO) {
            free(atual->filhos[i]);
            for (int j = i; j < atual->qtd_filhos - 1; j++)
                atual->filhos[j] = atual->filhos[j + 1];
            atual->qtd_filhos--;
            printf(COLOR_SUCCESS "Diretório '%s' removido.\n" COLOR_RESET, nome);
            return;
        }
    }
    printf(COLOR_ERROR "Erro: Diretório não encontrado.\n" COLOR_RESET);
}

void shell() {
    char linha[256], cmd[32], arg1[128], arg2[128];
    int interativo = isatty(fileno(stdin));

    while (prompt(), fgets(linha, sizeof(linha), stdin)) {
        if (!interativo) {
            printf(COLOR_CMD ">> %s" COLOR_RESET, linha);
        }
        arg1[0] = arg2[0] = '\0';
        sscanf(linha, "%s %s %s", cmd, arg1, arg2);

        if (strcmp(cmd, "mkdir") == 0) cmd_mkdir(arg1);
        else if (strcmp(cmd, "ls") == 0) cmd_ls();
        else if (strcmp(cmd, "cd") == 0) cmd_cd(arg1);
        else if (strcmp(cmd, "import") == 0) cmd_import(arg1, arg2);
        else if (strcmp(cmd, "cat") == 0) cmd_cat(arg1);
        else if (strcmp(cmd, "rename") == 0) cmd_rename(arg1, arg2);
        else if (strcmp(cmd, "mv") == 0) cmd_mv(arg1, arg2);
        else if (strcmp(cmd, "rm") == 0) cmd_rm(arg1);
        else if (strcmp(cmd, "rmdir") == 0) cmd_rmdir(arg1);
        else if (strlen(cmd) > 0)
            printf(COLOR_ERROR "Comando desconhecido: %s\n" COLOR_RESET, cmd);
        if (!interativo) printf("\n");
    }
    printf(COLOR_INFO "Encerrando shell (EOF).\n" COLOR_RESET);
}

int main() {
    inicializar_fs();
    shell();
    return 0;
}
