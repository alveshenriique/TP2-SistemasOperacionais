#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs_core.h"
#include "fs_types.h"
#include <time.h>
#include <stdarg.h>

// --- Variáveis Globais ---
static FILE* disk_file = NULL;
static Superblock sb;
uint32_t current_inode_num = 0;
static int verbose_mode = 0;

// --- Funções Auxiliares de Impressão ---
static void verbose_printf(const char* format, ...) {
    if (verbose_mode) {
        printf("\033[0;34m[verbose]\033[0m ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

// --- Funções Auxiliares de Bloco ---
static int block_write(uint32_t block_num, const void* data) {
    verbose_printf("Escrevendo no disco: Bloco %u\n", block_num);
    if (!disk_file) return -1;
    if (fseek(disk_file, block_num * sb.block_size, SEEK_SET) != 0) return -1;
    if (fwrite(data, sb.block_size, 1, disk_file) != 1) return -1;
    return 0;
}

static int block_read(uint32_t block_num, void* data) {
    verbose_printf("Lendo do disco: Bloco %u\n", block_num);
    if (!disk_file) return -1;
    if (fseek(disk_file, block_num * sb.block_size, SEEK_SET) != 0) return -1;
    if (fread(data, sb.block_size, 1, disk_file) != 1) return -1;
    return 0;
}

// --- Funções Auxiliares de I-node e Bitmap ---
static int inode_write(uint32_t inode_num, const Inode* inode_data) {
    uint32_t inodes_per_block = sb.block_size / sizeof(Inode);
    uint32_t block_num = sb.inode_table_start + (inode_num / inodes_per_block);
    uint32_t offset_in_block = (inode_num % inodes_per_block) * sizeof(Inode);
    verbose_printf("Escrevendo i-node %u (Bloco: %u, Offset: %u)\n", inode_num, block_num, offset_in_block);
    char block_buffer[sb.block_size];
    if (block_read(block_num, block_buffer) != 0) return -1;
    memcpy(block_buffer + offset_in_block, inode_data, sizeof(Inode));
    return block_write(block_num, block_buffer);
}

static int inode_read(uint32_t inode_num, Inode* inode_data) {
    uint32_t inodes_per_block = sb.block_size / sizeof(Inode);
    uint32_t block_num = sb.inode_table_start + (inode_num / inodes_per_block);
    uint32_t offset_in_block = (inode_num % inodes_per_block) * sizeof(Inode);
    verbose_printf("Lendo i-node %u (Bloco: %u, Offset: %u)\n", inode_num, block_num, offset_in_block);
    char block_buffer[sb.block_size];
    if (block_read(block_num, block_buffer) != 0) return -1;
    memcpy(inode_data, block_buffer + offset_in_block, sizeof(Inode));
    return 0;
}

static int find_free_bit_from(uint32_t bitmap_start_block, uint32_t total_bits, uint32_t start_bit) {
    if (start_bit >= total_bits) return -1;
    char block_buffer[sb.block_size];
    uint32_t start_block_idx = start_bit / (sb.block_size * 8);
    uint32_t blocks_in_bitmap = (total_bits + 8 * sb.block_size - 1) / (8 * sb.block_size);
    for (uint32_t i = start_block_idx; i < blocks_in_bitmap; ++i) {
        if (block_read(bitmap_start_block + i, block_buffer) != 0) return -1;
        uint32_t start_byte = (i == start_block_idx) ? (start_bit % (sb.block_size * 8)) / 8 : 0;
        for (uint32_t byte = start_byte; byte < sb.block_size; ++byte) {
            if ((unsigned char)block_buffer[byte] != 0xFF) {
                uint32_t start_bit_in_byte = (i == start_block_idx && byte == start_byte) ? (start_bit % 8) : 0;
                for (int bit = start_bit_in_byte; bit < 8; ++bit) {
                    if (!((block_buffer[byte] >> bit) & 1)) {
                        uint32_t bit_num = i * sb.block_size * 8 + byte * 8 + bit;
                        if (bit_num < total_bits) return bit_num;
                    }
                }
            }
        }
    }
    return -1;
}

static uint32_t count_set_bits(uint32_t bitmap_start_block, uint32_t total_bits) {
    verbose_printf("Contando bits usados no bitmap (início: bloco %u, total: %u bits)\n", bitmap_start_block, total_bits);
    uint32_t count = 0;
    char block_buffer[sb.block_size];
    uint32_t blocks_in_bitmap = (total_bits + 8 * sb.block_size - 1) / (8 * sb.block_size);
    for (uint32_t i = 0; i < blocks_in_bitmap; ++i) {
        if (block_read(bitmap_start_block + i, block_buffer) != 0) return -1;
        for (uint32_t byte = 0; byte < sb.block_size; ++byte) {
            for (int bit = 0; bit < 8; ++bit) {
                if ((block_buffer[byte] >> bit) & 1) {
                    if ((i * sb.block_size * 8 + byte * 8 + bit) < total_bits) count++;
                }
            }
        }
    }
    verbose_printf("Total de bits usados: %u\n", count);
    return count;
}

static int set_bit(uint32_t bitmap_start_block, uint32_t bit_num, int value) {
    uint32_t block_idx = bit_num / (sb.block_size * 8);
    uint32_t byte_in_block = (bit_num % (sb.block_size * 8)) / 8;
    uint32_t bit_in_byte = bit_num % 8;
    verbose_printf("Definindo bit #%u para %d (Bloco do bitmap: %u)\n", bit_num, value, bitmap_start_block + block_idx);
    char block_buffer[sb.block_size];
    if (block_read(bitmap_start_block + block_idx, block_buffer) != 0) return -1;
    if (value) block_buffer[byte_in_block] |= (1 << bit_in_byte);
    else block_buffer[byte_in_block] &= ~(1 << bit_in_byte);
    return block_write(bitmap_start_block + block_idx, block_buffer);
}

static int alloc_inode() {
    verbose_printf("Procurando i-node livre a partir do #1...\n");
    int inode_num = find_free_bit_from(sb.inode_bitmap_start, sb.total_inodes, 1);
    if (inode_num != -1) {
        verbose_printf("I-node livre encontrado: #%d. Marcando como usado.\n", inode_num);
        if (set_bit(sb.inode_bitmap_start, inode_num, 1) != 0) return -1;
    }
    return inode_num;
}

static int alloc_block() {
    verbose_printf("Procurando bloco de dados livre a partir do bloco #%u...\n", sb.data_blocks_start);
    int block_num = find_free_bit_from(sb.block_bitmap_start, sb.total_blocks, sb.data_blocks_start);
    if (block_num != -1) {
        verbose_printf("Bloco livre encontrado: #%d. Marcando como usado.\n", block_num);
        if (set_bit(sb.block_bitmap_start, block_num, 1) != 0) return -1;
    }
    return block_num;
}

static int find_in_directory(const Inode* dir_inode, const char* name) {
    verbose_printf("Procurando por '%s' nas entradas do i-node.\n", name);
    if (dir_inode->type != TYPE_DIR) return -1;
    char block_buffer[sb.block_size];
    for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
        uint32_t block_num = dir_inode->direct_blocks[i];
        if (block_num == 0) continue;
        if (block_read(block_num, block_buffer) != 0) return -1;
        DirectoryEntry* entry = (DirectoryEntry*) block_buffer;
        int num_entries = sb.block_size / sizeof(DirectoryEntry);
        for (int j = 0; j < num_entries; ++j) {
            if (strlen(entry[j].name) > 0 && strcmp(entry[j].name, name) == 0) {
                verbose_printf("Entrada '%s' encontrada, aponta para o i-node %u.\n", name, entry[j].inode_num);
                return entry[j].inode_num;
            }
        }
    }
    verbose_printf("Entrada '%s' não encontrada.\n", name);
    return -1;
}

static int add_entry_to_directory(Inode* dir_inode, uint32_t dir_inode_num, const char* new_name, uint32_t new_inode_num) {
    verbose_printf("Adicionando entrada '%s' (i-node %u) ao diretório (i-node %u).\n", new_name, new_inode_num, dir_inode_num);
    char block_buffer[sb.block_size];
    for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
        uint32_t block_num = dir_inode->direct_blocks[i];
        if (block_num == 0) {
            // Futuramente: alocar novo bloco se diretório estiver cheio
            continue;
        }
        if (block_read(block_num, block_buffer) != 0) return -1;
        DirectoryEntry* entry = (DirectoryEntry*) block_buffer;
        int num_entries = sb.block_size / sizeof(DirectoryEntry);
        for (int j = 0; j < num_entries; ++j) {
            if (strlen(entry[j].name) == 0) {
                verbose_printf(" -> Slot livre encontrado no bloco de dados %u, posição %d.\n", block_num, j);
                strncpy(entry[j].name, new_name, MAX_FILENAME_LEN);
                entry[j].name[MAX_FILENAME_LEN-1] = '\0';
                entry[j].inode_num = new_inode_num;
                dir_inode->size += sizeof(DirectoryEntry);
                verbose_printf(" -> Atualizando tamanho do i-node pai %u para %u bytes.\n", dir_inode_num, dir_inode->size);
                inode_write(dir_inode_num, dir_inode);
                return block_write(block_num, block_buffer);
            }
        }
    }
    fprintf(stderr, "Erro: Diretório está cheio.\n");
    return -1;
}

static int free_block(uint32_t block_num) {
    verbose_printf("Liberando bloco de dados #%u.\n", block_num);
    return set_bit(sb.block_bitmap_start, block_num, 0);
}

static int free_inode(uint32_t inode_num) {
    verbose_printf("Liberando i-node #%u.\n", inode_num);
    return set_bit(sb.inode_bitmap_start, inode_num, 0);
}

static int remove_entry_from_directory(Inode* parent_inode, uint32_t parent_inode_num, const char* name_to_remove) {
    verbose_printf("Removendo entrada '%s' do diretório (i-node %u).\n", name_to_remove, parent_inode_num);
    char block_buffer[sb.block_size];
    for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
        uint32_t block_num = parent_inode->direct_blocks[i];
        if (block_num == 0) continue;
        if (block_read(block_num, block_buffer) != 0) return -1;
        DirectoryEntry* entry = (DirectoryEntry*) block_buffer;
        int num_entries = sb.block_size / sizeof(DirectoryEntry);
        for (int j = 0; j < num_entries; ++j) {
            if (strlen(entry[j].name) > 0 && strcmp(entry[j].name, name_to_remove) == 0) {
                verbose_printf(" -> Entrada encontrada no bloco %u. Zerando entrada.\n", block_num);
                memset(&entry[j], 0, sizeof(DirectoryEntry));
                parent_inode->size -= sizeof(DirectoryEntry);
                verbose_printf(" -> Atualizando tamanho do i-node pai %u para %u bytes.\n", parent_inode_num, parent_inode->size);
                inode_write(parent_inode_num, parent_inode);
                return block_write(block_num, block_buffer);
            }
        }
    }
    return -1;
}

