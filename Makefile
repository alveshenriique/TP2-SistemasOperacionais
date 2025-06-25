# Compilador
CC=gcc
# Flags de compilação: -g para debug, -Wall para warnings
CFLAGS=-g -Wall
# Diretório dos includes
IDIR=include
# Diretório do código fonte
SDIR=src
# Nome do executável final
TARGET=simulador

# Encontra todos os arquivos .c no diretório src
SOURCES=$(wildcard $(SDIR)/*.c)
# Gera os nomes dos arquivos objeto (.o) a partir dos fontes
OBJECTS=$(SOURCES:.c=.o)

# Regra principal: criar o executável
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Regra para compilar arquivos .c em .o
# A flag -I$(IDIR) diz ao compilador para procurar arquivos de cabeçalho na pasta 'include'
%.o: %.c
	$(CC) $(CFLAGS) -I$(IDIR) -c $< -o $@

# Regra para limpar os arquivos gerados
clean:
	rm -f $(SDIR)/*.o $(TARGET)

.PHONY: all clean