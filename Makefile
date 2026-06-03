CXX      = clang++
CXXFLAGS = -I./include -Wall -Wextra \
           $(shell llvm-config --cxxflags) \
           $(shell pkg-config --cflags libdw 2>/dev/null) \
           -I/home/ani/software/opensource/cppdap/include
LDFLAGS  = $(shell llvm-config --ldflags) -lclang \
           $(shell pkg-config --libs libdw 2>/dev/null || echo "-ldw -lelf") \
           -L/home/ani/software/opensource/cppdap/build -lcppdap

SRC = src/main.cpp \
      src/static_analyzer.cpp \
      src/ast_visitor.cpp \
      src/report.cpp \
      src/runtime_analyze.cpp

OBJ = $(SRC:.cpp=.o)
OUT = crix

all: $(OUT)

$(OUT): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(OUT)

.PHONY: all clean