// --- Funções Principais ---

Inode fs_list_inode() {
    verbose_printf("Iniciando 'ls' no i-node nº %u\n", current_inode_num);
    Inode current_inode;
    if (inode_read(current_inode_num, &current_inode) != 0 || current_inode.type != TYPE_DIR) {
        Inode empty = {0};
        return empty;
    }
    return current_inode;
}

FileList fs_list_directory() {
    verbose_printf("Iniciando 'ls' no i-node nº %u\n", current_inode_num);
    Inode current_inode = fs_list_inode();
    char block_buffer[sb.block_size];
    FileList file_list = {0};
    int num_blocks = (current_inode.size + sb.block_size - 1) / sb.block_size;
    for (int i = 0; i < num_blocks; ++i) {
        uint32_t block_num = current_inode.direct_blocks[i];
        if (block_num == 0) continue;
        if (block_read(block_num, block_buffer) != 0) {
            fprintf(stderr, "Erro ao ler o bloco de dados do diretório.\n");
            free(file_list.entries);
            file_list.entries = NULL;
            file_list.count = 0;
            return file_list;
        }
        DirectoryEntry* entry = (DirectoryEntry*) block_buffer;
        int num_entries_per_block = sb.block_size / sizeof(DirectoryEntry);

        for (int j = 0; j < num_entries_per_block; ++j) {
            if (strlen(entry[j].name) > 0) {
                Inode entry_inode;
                if(inode_read(entry[j].inode_num, &entry_inode) == 0) {
                    FileEntry *tmp = realloc(file_list.entries, (file_list.count + 1) * sizeof(FileEntry));
                    if (!tmp) {
                        fprintf(stderr, "Erro de alocação de memória\n");
                        free(file_list.entries);
                        file_list.entries = NULL;
                        file_list.count = 0;
                        return file_list;
                    }
                    file_list.entries = tmp;
                    strncpy(file_list.entries[file_list.count].name, entry[j].name, MAX_FILENAME_LEN);
                    file_list.entries[file_list.count].name[MAX_FILENAME_LEN-1] = '\0';
                    file_list.entries[file_list.count].type = entry_inode.type;
                    file_list.count++;
                }
            }
        }
    }
    return file_list;
}

