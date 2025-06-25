// include/fs_core.h
#ifndef FS_CORE_H
#define FS_CORE_H

#include <stdint.h>
#include "fs_types.h"

// Formata um novo disco com o tamanho total e de bloco especificados (em KB)
int fs_format(const char* path, uint32_t total_size_kb, uint32_t block_size_kb);

// Monta (abre) um disco existente
int fs_mount(const char* path);

// Desmonta (fecha) o disco
void fs_unmount();


int fs_list_directory();
int fs_create_directory(const char* name);
int fs_change_directory(const char* name);
void fs_get_current_path(char* path_buffer, size_t buffer_size);
int fs_remove_directory(const char* name);
int fs_import_file(const char* source_path, const char* dest_name);
int fs_read_file(const char* filename);
int fs_remove_file(const char* filename);
int fs_rename(const char* old_name, const char* new_name);
int fs_move_item(const char* source_name, const char* dest_dir_name);
void fs_set_verbose(int mode);
//Extras
int fs_stat_item(const char* name);
int fs_disk_free();
int fs_write_file(const char* filename, const char* text, const char* op);


#endif // FS_CORE_H