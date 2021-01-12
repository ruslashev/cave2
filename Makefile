CC = gcc
OBJ = cave.o gfx.o
CFLAGS = -g
LFLAGS = -lSDL2
BIN = cave

all: $(BIN)
	./$(BIN)

$(BIN): $(OBJ)
	@echo ld $@
	@$(CC) $^ -o $@ $(LFLAGS)

%.o: %.c
	@echo cc $^
	@$(CC) $^ -o $@ $(CFLAGS) -c

clean:
	rm -f $(OBJ) $(BIN)
