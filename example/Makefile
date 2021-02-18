BIN_DIR_LLVM=../build/bin

CFLAGS=-O0 -g3
CXXFLAGS=-O0 -g3
SANITIZER_FLAGS=-fsanitize=dataflow
SANITIZER_ADDL_FLAGS=

SRCS=$(wildcard *.c)

EXES=$(SRCS:.c=.exe)
BITCODES=$(SRCS:.c=.dfsan.bc)
LLS=$(SRCS:.c=.plain.ll)
CC=$(BIN_DIR_LLVM)/clang
CXX=$(BIN_DIR_LLVM)/clang++
DIS=$(BIN_DIR_LLVM)/llvm-dis
OPT=$(BIN_DIR_LLVM)/opt

all: $(LLS) $(BITCODES) $(EXES)

%.exe: %.c
	$(CC) $(CXXFLAGS) $< -o $@ $(SANITIZER_FLAGS) $(SANITIZER_ADDL_FLAGS)

%.exe: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(SANITIZER_FLAGS) $(SANITIZER_ADDL_FLAGS) -fsanitize-blacklist=cpp_blacklist.txt

%.plain.ll:	%.c
	$(CC) $(CFLAGS) -emit-llvm -S $< -o $@

%.plain.ll:	%.cpp
	$(CXX) $(CXXFLAGS) -emit-llvm -S $< -o $@ 

%.dfsan.bc:	%.c
	$(CC) $(CFLAGS) -c -emit-llvm $< -o $@ $(SANITIZER_FLAGS) $(SANITIZER_ADDL_FLAGS) 
	$(DIS) $@

%.dfsan.bc:	%.cpp
	$(CXX) $(CXXFLAGS) -c -emit-llvm $< -o $@ $(SANITIZER_FLAGS) $(SANITIZER_ADDL_FLAGS)  -fsanitize-blacklist=cpp_blacklist.txt
	$(DIS) $@

clean:
	rm *.exe *.bc *.ll 2>/dev/null || true

