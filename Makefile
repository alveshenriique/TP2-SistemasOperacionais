# Compilador
CC=gcc
# Flags de compilação: -g para debug, -Wall para warnings
CFLAGS=-g -Wall -Iinclude
# Diretório dos includes
IDIR=include
# Diretório do código fonte
SDIR=src
# Nome do executável final
TARGET=simulador
# Flags do GTK (detectadas automaticamente)
GTK_FLAGS=`pkg-config --cflags --libs gtk+-3.0`

# Encontra todos os arquivos .c no diretório src
SOURCES=$(wildcard $(SDIR)/*.c)
# Gera os nomes dos arquivos objeto (.o) a partir dos fontes
OBJECTS=$(SOURCES:.c=.o)

# Regra principal: criar o executável
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(GTK_FLAGS)

# Regra para compilar arquivos .c em .o
%.o: %.c
	$(CC) $(CFLAGS) -I$(IDIR) $(GTK_FLAGS) -c $< -o $@

# Regra para limpar os arquivos gerados
clean:
	rm -f $(SDIR)/*.o $(TARGET)

.PHONY: all clean