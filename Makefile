.DEFAULT_GOAL := p3

p3:
	gcc -shared -o libmfs.so -fPIC udp.c mfs.c
	gcc -o server -fPIC server.c libmfs.so

test:
	gcc -o tester test37.c libmfs.so

client:
	gcc -o testclient client.c udp.c

clean:
	rm -f libmfs.so
	rm -f server
	rm -f hello.mfs


