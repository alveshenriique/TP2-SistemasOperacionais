#ifndef FS_CORE_H
#define FS_CORE_H

#include "fs_types.h"

// Funções de inicialização e estado
void fs_init();
Node* fs_get_raiz();
Node* fs_get_atual();
void fs_set_atual(Node* novo_atual);

// Funções de manipulação de nós
Node* fs_criar_node(const char* nome, Tipo tipo, Node* pai);
Node* fs_buscar_no_dir(Node* dir, const char* nome);
void fs_adicionar_filho(Node* pai, Node* filho);
int fs_remover_filho(Node* pai, const char* nome_filho);

#endif // FS_CORE_H