CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = simulador
SRC = simulador_fs.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) filesystem.dat

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET) comandos.txt

.PHONY: all clean run test