int fs_create_directory(const char* name) {
    verbose_printf("Iniciando 'mkdir %s'\n", name);
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) {
        fprintf(stderr, "Erro ao ler o i-node do diretório atual.\n");
        return -1;
    }
    verbose_printf("Lido i-node pai nº %u\n", current_inode_num);
    if (find_in_directory(&parent_inode, name) != -1) {
        fprintf(stderr, "Erro: Um item com o nome '%s' já existe.\n", name);
        return -1;
    }
    int new_inode_num = alloc_inode();
    if (new_inode_num == -1) {
        fprintf(stderr, "Erro: Não há i-nodes livres.\n");
        return -1;
    }
    int new_block_num = alloc_block();
    if (new_block_num == -1) {
        fprintf(stderr, "Erro: Não há blocos de dados livres.\n");
        free_inode(new_inode_num);
        return -1;
    }
    if (add_entry_to_directory(&parent_inode, current_inode_num, name, new_inode_num) != 0) {
        fprintf(stderr, "Erro ao adicionar entrada no diretório pai.\n");
        free_inode(new_inode_num);
        free_block(new_block_num);
        return -1;
    }
    verbose_printf("Inicializando i-node %d para o novo diretório.\n", new_inode_num);
    Inode new_inode;
    new_inode.type = TYPE_DIR;
    new_inode.size = sizeof(DirectoryEntry) * 2;
    new_inode.link_count = 2;
    new_inode.created = new_inode.modified = new_inode.accessed = time(NULL);
    for(int i=0; i<INODE_DIRECT_BLOCKS; ++i) new_inode.direct_blocks[i] = 0;
    new_inode.direct_blocks[0] = new_block_num;
    if (inode_write(new_inode_num, &new_inode) != 0) { return -1; }

    verbose_printf("Escrevendo '.' e '..' no bloco de dados %d.\n", new_block_num);
    DirectoryEntry dot_entry, dotdot_entry;
    strncpy(dot_entry.name, ".", MAX_FILENAME_LEN);
    dot_entry.inode_num = new_inode_num;
    strncpy(dotdot_entry.name, "..", MAX_FILENAME_LEN);
    dotdot_entry.inode_num = current_inode_num;
    char block_buffer[sb.block_size];
    memset(block_buffer, 0, sb.block_size);
    memcpy(block_buffer, &dot_entry, sizeof(DirectoryEntry));
    memcpy(block_buffer + sizeof(DirectoryEntry), &dotdot_entry, sizeof(DirectoryEntry));
    if (block_write(new_block_num, block_buffer) != 0) { return -1; }
    
    verbose_printf("Atualizando contagem de links do i-node pai %u.\n", current_inode_num);
    parent_inode.link_count++;
    inode_write(current_inode_num, &parent_inode);
    return 0;
}

int fs_change_directory(const char* name) {
    verbose_printf("Iniciando 'cd %s'.\n", name);
    Inode current_dir_inode;
    if (inode_read(current_inode_num, &current_dir_inode) != 0) return -1;
    verbose_printf("Procurando por '%s' no diretório atual (i-node %u).\n", name, current_inode_num);
    int target_inode_num = find_in_directory(&current_dir_inode, name);
    if (target_inode_num == -1) {
        fprintf(stderr, "Erro: Diretório '%s' não encontrado.\n", name);
        return -1;
    }
    Inode target_inode;
    if (inode_read(target_inode_num, &target_inode) != 0) return -1;
    if (target_inode.type != TYPE_DIR) {
        fprintf(stderr, "Erro: '%s' não é um diretório.\n", name);
        return -1;
    }
    verbose_printf("Mudando o diretório atual para o i-node %d.\n", target_inode_num);
    current_inode_num = target_inode_num;
    return 0;
}

