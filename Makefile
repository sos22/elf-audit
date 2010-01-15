audit.so: audit.c trampoline.o
	gcc -g -fPIC -nostdlib -shared audit.c trampoline.o -o audit.so -Wall

trampoline.o: trampoline.S
	gcc -c trampoline.S -o trampoline.o
