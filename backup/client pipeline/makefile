
#NAME: Calvin Yung
#EMAIL: yung.calvin322@gmail.com
#ID: 304787184

default:
	gcc -Wall -Wextra -lz -std=c11 -o client client.c
	gcc -Wall -Wextra -lz -std=c11 -o server server.c
clean:
	rm -f *.o server client *.tar.gz
dist:
	tar -cvzf 304787184.tar.gz Makefile README server.c client.c