void fs_get_current_path(char* path_buffer, size_t buffer_size) {
    if (current_inode_num == 0) {
        snprintf(path_buffer, buffer_size, "/");
        return;
    }
    char temp_path[buffer_size];
    temp_path[0] = '\0';
    uint32_t temp_inode_num = current_inode_num;
    while (temp_inode_num != 0) {
        Inode child_inode;
        if(inode_read(temp_inode_num, &child_inode) != 0) { snprintf(path_buffer, buffer_size, "/<erro>"); return; }
        int parent_inode_num = find_in_directory(&child_inode, "..");
        if (parent_inode_num == -1) { snprintf(path_buffer, buffer_size, "/<erro_pai>"); return; }
        Inode parent_inode;
        if (inode_read(parent_inode_num, &parent_inode) != 0) { snprintf(path_buffer, buffer_size, "/<erro>"); return; }
        char current_name[MAX_FILENAME_LEN] = "?";
        char block_buffer[sb.block_size];
        int found = 0;
        for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
             uint32_t block_num = parent_inode.direct_blocks[i];
             if (block_num == 0) continue;
             block_read(block_num, block_buffer);
             DirectoryEntry* entry = (DirectoryEntry*) block_buffer;
             int num_entries = sb.block_size / sizeof(DirectoryEntry);
             for (int j = 0; j < num_entries; ++j) {
                 if(entry[j].inode_num == temp_inode_num) {
                     strncpy(current_name, entry[j].name, MAX_FILENAME_LEN);
                     found = 1;
                     break;
                 }
             }
             if(found) break;
        }
        char segment[buffer_size];
        snprintf(segment, buffer_size, "/%s%s", current_name, temp_path);
        strcpy(temp_path, segment);
        temp_inode_num = parent_inode_num;
    }
    strncpy(path_buffer, temp_path, buffer_size);
}

int fs_remove_directory(const char* name) {
    verbose_printf("Iniciando 'rmdir %s'.\n", name);
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        fprintf(stderr, "Erro: Não é permitido remover '.' ou '..'.\n");
        return -1;
    }
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) return -1;
    int target_inode_num = find_in_directory(&parent_inode, name);
    if (target_inode_num == -1) {
        fprintf(stderr, "Erro: Diretório '%s' não encontrado.\n", name);
        return -1;
    }
    Inode target_inode;
    if (inode_read(target_inode_num, &target_inode) != 0) return -1;
    if (target_inode.type != TYPE_DIR) {
        fprintf(stderr, "Erro: '%s' não é um diretório.\n", name);
        return -1;
    }
    verbose_printf("Verificando se o diretório (i-node %d) está vazio.\n", target_inode_num);
    if (target_inode.size > sizeof(DirectoryEntry) * 2) {
        fprintf(stderr, "Erro: O diretório '%s' não está vazio.\n", name);
        return -1;
    }
    verbose_printf("Removendo entrada '%s' do diretório pai (i-node %u).\n", name, current_inode_num);
    if (remove_entry_from_directory(&parent_inode, current_inode_num, name) != 0) {
        fprintf(stderr, "Erro ao remover a entrada do diretório pai.\n");
        return -1;
    }
    uint32_t target_block_num = target_inode.direct_blocks[0];
    verbose_printf("Liberando recursos do i-node %d (bloco de dados %u).\n", target_inode_num, target_block_num);
    if (free_block(target_block_num) != 0) { return -1; }
    if (free_inode(target_inode_num) != 0) { return -1; }
    verbose_printf("Decrementando contagem de links do pai %u.\n", current_inode_num);
    parent_inode.link_count--;
    inode_write(current_inode_num, &parent_inode);
    return 0;
}

int fs_import_file(const char* source_path, const char* dest_name) {
    verbose_printf("Iniciando 'import %s' para '%s'.\n", source_path, dest_name);
    FILE* source_file = fopen(source_path, "rb");
    if (!source_file) {
        fprintf(stderr, "Erro: Não foi possível abrir o arquivo de origem '%s'.\n", source_path);
        return -1;
    }
    fseek(source_file, 0, SEEK_END);
    long file_size = ftell(source_file);
    fseek(source_file, 0, SEEK_SET);
    verbose_printf("Arquivo de origem aberto, tamanho: %ld bytes.\n", file_size);
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) { fclose(source_file); return -1; }
    if (find_in_directory(&parent_inode, dest_name) != -1) {
        fprintf(stderr, "Erro: Um item com o nome '%s' já existe.\n", dest_name);
        fclose(source_file);
        return -1;
    }
    int new_inode_num = alloc_inode();
    if (new_inode_num == -1) {
        fprintf(stderr, "Erro: Sem i-nodes livres.\n");
        fclose(source_file);
        return -1;
    }
    uint32_t num_blocks_needed = (file_size + sb.block_size - 1) / sb.block_size;
    verbose_printf("Arquivo necessita de %u blocos de dados.\n", num_blocks_needed);
    if (num_blocks_needed > INODE_DIRECT_BLOCKS) {
        fprintf(stderr, "Erro: O arquivo é muito grande para os blocos diretos.\n");
        free_inode(new_inode_num);
        fclose(source_file);
        return -1;
    }
    uint32_t allocated_blocks[INODE_DIRECT_BLOCKS];
    for (uint32_t i = 0; i < num_blocks_needed; ++i) {
        int block_num = alloc_block();
        if (block_num == -1) {
            fprintf(stderr, "Erro: Sem blocos de dados livres.\n");
            for (uint32_t j = 0; j < i; ++j) { free_block(allocated_blocks[j]); }
            free_inode(new_inode_num);
            fclose(source_file);
            return -1;
        }
        allocated_blocks[i] = block_num;
    }
    verbose_printf("Copiando dados para os blocos alocados...\n");
    char block_buffer[sb.block_size];
    for (uint32_t i = 0; i < num_blocks_needed; ++i) {
        fread(block_buffer, 1, sb.block_size, source_file);
        if (block_write(allocated_blocks[i], block_buffer) != 0) {
            for (uint32_t j = 0; j < num_blocks_needed; ++j) { free_block(allocated_blocks[j]); }
            free_inode(new_inode_num);
            fclose(source_file);
            return -1;
        }
    }
    fclose(source_file);
    verbose_printf("Inicializando i-node %d para o novo arquivo.\n", new_inode_num);
    Inode new_inode;
    new_inode.type = TYPE_FILE;
    new_inode.size = file_size;
    new_inode.link_count = 1;
    new_inode.created = new_inode.modified = new_inode.accessed = time(NULL);
    memset(new_inode.direct_blocks, 0, sizeof(new_inode.direct_blocks));
    for (uint32_t i = 0; i < num_blocks_needed; ++i) {
        new_inode.direct_blocks[i] = allocated_blocks[i];
    }
    if (inode_write(new_inode_num, &new_inode) != 0) { return -1; }
    if (add_entry_to_directory(&parent_inode, current_inode_num, dest_name, new_inode_num) != 0) { return -1; }
    return 0;
}

