
#NAME: Calvin Yung
#EMAIL: yung.calvin322@gmail.com
#ID: 304787184

default:
	gcc -Wall -Wextra -lz -std=c11 -o client client.c
	gcc -Wall -Wextra -lz -std=c11 -o server server.c
clean:
	rm -f *.o server client *.tar.gz
dist:
	tar -cvzf 304787184.tar.gz makefile README server.c client.c
maketests:
	echo "Creating garbage test files"
	head -c 100000 /dev/urandom >reallybigtest.file
	head -c 1500 /dev/urandom >bigtest.file
testclean:
	rm -f *.file
test:
	make testclean
	make maketests
	./client localhost 8888 reallybigtest.file
	./client localhost 8888 bigtest.file
	diff reallybigtest.file 1.file
	diff bigtest.file 2.file
testserver:
	./server 8888
