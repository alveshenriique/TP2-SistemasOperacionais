#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs_core.h"

// Globais que representam o estado do FS.
// 'static' as torna visíveis apenas dentro deste arquivo.
static Node *raiz = NULL;
static Node *atual = NULL;

void fs_init() {
    raiz = fs_criar_node("/", DIRETORIO, NULL);
    atual = raiz;
}

Node* fs_get_raiz() { 
    return raiz; 
}

Node* fs_get_atual() { 
    return atual; 
}

void fs_set_atual(Node* novo_atual) { 
    atual = novo_atual; 
}

Node* fs_criar_node(const char *nome, Tipo tipo, Node *pai) {
    Node *n = (Node*) malloc(sizeof(Node));
    if (!n) {
        perror("Erro de alocação de memória para novo nó");
        exit(EXIT_FAILURE);
    }
    strncpy(n->nome, nome, MAX_NOME - 1);
    n->nome[MAX_NOME - 1] = '\0';
    n->tipo = tipo;
    n->pai = pai;
    n->qtd_filhos = 0;
    n->criado = n->modificado = n->acessado = time(NULL);
    if (tipo == ARQUIVO) {
        n->conteudo[0] = '\0';
    }
    return n;
}

Node* fs_buscar_no_dir(Node *dir, const char *nome) {
    if (!dir) return NULL;
    for (int i = 0; i < dir->qtd_filhos; i++) {
        if (strcmp(dir->filhos[i]->nome, nome) == 0) {
            return dir->filhos[i];
        }
    }
    return NULL;
}

void fs_adicionar_filho(Node* pai, Node* filho) {
    if (pai && pai->tipo == DIRETORIO && pai->qtd_filhos < MAX_ARQUIVOS) {
        pai->filhos[pai->qtd_filhos++] = filho;
    }
}

int fs_remover_filho(Node* pai, const char* nome_filho) {
    if (!pai || pai->tipo != DIRETORIO) return -1;

    int indice = -1;
    for (int i = 0; i < pai->qtd_filhos; i++) {
        if (strcmp(pai->filhos[i]->nome, nome_filho) == 0) {
            indice = i;
            break;
        }
    }

    if (indice != -1) {
        free(pai->filhos[indice]); // Libera a memória do nó
        // Move os elementos para preencher o espaço
        for (int j = indice; j < pai->qtd_filhos - 1; j++) {
            pai->filhos[j] = pai->filhos[j + 1];
        }
        pai->qtd_filhos--;
        return 0; // Sucesso
    }
    return -1; // Não encontrado
}