int fs_remove_file(const char* filename) {
    verbose_printf("Iniciando 'rm %s'.\n", filename);
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) return -1;
    int target_inode_num = find_in_directory(&parent_inode, filename);
    if (target_inode_num == -1) {
        fprintf(stderr, "Erro: Arquivo '%s' não encontrado.\n", filename);
        return -1;
    }
    Inode target_inode;
    if (inode_read(target_inode_num, &target_inode) != 0) return -1;
    if (target_inode.type != TYPE_FILE) {
        fprintf(stderr, "Erro: '%s' não é um arquivo. Use 'rmdir' para diretórios.\n", filename);
        return -1;
    }
    verbose_printf("Liberando blocos de dados do i-node %d...\n", target_inode_num);
    for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
        uint32_t block_num = target_inode.direct_blocks[i];
        if (block_num == 0) continue;
        if (free_block(block_num) != 0) { fprintf(stderr, "Erro crítico ao liberar o bloco de dados %u.\n", block_num); }
    }
    verbose_printf("Liberando i-node %d...\n", target_inode_num);
    if (free_inode(target_inode_num) != 0) { fprintf(stderr, "Erro crítico ao liberar o i-node %d.\n", target_inode_num); }
    verbose_printf("Removendo entrada '%s' do diretório pai.\n", filename);
    if (remove_entry_from_directory(&parent_inode, current_inode_num, filename) != 0) {
        fprintf(stderr, "Erro ao remover a entrada '%s' do diretório pai.\n", filename);
        return -1;
    }
    return 0;
}

int fs_delete(const char* name) {
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) return -1;

    int target_inode_num = find_in_directory(&parent_inode, name);
    if (target_inode_num == -1) {
        fprintf(stderr, "Erro: Item '%s' não encontrado.\n", name);
        return -1;
    }

    Inode target_inode;
    if (inode_read(target_inode_num, &target_inode) != 0) return -1;

    if (target_inode.type == TYPE_FILE) {
        return fs_remove_file(name);
    } else if (target_inode.type == TYPE_DIR) {
        if (fs_change_directory(name) != 0) return -1;

        FileList contents = fs_list_directory();
        for (size_t i = 0; i < contents.count; ++i) {
            if (strcmp(contents.entries[i].name, ".") == 0 || strcmp(contents.entries[i].name, "..") == 0)
                continue;

            fs_delete(contents.entries[i].name);
        }
        free(contents.entries);

        // Volta para o diretório original, não só para ".."
        fs_change_directory("..");

        return fs_remove_directory(name);
    }

    return -1;
}

int fs_rename(const char* old_name, const char* new_name) {
    verbose_printf("Iniciando 'rename %s' para '%s'.\n", old_name, new_name);
    if (strcmp(old_name, ".") == 0 || strcmp(old_name, "..") == 0 || strcmp(new_name, ".") == 0 || strcmp(new_name, "..") == 0) {
        fprintf(stderr, "Erro: Não é permitido renomear '.' ou '..'.\n");
        return -1;
    }
    if (strlen(new_name) >= MAX_FILENAME_LEN) {
        fprintf(stderr, "Erro: Novo nome é muito longo.\n");
        return -1;
    }
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) return -1;
    if (find_in_directory(&parent_inode, old_name) == -1) {
        fprintf(stderr, "Erro: Item '%s' não encontrado.\n", old_name);
        return -1;
    }
    if (find_in_directory(&parent_inode, new_name) != -1) {
        fprintf(stderr, "Erro: Já existe um item com o nome '%s'.\n", new_name);
        return -1;
    }
    verbose_printf("Modificando entrada no bloco de dados do diretório pai (i-node %u).\n", current_inode_num);
    char block_buffer[sb.block_size];
    for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
        uint32_t block_num = parent_inode.direct_blocks[i];
        if (block_num == 0) continue;
        if (block_read(block_num, block_buffer) != 0) return -1;
        DirectoryEntry* entry = (DirectoryEntry*) block_buffer;
        int num_entries = sb.block_size / sizeof(DirectoryEntry);
        for (int j = 0; j < num_entries; ++j) {
            if (strlen(entry[j].name) > 0 && strcmp(entry[j].name, old_name) == 0) {
                strncpy(entry[j].name, new_name, MAX_FILENAME_LEN);
                entry[j].name[MAX_FILENAME_LEN - 1] = '\0';
                if (block_write(block_num, block_buffer) != 0) {
                    fprintf(stderr, "Erro ao escrever as alterações no disco.\n");
                    return -1;
                }
                parent_inode.modified = time(NULL);
                inode_write(current_inode_num, &parent_inode);
                return 0;
            }
        }
    }
    fprintf(stderr, "Erro: Inconsistência encontrada ao tentar renomear.\n");
    return -1;
}

