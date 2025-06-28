# Simulador Sistemas de Arquivos

## 1. Preparação do Ambiente

Siga os passos abaixo para configurar e executar o simulador.

### 1.1. Compilar o Projeto

No terminal, na pasta raiz do projeto (onde está o Makefile), execute:

```bash
make clean && make
```

### 1.2. Criar o Disco (Primeira Vez ou para Resetar)

O simulador utiliza um arquivo de "disco". Para criar um novo disco de 2MB (2048 KB) com blocos de 1KB:

```bash
./simulador create 2048 1
```

Esse comando apaga qualquer `meu_sistema.disk` antigo e cria um novo, já formatado e pronto para uso.

### 1.3. Executar o Simulador

Para iniciar o shell do simulador:

```bash
./simulador run
```

Você verá uma mensagem de boas-vindas e o prompt `fs:/$`, pronto para receber comandos.


## 2. Guia de Comandos

### ls

Lista o conteúdo do diretório atual.

```shell
fs:/$ ls
```

### mkdir `<nome_dir>`

Cria um novo diretório.

```shell
fs:/$ mkdir projetos
```

### cd `<nome_dir>`

Navega para dentro de um diretório ou para o diretório pai (`..`).

```shell
fs:/$ cd projetos
fs:/projetos$ cd ..
```

### rmdir `<nome_dir>`

Remove um diretório vazio.

```shell
fs:/$ rmdir temp
```

### import `<caminho_real>` `<nome_dest>`

Importa um arquivo do seu computador para o simulador.

```shell
fs:/$ import teste.txt meu_arquivo.txt
```

### cat `<nome_arquivo>`

Exibe o conteúdo de um arquivo.

```shell
fs:/$ cat meu_arquivo.txt
```

### rm `<nome_arquivo>`

Remove um arquivo.

```shell
fs:/$ rm meu_arquivo.txt
```

### echo `"texto"` `>`/`>>` `<arquivo>`

Escreve (`>`) ou anexa (`>>`) texto a um arquivo.

```shell
fs:/$ echo "linha 1" > notas.txt
fs:/$ echo "linha 2" >> notas.txt
```

### rename `<antigo>` `<novo>`

Renomeia um arquivo ou diretório.

```shell
fs:/$ rename pasta documentos
```

### mv `<origem>` `<dir_destino>`

Move um arquivo ou diretório para outro diretório.

```shell
fs:/$ mv mover.txt backup
```

### stat `<nome_item>`

Exibe os metadados (i-node) de um item.

```shell
fs:/$ stat documentos
```

### df

Mostra o uso geral do disco (i-nodes e blocos).

```shell
fs:/$ df
```

### set verbose on|off

Ativa/desativa o modo detalhado de operações de disco.

```shell
fs:/$ set verbose on
fs:/$ set verbose off
```

### exit

Encerra o simulador.


## 3. Teste Integrado com Script

Para testar todas as funcionalidades juntas:

1. Certifique-se de que o arquivo `montar_ambiente.txt` está na pasta do projeto.
2. Execute:

```bash
./simulador run < montar_ambiente.txt
```

Verifique a saída para garantir que todos os comandos foram executados corretamente.


## 4. Dependências

- GCC (compilador C)
- Make

