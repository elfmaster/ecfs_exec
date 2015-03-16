all: exec
exec: exec.o
	gcc -DDEBUG -g -fPIC -pie exec.o -o exec -ldl
exec.o:
	gcc -c -pie -fPIC exec.c
clean:
	rm -f *.o exec
	
	