Inode fs_stat_item(const char* name) {
    verbose_printf("Iniciando 'stat %s'.\n", name);
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) return (Inode){0};
    int target_inode_num = find_in_directory(&parent_inode, name);
    if (target_inode_num == -1) {
        return (Inode){0}; // Retorna um i-node vazio se não encontrado
    }
    Inode target_inode;
    if (inode_read(target_inode_num, &target_inode) != 0) return (Inode){0};
    return target_inode;
}

DiskUsageInfo fs_disk_free() {
    verbose_printf("Iniciando 'df'.\n");
    uint32_t used_inodes = count_set_bits(sb.inode_bitmap_start, sb.total_inodes);
    uint32_t used_blocks = count_set_bits(sb.block_bitmap_start, sb.total_blocks);
    if (used_inodes == (uint32_t)-1 || used_blocks == (uint32_t)-1) {
        fprintf(stderr, "Erro ao ler bitmaps do disco.\n");
        return (DiskUsageInfo){0};
    }
    uint32_t free_inodes = sb.total_inodes - used_inodes;
    uint32_t free_blocks = sb.total_blocks - used_blocks;
    uint32_t total_kb = (sb.total_blocks * sb.block_size) / 1024;
    uint32_t used_kb = (used_blocks * sb.block_size) / 1024;
    uint32_t free_kb = (free_blocks * sb.block_size) / 1024;
    return (DiskUsageInfo){
        .total_inodes = sb.total_inodes,
        .used_inodes = used_inodes,
        .free_inodes = free_inodes,
        .total_blocks = sb.total_blocks,
        .used_blocks = used_blocks,
        .free_blocks = free_blocks,
        .total_kb = total_kb,
        .used_kb = used_kb,
        .free_kb = free_kb
    };

}

int fs_check_item_type(const char* name) {
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) return -1;
    int target_inode_num = find_in_directory(&parent_inode, name);
    if (target_inode_num == -1) return -1;
    
    Inode target_inode;
    if (inode_read(target_inode_num, &target_inode) != 0) return -1;
    
    return target_inode.type;
}


int fs_write_file(const char* filename, const char* text, const char* op) {
    verbose_printf("Iniciando 'echo' para o arquivo '%s' (operação: %s)\n", filename, op);
    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) return -1;
    int target_inode_num = find_in_directory(&parent_inode, filename);
    if (strcmp(op, ">") == 0 && target_inode_num != -1) {
        verbose_printf("Arquivo '%s' existe. Removendo para sobrescrever.\n", filename);
        if (fs_remove_file(filename) != 0) {
            fprintf(stderr, "Erro ao tentar sobrescrever o arquivo existente.\n");
            return -1;
        }
        target_inode_num = -1;
    }
    if (target_inode_num == -1) {
        verbose_printf("Arquivo '%s' não existe. Criando novo arquivo.\n", filename);
        int new_inode_num = alloc_inode();
        if (new_inode_num == -1) return -1;
        if (add_entry_to_directory(&parent_inode, current_inode_num, filename, new_inode_num) != 0) {
            free_inode(new_inode_num);
            return -1;
        }
        target_inode_num = new_inode_num;
    }
    Inode target_inode;
    if (inode_read(target_inode_num, &target_inode) != 0) return -1;
    if(target_inode.type == TYPE_DIR) {
        fprintf(stderr, "Erro: Não é possível escrever em um diretório.\n");
        return -1;
    }
    long text_len = strlen(text);
    long new_size = target_inode.size + text_len;
    uint32_t total_blocks_needed = (new_size + sb.block_size - 1) / sb.block_size;
    verbose_printf("Texto com %ld bytes. Novo tamanho do arquivo: %ld bytes. Blocos necessários: %u.\n", text_len, new_size, total_blocks_needed);
    if (total_blocks_needed > INODE_DIRECT_BLOCKS) {
        fprintf(stderr, "Erro: Conteúdo excede o tamanho máximo do arquivo.\n");
        return -1;
    }
    const char* p_text = text;
    char block_buffer[sb.block_size];
    uint32_t current_block_index = (strcmp(op, ">>") == 0) ? (target_inode.size / sb.block_size) : 0;
    uint32_t offset_in_block = (strcmp(op, ">>") == 0) ? (target_inode.size % sb.block_size) : 0;
    if (offset_in_block > 0) {
        block_read(target_inode.direct_blocks[current_block_index], block_buffer);
    }
    while(p_text < text + text_len) {
        if (offset_in_block == 0) {
            memset(block_buffer, 0, sb.block_size);
            if (target_inode.direct_blocks[current_block_index] == 0) {
                int new_block = alloc_block();
                if(new_block == -1) return -1;
                target_inode.direct_blocks[current_block_index] = new_block;
            }
        }
        size_t space_in_block = sb.block_size - offset_in_block;
        size_t bytes_to_write = strlen(p_text);
        if (bytes_to_write > space_in_block) {
            bytes_to_write = space_in_block;
        }
        verbose_printf("Escrevendo %zu bytes no bloco %u (offset %u).\n", bytes_to_write, target_inode.direct_blocks[current_block_index], offset_in_block);
        memcpy(block_buffer + offset_in_block, p_text, bytes_to_write);
        block_write(target_inode.direct_blocks[current_block_index], block_buffer);
        p_text += bytes_to_write;
        offset_in_block = 0;
        current_block_index++;
    }
    target_inode.size = new_size;
    target_inode.modified = target_inode.accessed = time(NULL);
    inode_write(target_inode_num, &target_inode);
    return 0;
}

