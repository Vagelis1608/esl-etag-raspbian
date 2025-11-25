all: build

build:
	gcc -c ./btferret/btlib.c -o ./btferret/btlib.o
	ar rcs ./btferret/btlib.a ./btferret/btlib.o
	g++ --std=c++20 -o esl esl.cpp -L. -l:btferret/btlib.a