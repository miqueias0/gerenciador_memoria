CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS =

TARGET = saida
OBJS = main.o gerenciador.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean