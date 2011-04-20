all:
	gcc -o ratout ratout.c -lcurl
	gcc -o ratout-server ratout-server.c -lcurl -I /usr/include/libxml2 -lxml2