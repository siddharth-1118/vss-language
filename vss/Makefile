CC = cc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -g
INCLUDES = -Iinclude
SRC = src/main.c src/token.c src/lexer.c src/ast.c src/value.c src/env.c src/parser.c src/interpreter.c src/chunk.c src/object.c src/compiler.c src/vm.c src/cli.c
OUT = vss

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) -o $(OUT)

run: $(OUT)
	./$(OUT) examples/hello.vss

clean:
	rm -f $(OUT)

.PHONY: all run clean
