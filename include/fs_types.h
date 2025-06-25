// include/fs_types.h
#ifndef FS_TYPES_H
#define FS_TYPES_H

#include <time.h>
#include <stdint.h> // Essencial para tipos de tamanho fixo como uint32_t

#define MAGIC_NUMBER 0xDA7AF17E // "DATA FILE" em Leetspeak, para identificar nosso FS
#define MAX_FILENAME_LEN 60
#define INODE_DIRECT_BLOCKS 12 // 12 ponteiros diretos para blocos de dados

// Cores (opcional, mas mantido)
#define COLOR_RESET   "\033[0m"
// ... (outras cores podem ser mantidas)

// Superbloco: O primeiro bloco do disco, com informações gerais.
typedef struct {
    uint32_t magic_number;        // Para identificar nosso sistema de arquivos
    uint32_t total_blocks;        // Número total de blocos no disco
    uint32_t total_inodes;        // Número total de i-nodes
    uint32_t block_size;          // Tamanho de cada bloco em bytes

    uint32_t inode_bitmap_start;  // Bloco onde começa o bitmap de i-nodes
    uint32_t block_bitmap_start;  // Bloco onde começa o bitmap de blocos
    uint32_t inode_table_start;   // Bloco onde começa a tabela de i-nodes
    uint32_t data_blocks_start;   // Bloco onde começam os blocos de dados
} Superblock;

// Tipo do I-node: Arquivo ou Diretório
typedef enum {
    TYPE_FILE,
    TYPE_DIR
} InodeType;

// I-node: Estrutura que representa um arquivo ou diretório no disco.
// Ela não contém ponteiros de memória!
typedef struct {
    InodeType type;               // Tipo: arquivo ou diretório
    uint32_t size;                // Tamanho do arquivo em bytes
    uint32_t link_count;          // Quantidade de links para este i-node
    time_t created;
    time_t modified;
    time_t accessed;
    uint32_t direct_blocks[INODE_DIRECT_BLOCKS]; // Ponteiros diretos para blocos de dados
    // Ponteiros indiretos podem ser adicionados aqui futuramente
} Inode;

// Representa uma única entrada dentro de um diretório.
typedef struct {
    char name[MAX_FILENAME_LEN]; // Nome do arquivo/subdiretório
    uint32_t inode_num;          // Número do i-node correspondente
} DirectoryEntry;

#endif // FS_TYPES_H