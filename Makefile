test_audit.so: libaudit.a test_audit.o
	gcc -fPIC -nostdlib -shared -o test_audit.so libaudit.a test_audit.o -Wl,-u -Wl,la_version

test_audit.o: test_audit.c
	gcc -shared -c -fPIC test_audit.c -Wall

libaudit.a: audit.o trampoline.o libc.o
	ar -rcs libaudit.a audit.o trampoline.o libc.o

audit.o: audit.c audit.h
	gcc -c -fPIC -nostdlib -shared audit.c -Wall

libc.o: libc.c audit.h
	gcc -c -fPIC -nostdlib -shared libc.c -Wall

trampoline.o: trampoline.S
	gcc -c trampoline.S -o trampoline.o