int fs_move_item(const char* source_name, const char* dest_dir_name) {
    verbose_printf("Iniciando 'mv %s' para '%s'.\n", source_name, dest_dir_name);
    if (strcmp(source_name, ".") == 0 || strcmp(source_name, "..") == 0) {
        fprintf(stderr, "Erro: Não é permitido mover '.' ou '..'.\n");
        return -1;
    }
    if (strcmp(source_name, dest_dir_name) == 0) {
        fprintf(stderr, "Erro: A origem e o destino não podem ser os mesmos.\n");
        return -1;
    }
    Inode current_dir_inode;
    if (inode_read(current_inode_num, &current_dir_inode) != 0) return -1;
    int source_inode_num = find_in_directory(&current_dir_inode, source_name);
    if (source_inode_num == -1) {
        fprintf(stderr, "Erro: Item de origem '%s' não encontrado.\n", source_name);
        return -1;
    }
    int dest_dir_inode_num = find_in_directory(&current_dir_inode, dest_dir_name);
    if (dest_dir_inode_num == -1) {
        fprintf(stderr, "Erro: Diretório de destino '%s' não encontrado.\n", dest_dir_name);
        return -1;
    }
    Inode source_inode, dest_dir_inode;
    if (inode_read(source_inode_num, &source_inode) != 0) return -1;
    if (inode_read(dest_dir_inode_num, &dest_dir_inode) != 0) return -1;
    if (dest_dir_inode.type != TYPE_DIR) {
        fprintf(stderr, "Erro: O destino '%s' não é um diretório.\n", dest_dir_name);
        return -1;
    }
    if (find_in_directory(&dest_dir_inode, source_name) != -1) {
        fprintf(stderr, "Erro: Já existe um item com o nome '%s' no destino.\n", source_name);
        return -1;
    }
    verbose_printf("Movendo i-node %d para o diretório de destino (i-node %d).\n", source_inode_num, dest_dir_inode_num);
    if (add_entry_to_directory(&dest_dir_inode, dest_dir_inode_num, source_name, source_inode_num) != 0) { return -1; }
    if (source_inode.type == TYPE_DIR) {
        verbose_printf("Item movido é um diretório. Atualizando sua entrada '..'.\n");
        char block_buffer[sb.block_size];
        if(block_read(source_inode.direct_blocks[0], block_buffer) != 0) return -1;
        DirectoryEntry* entry = (DirectoryEntry*) block_buffer;
        int num_entries = sb.block_size / sizeof(DirectoryEntry);
        for (int i = 0; i < num_entries; ++i) {
            if (strcmp(entry[i].name, "..") == 0) {
                entry[i].inode_num = dest_dir_inode_num;
                break;
            }
        }
        if(block_write(source_inode.direct_blocks[0], block_buffer) != 0) return -1;
        verbose_printf("Atualizando contagem de links dos diretórios pai (antigo: %u, novo: %u).\n", current_inode_num, dest_dir_inode_num);
        current_dir_inode.link_count--;
        dest_dir_inode.link_count++;
        inode_write(current_inode_num, &current_dir_inode);
        inode_write(dest_dir_inode_num, &dest_dir_inode);
    }
    if (remove_entry_from_directory(&current_dir_inode, current_inode_num, source_name) != 0) {
        fprintf(stderr, "Erro crítico: O item foi copiado para o destino, mas falhou ao ser removido da origem.\n");
        return -1;
    }
    return 0;
}

