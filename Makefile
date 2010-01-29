all: libaudit.a audit.h test_audit.so

install: libaudit.a audit.h
	install audit.h /usr/local/include
	install libaudit.a /usr/local/lib

clean:
	rm -f *.a *.o *.so

test_audit.so: libaudit.a test_audit.o
	gcc -fPIC -nostdlib -shared -o test_audit.so test_audit.o libaudit.a -Wl,-u -Wl,la_version

test_audit.o: test_audit.c audit.h
	gcc -shared -c -fPIC test_audit.c -Wall

libaudit.a: audit.o trampoline.o libc.o malloc.o helpers.o
	ar -rcs libaudit.a audit.o trampoline.o libc.o malloc.o helpers.o

audit.o: audit.c audit.h
	gcc -c -fPIC -nostdlib -shared audit.c -Wall

libc.o: libc.c audit.h
	gcc -c -fPIC -nostdlib -shared libc.c -Wall

malloc.o: malloc.c audit.h
	gcc -c -fPIC -nostdlib -shared malloc.c -Wall

trampoline.o: trampoline.S
	gcc -c trampoline.S -o trampoline.o

helpers.o: helpers.S
	gcc -c helpers.S -o helpers.o
