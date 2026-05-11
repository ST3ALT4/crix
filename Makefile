CC      = clang
CFLAGS  = -I./include -Wall -Wextra \
          $(shell llvm-config --cflags) \
          $(shell pkg-config --cflags libdw 2>/dev/null)
LDFLAGS = $(shell llvm-config --ldflags) -lclang \
          $(shell pkg-config --libs libdw 2>/dev/null || echo "-ldw -lelf")

SRC = src/main.c \
      src/static_analyzer.c \
      src/ast_visitor.c \
      src/report.c \
      src/runtime_analyzer.c

OBJ = $(SRC:.c=.o)
OUT = crix

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(OUT)

.PHONY: all clean
