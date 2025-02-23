
GCC_ARGS=-lpthread -Wall -O3 -Wextra
.PHONY=build

run: build
	./main

build:
	gcc main.cc -o main ${GCC_ARGS}

