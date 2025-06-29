// src/commands.c

#include <stdio.h>
#include "commands.h"
#include "fs_types.h" 
#include "fs_core.h"
#include <string.h>
#include <stdlib.h>

/*
 * =================================================================
 * NOTA IMPORTANTE:
 *
 * Todas as funções de comando abaixo foram intencionalmente
 * esvaziadas. A lógica antiga baseada em memória (struct Node)
 * não é mais compatível com o novo sistema baseado em disco
 * (struct Inode).
 *
 * Nos próximos passos, recriaremos a lógica de cada comando,
 * um a um, para que operem no disco. Por enquanto, elas
 * apenas informam que não estão implementadas.
 * =================================================================
 */

 void cmd_mkdir(const char* nome_dir) {
    if (fs_create_directory(nome_dir) == 0) {
        printf("Diretório '%s' criado com sucesso.\n", nome_dir);
    } else {
        // A função fs_create_directory já imprime uma mensagem de erro detalhada
    }
}

void cmd_ls() {
    FileList files = fs_list_directory();
    printf("Tipo\t\tNome\n");
    printf("----\t\t----\n");
    for (size_t i = 0; i < files.count; i++) {
        printf("<%s>\t\t%s\n", files.entries[i].type == TYPE_DIR ? "DIR" : "FILE", files.entries[i].name);
    }
    free(files.entries);
}

void cmd_cd(const char* nome_dir) {
    if (fs_change_directory(nome_dir) != 0) {
        // A função do core já imprime o erro detalhado
    }
}

void cmd_import(const char* caminho_real, const char* nome_dest) {
    if (fs_import_file(caminho_real, nome_dest) == 0) {
        printf("Arquivo '%s' importado com sucesso para '%s'.\n", caminho_real, nome_dest);
    }
    // A função do core já imprime a mensagem de erro específica
}

void cmd_cat(const char* nome_arq) {
    char* file_content = fs_read_file(nome_arq);
    if (file_content) {
        printf("%s\n",file_content);
        free(file_content); // Libera a memória alocada pela função fs_read_file
    } else {
        // A função do core já imprime a mensagem de erro específica
    }
}

void cmd_rename(const char* nome_antigo, const char* nome_novo) {
    if (fs_rename(nome_antigo, nome_novo) == 0) {
        printf("Item '%s' renomeado para '%s' com sucesso.\n", nome_antigo, nome_novo);
    }
    // A função do core já imprime a mensagem de erro específica
}

void cmd_mv(const char* nome_origem, const char* nome_destino) {
    if (fs_move_item(nome_origem, nome_destino) == 0) {
        printf("'%s' movido para '%s' com sucesso.\n", nome_origem, nome_destino);
    }
    // A função do core já imprime a mensagem de erro específica
}

void cmd_rm(const char* nome_arq) {
    if (fs_remove_file(nome_arq) == 0) {
        printf("Arquivo '%s' removido com sucesso.\n", nome_arq);
    }
    // A função do core já imprime a mensagem de erro específica
}

void cmd_rmdir(const char* nome_dir) {
    if (fs_remove_directory(nome_dir) == 0) {
        printf("Diretório '%s' removido com sucesso.\n", nome_dir);
    }
    // A função do core já imprime a mensagem de erro específica
}

void cmd_stat(const char* name) {
    Inode inode = fs_stat_item(name);

    if (inode.link_count != 0) {  // Supondo que inode válido sempre tem link_count > 0
        printf("Estatísticas para: '%s'\n", name);
        printf("----------------------------------\n");
        printf("  Tipo..........: %s\n", (inode.type == TYPE_DIR) ? "Diretório" : "Arquivo");
        printf("  Links.........: %u\n", inode.link_count);
        printf("  Tamanho.......: %u bytes\n", inode.size);
        char time_buffer[26];
        ctime_r(&inode.created, time_buffer);
        time_buffer[strlen(time_buffer) - 1] = '\0';
        printf("  Criado em.....: %s\n", time_buffer);
        ctime_r(&inode.accessed, time_buffer);
        time_buffer[strlen(time_buffer) - 1] = '\0';
        printf("  Acessado em...: %s\n", time_buffer);
        ctime_r(&inode.modified, time_buffer);
        time_buffer[strlen(time_buffer) - 1] = '\0';
        printf("  Modificado em.: %s\n", time_buffer);
        printf("  Blocos de Dados: [ ");
        for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
            if (inode.direct_blocks[i] != 0) {
                printf("%u ", inode.direct_blocks[i]);
            }
        }
        printf("]\n");
    }
}

void cmd_df() {
    DiskUsageInfo disk_data = fs_disk_free();
    printf("Visão Geral do Sistema de Arquivos\n");
    printf("----------------------------------------------------------\n");
    printf("Recurso      |         Total |          Usado |          Livre\n");
    printf("----------------------------------------------------------\n");
    printf("I-nodes      | %13u | %12u | %12u\n", disk_data.total_inodes, disk_data.used_inodes, disk_data.free_inodes);
    printf("Blocos       | %13u | %12u | %12u\n", disk_data.total_blocks, disk_data.used_blocks, disk_data.free_blocks);
    printf("Espaço (KB)  | %13u | %12u | %12u\n", disk_data.total_kb, disk_data.used_kb, disk_data.free_kb);
    printf("----------------------------------------------------------\n");
}

void cmd_echo(const char* text, const char* op, const char* filename) {
    if (fs_write_file(filename, text, op) != 0) {
        // Erro já foi impresso pelo core
    } else {
        printf("Texto escrito em '%s' com sucesso.\n", filename);
    }
}

void cmd_set(const char* param, const char* value) {
    if (strcmp(param, "verbose") == 0) {
        if (strcmp(value, "on") == 0) {
            fs_set_verbose(1);
        } else if (strcmp(value, "off") == 0) {
            fs_set_verbose(0);
        } else {
            printf("Uso: set verbose <on|off>\n");
        }
    } else {
        printf("Parâmetro desconhecido: %s\n", param);
    }
}