char* fs_read_file(const char* filename) {
    verbose_printf("Iniciando 'cat %s'.\n", filename);

    Inode parent_inode;
    if (inode_read(current_inode_num, &parent_inode) != 0) return NULL;

    int target_inode_num = find_in_directory(&parent_inode, filename);
    if (target_inode_num == -1) {
        fprintf(stderr, "Erro: Arquivo '%s' não encontrado.\n", filename);
        return NULL;
    }

    Inode target_inode;
    if (inode_read(target_inode_num, &target_inode) != 0) return NULL;

    if (target_inode.type != TYPE_FILE) {
        fprintf(stderr, "Erro: '%s' não é um arquivo.\n", filename);
        return NULL;
    }

    verbose_printf("Lendo %u bytes do arquivo (i-node %d).\n", target_inode.size, target_inode_num);

    if (target_inode.size == 0) {
        // Arquivo vazio, retorna string vazia alocada
        char* empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    char* content = malloc(target_inode.size + 1); // +1 para o '\0'
    if (!content) {
        fprintf(stderr, "Erro de alocação de memória.\n");
        return NULL;
    }

    char block_buffer[sb.block_size];
    long bytes_left_to_read = target_inode.size;
    size_t offset = 0;

    for (int i = 0; i < INODE_DIRECT_BLOCKS; ++i) {
        uint32_t block_num = target_inode.direct_blocks[i];
        if (block_num == 0 || bytes_left_to_read == 0) break;

        if (block_read(block_num, block_buffer) != 0) {
            fprintf(stderr, "Erro ao ler bloco de dados do arquivo.\n");
            free(content);
            return NULL;
        }

        size_t bytes_to_copy = (bytes_left_to_read > sb.block_size) ? sb.block_size : bytes_left_to_read;
        memcpy(content + offset, block_buffer, bytes_to_copy);

        offset += bytes_to_copy;
        bytes_left_to_read -= bytes_to_copy;
    }

    content[target_inode.size] = '\0'; // Garantir terminação nula

    verbose_printf("Atualizando timestamp de acesso do i-node %d.\n", target_inode_num);
    target_inode.accessed = time(NULL);
    inode_write(target_inode_num, &target_inode);

    return content;
}

void fs_set_verbose(int mode) {
    verbose_mode = mode;
    printf("Modo verboso %s.\n", verbose_mode ? "ativado" : "desativado");
}

int fs_format(const char* path, uint32_t total_size_kb, uint32_t block_size_kb) {
    uint32_t total_size = total_size_kb * 1024;
    uint32_t block_size = block_size_kb * 1024;
    if (block_size == 0 || total_size == 0) {
        fprintf(stderr, "Erro: Tamanho do disco e do bloco devem ser maiores que zero.\n");
        return -1;
    }
    sb.block_size = block_size;
    sb.total_blocks = total_size / block_size;
    sb.total_inodes = sb.total_blocks / 4;
    if (sb.total_inodes < 16) sb.total_inodes = 16;
    sb.magic_number = MAGIC_NUMBER;
    sb.inode_bitmap_start = 1;
    uint32_t inode_bitmap_blocks = (sb.total_inodes + 8 * block_size - 1) / (8 * block_size);
    sb.block_bitmap_start = sb.inode_bitmap_start + inode_bitmap_blocks;
    uint32_t block_bitmap_blocks = (sb.total_blocks + 8 * block_size - 1) / (8 * block_size);
    sb.inode_table_start = sb.block_bitmap_start + block_bitmap_blocks;
    uint32_t inode_table_blocks = (sb.total_inodes * sizeof(Inode) + block_size - 1) / block_size;
    sb.data_blocks_start = sb.inode_table_start + inode_table_blocks;
    if (sb.data_blocks_start >= sb.total_blocks) {
        fprintf(stderr, "Erro: Tamanho do disco insuficiente para os metadados.\n");
        return -1;
    }
    disk_file = fopen(path, "wb+");
    if (!disk_file) {
        perror("Erro ao criar arquivo de disco");
        return -1;
    }
    void* zero_block = calloc(1, block_size);
    for (uint32_t i = 0; i < sb.total_blocks; ++i) {
        if (fwrite(zero_block, block_size, 1, disk_file) != 1) {
            fprintf(stderr, "Erro ao zerar o disco\n");
            free(zero_block);
            fclose(disk_file);
            return -1;
        }
    }
    free(zero_block);
    fflush(disk_file);
    void* block_buffer = malloc(block_size);
    memset(block_buffer, 0, block_size);
    memcpy(block_buffer, &sb, sizeof(Superblock));
    if (block_write(0, block_buffer) != 0) {
        fprintf(stderr, "Erro ao escrever superbloco.\n");
        fclose(disk_file);
        free(block_buffer);
        return -1;
    }
    fflush(disk_file);
    int root_inode_num = find_free_bit_from(sb.inode_bitmap_start, sb.total_inodes, 0);
    if (root_inode_num != 0) {
        fprintf(stderr, "Erro: O primeiro i-node alocado não foi o 0.\n");
        goto fail;
    }
    set_bit(sb.inode_bitmap_start, root_inode_num, 1);
    uint32_t root_data_block_num = sb.data_blocks_start;
    if (set_bit(sb.block_bitmap_start, root_data_block_num, 1) != 0) {
        fprintf(stderr, "Erro: Falha ao alocar bloco de dados para o diretório raiz.\n");
        goto fail;
    }
    Inode root_inode;
    root_inode.type = TYPE_DIR;
    root_inode.size = sizeof(DirectoryEntry) * 2;
    root_inode.link_count = 2;
    root_inode.created = root_inode.modified = root_inode.accessed = time(NULL);
    for(int i=0; i<INODE_DIRECT_BLOCKS; ++i) root_inode.direct_blocks[i] = 0;
    root_inode.direct_blocks[0] = root_data_block_num;
    if (inode_write(root_inode_num, &root_inode) != 0) {
        fprintf(stderr, "Erro ao escrever o i-node raiz.\n");
        goto fail;
    }
    DirectoryEntry dot_entry;
    strncpy(dot_entry.name, ".", MAX_FILENAME_LEN);
    dot_entry.inode_num = root_inode_num;
    DirectoryEntry dotdot_entry;
    strncpy(dotdot_entry.name, "..", MAX_FILENAME_LEN);
    dotdot_entry.inode_num = root_inode_num;
    memset(block_buffer, 0, block_size);
    memcpy(block_buffer, &dot_entry, sizeof(DirectoryEntry));
    memcpy(block_buffer + sizeof(DirectoryEntry), &dotdot_entry, sizeof(DirectoryEntry));
    if (block_write(root_data_block_num, block_buffer) != 0) {
        fprintf(stderr, "Erro ao escrever o bloco de dados do diretório raiz.\n");
        goto fail;
    }
    fflush(disk_file);
    fclose(disk_file);
    disk_file = NULL;
    free(block_buffer);
    printf("Disco formatado com sucesso.\n");
    printf("Diretório raiz criado no i-node %d e bloco de dados %u.\n", root_inode_num, root_data_block_num);
    return 0;
fail:
    fclose(disk_file);
    disk_file = NULL;
    free(block_buffer);
    return -1;
}

int fs_mount(const char* path) {
    disk_file = fopen(path, "rb+");
    if (!disk_file) {
        return -1;
    }
    fseek(disk_file, 0, SEEK_SET);
    Superblock temp_sb;
    if (fread(&temp_sb, sizeof(Superblock), 1, disk_file) != 1) {
        fprintf(stderr, "Erro: Não foi possível ler o superbloco do disco.\n");
        fclose(disk_file);
        disk_file = NULL;
        return -1;
    }
    if (temp_sb.magic_number != MAGIC_NUMBER) {
        fprintf(stderr, "Erro: Magic number inválido (lido: 0x%X, esperado: 0x%X).\n", temp_sb.magic_number, MAGIC_NUMBER);
        fprintf(stderr, "O arquivo não é um disco do nosso sistema ou está corrompido.\n");
        fclose(disk_file);
        disk_file = NULL;
        return -1;
    }
    memcpy(&sb, &temp_sb, sizeof(Superblock));
    current_inode_num = 0;
    return 0;
}

void fs_unmount() {
    if (disk_file) {
        fclose(disk_file);
        disk_file = NULL;
    }
}
