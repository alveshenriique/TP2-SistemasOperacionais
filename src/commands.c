#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "fs_core.h"
#include "fs_types.h"

void cmd_mkdir(const char *nome) {
    Node* atual = fs_get_atual();
    if (fs_buscar_no_dir(atual, nome)) {
        printf(COLOR_ERROR "Erro: Item '%s' já existe.\n" COLOR_RESET, nome);
        return;
    }
    if (atual->qtd_filhos >= MAX_ARQUIVOS) {
        printf(COLOR_ERROR "Erro: Limite de arquivos/diretórios atingido neste diretório.\n" COLOR_RESET);
        return;
    }
    Node *n = fs_criar_node(nome, DIRETORIO, atual);
    fs_adicionar_filho(atual, n);
    printf(COLOR_SUCCESS "Diretório '%s' criado com sucesso.\n" COLOR_RESET, nome);
}

void cmd_ls() {
    Node* atual = fs_get_atual();
    printf(COLOR_INFO "Conteúdo de '%s':\n" COLOR_RESET, atual->nome);
    for (int i = 0; i < atual->qtd_filhos; i++) {
        printf("<%s>\t%s\n", atual->filhos[i]->tipo == DIRETORIO ? "DIR " : "FILE", atual->filhos[i]->nome);
    }
}

void cmd_cd(const char *nome) {
    if (strcmp(nome, "..") == 0) {
        Node* pai = fs_get_atual()->pai;
        if (pai) {
            fs_set_atual(pai);
        } else {
            printf(COLOR_ERROR "Erro: Já está no diretório raiz.\n" COLOR_RESET);
        }
        return;
    }
    Node* dest = fs_buscar_no_dir(fs_get_atual(), nome);
    if (dest && dest->tipo == DIRETORIO) {
        fs_set_atual(dest);
    } else {
        printf(COLOR_ERROR "Erro: Diretório '%s' não encontrado.\n" COLOR_RESET, nome);
    }
}

void cmd_import(const char *caminho_real, const char *nome_dest) {
    Node* atual = fs_get_atual();
    if (fs_buscar_no_dir(atual, nome_dest)) {
        printf(COLOR_ERROR "Erro: Item '%s' já existe.\n" COLOR_RESET, nome_dest);
        return;
    }
    FILE *fp = fopen(caminho_real, "r");
    if (!fp) {
        printf(COLOR_ERROR "Erro ao abrir arquivo real: '%s'.\n" COLOR_RESET, caminho_real);
        return;
    }
    Node *n = fs_criar_node(nome_dest, ARQUIVO, atual);
    size_t bytes_lidos = fread(n->conteudo, 1, MAX_CONTEUDO - 1, fp);
    n->conteudo[bytes_lidos] = '\0';
    fclose(fp);
    fs_adicionar_filho(atual, n);
    printf(COLOR_SUCCESS "Arquivo '%s' importado (%zu bytes).\n" COLOR_RESET, nome_dest, strlen(n->conteudo));
}

void cmd_cat(const char *nome) {
    Node *n = fs_buscar_no_dir(fs_get_atual(), nome);
    if (!n || n->tipo != ARQUIVO) {
        printf(COLOR_ERROR "Erro: Arquivo '%s' não encontrado.\n" COLOR_RESET, nome);
        return;
    }
    n->acessado = time(NULL);
    printf("%s\n", n->conteudo);
}

void cmd_rename(const char *orig, const char *novo) {
    Node *n = fs_buscar_no_dir(fs_get_atual(), orig);
    if (!n) {
        printf(COLOR_ERROR "Erro: Item '%s' não encontrado.\n" COLOR_RESET, orig);
        return;
    }
    if (fs_buscar_no_dir(fs_get_atual(), novo)) {
        printf(COLOR_ERROR "Erro: Já existe um item com o nome '%s'.\n" COLOR_RESET, novo);
        return;
    }
    strncpy(n->nome, novo, MAX_NOME - 1);
    n->nome[MAX_NOME - 1] = '\0';
    n->modificado = time(NULL);
    printf(COLOR_SUCCESS "Item '%s' renomeado para '%s'.\n" COLOR_RESET, orig, novo);
}

void cmd_mv(const char *nome, const char *dest_dir_nome) {
    Node* atual = fs_get_atual();
    Node* item_mover = fs_buscar_no_dir(atual, nome);
    if (!item_mover) {
        printf(COLOR_ERROR "Erro: Item a ser movido ('%s') não encontrado.\n" COLOR_RESET, nome);
        return;
    }
    Node* dest_dir = fs_buscar_no_dir(atual, dest_dir_nome);
    if (!dest_dir || dest_dir->tipo != DIRETORIO) {
        printf(COLOR_ERROR "Erro: Diretório de destino ('%s') não encontrado.\n" COLOR_RESET, dest_dir_nome);
        return;
    }
    if (fs_remover_filho(atual, nome) == 0) {
        fs_adicionar_filho(dest_dir, item_mover);
        item_mover->pai = dest_dir;
        printf(COLOR_SUCCESS "'%s' movido para '%s'.\n" COLOR_RESET, nome, dest_dir_nome);
    }
}

void cmd_rm(const char *nome) {
    Node* item = fs_buscar_no_dir(fs_get_atual(), nome);
    if (!item || item->tipo != ARQUIVO) {
        printf(COLOR_ERROR "Erro: Arquivo '%s' não encontrado.\n" COLOR_RESET, nome);
        return;
    }
    if (fs_remover_filho(fs_get_atual(), nome) == 0) {
        printf(COLOR_SUCCESS "Arquivo '%s' removido.\n" COLOR_RESET, nome);
    }
}

void cmd_rmdir(const char *nome) {
    Node* item = fs_buscar_no_dir(fs_get_atual(), nome);
    if (!item || item->tipo != DIRETORIO) {
        printf(COLOR_ERROR "Erro: Diretório '%s' não encontrado.\n" COLOR_RESET, nome);
        return;
    }
    if (item->qtd_filhos > 0) {
        printf(COLOR_ERROR "Erro: O diretório '%s' não está vazio.\n" COLOR_RESET, nome);
        return;
    }
    if (fs_remover_filho(fs_get_atual(), nome) == 0) {
        printf(COLOR_SUCCESS "Diretório '%s' removido.\n" COLOR_RESET, nome);
    }
}