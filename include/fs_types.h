#ifndef FS_TYPES_H
#define FS_TYPES_H

#include <time.h>

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

#endif // FS_TYPES_H