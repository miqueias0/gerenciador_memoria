CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -no-pie

TARGET = saida
OBJS = main.o gerenciador.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

debug: CFLAGS += -DDEBUG
debug: $(OBJS) tests/testes.o
	$(CC) $^ -o $(TARGET)

clean:
	rm -f $(OBJS) tests/testes.o $(TARGET)

.PHONY: all clean debug