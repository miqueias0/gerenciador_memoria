CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -fno-pie -I.
LDFLAGS = -no-pie

TARGET = saida
OBJS = main.o gerenciador.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

debug:
	$(MAKE) CFLAGS="$(CFLAGS) -DDEBUG" OBJS="$(OBJS) tests/testes.o" clean all

clean:
	rm -f $(OBJS) tests/testes.o $(TARGET)

.PHONY: all clean debug
