CC = cc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -g
INCLUDES = -Iinclude
SRC = src/main.c src/token.c src/lexer.c src/ast.c src/value.c src/env.c src/parser.c src/interpreter.c src/semantic.c src/chunk.c src/object.c src/compiler.c src/vm.c src/cli.c src/platform.c

# Version injection: make VERSION=1.0.0 BUILD_TYPE=Release
VERSION ?= dev
BUILD_TYPE ?= Debug
VERSION_FLAGS = -DVSS_VERSION_STRING=\"$(VERSION)\" -DVSS_BUILD_TYPE=\"$(BUILD_TYPE)\"

ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    OUT = vss.exe
else
    LIBS = 
    OUT = vss
endif

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(VERSION_FLAGS) $(INCLUDES) $(SRC) $(LIBS) -o $(OUT)

run: $(OUT)
	./$(OUT) examples/hello.vss

clean:
	rm -f $(OUT)

.PHONY: all run clean
