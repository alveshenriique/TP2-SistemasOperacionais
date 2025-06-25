#ifndef COMMANDS_H
#define COMMANDS_H


// Declaração de todas as funções de comando que o shell pode chamar.
void cmd_mkdir(const char* nome_dir);
void cmd_ls();
void cmd_cd(const char* nome_dir);
void cmd_import(const char* caminho_real, const char* nome_dest);
void cmd_cat(const char* nome_arq);
void cmd_rename(const char* nome_orig, const char* nome_novo);
void cmd_mv(const char* nome_orig, const char* nome_dest);
void cmd_rm(const char* nome_arq);
void cmd_rmdir(const char* nome_dir);
void cmd_stat(const char* name);
void cmd_df(void);
void cmd_echo(const char* text, const char* op, const char* filename);
void cmd_set(const char* param, const char* value);

#endif // COMMANDS_H