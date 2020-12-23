#include "common.hpp"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void usage(int argc, char **argv) {
	printf("usage: %s <server IP> <server port>\n", argv[0]);
	printf("example: %s 127.0.0.1 51511\n", argv[0]);
	exit(EXIT_FAILURE);
}

#define BUFSZ 1024

struct connect_data{
	int csock;
};

void *manda(void *data){
	struct connect_data *cdata = (struct connect_data *)data;
	char buf[BUFSZ];
	memset(buf, 0, BUFSZ);
	//printf("mensagem> ");
	fgets(buf, BUFSZ-1, stdin);
	size_t count = send(cdata->csock, buf, strlen(buf)+1, 0);
	if (count != strlen(buf)+1) {
		logexit("send");
	}
	pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		usage(argc, argv);
	}

	struct sockaddr_storage storage;
	if (0 != addrparse(argv[1], argv[2], &storage)) {
		usage(argc, argv);
	}

	int s;
	s = socket(storage.ss_family, SOCK_STREAM, 0);
	if (s == -1) {
		logexit("socket");
	}
	struct sockaddr *addr = (struct sockaddr *)(&storage);
	if (0 != connect(s, addr, sizeof(storage))) {
		logexit("connect");
	}

	char addrstr[BUFSZ];
	addrtostr(addr, addrstr, BUFSZ);

	printf("connected to %s\n", addrstr);
	while(1){
		
		struct connect_data *cdata = (connect_data *) malloc(sizeof(struct connect_data));
		cdata->csock = s;
		pthread_t in;
		pthread_create(&in, NULL, manda, cdata);
		// pthread_t out;
		// pthread_create(&out, NULL, recebe, cdata);


		char buf[BUFSZ];
		memset(buf, 0, BUFSZ);
		unsigned total = 0;
		size_t count = recv(cdata->csock, buf + total, BUFSZ - total, 0);
		total += count;
		

		//printf("received %u bytes\n", total);
		puts(buf);
	}
	close(s);

	exit(EXIT_SUCCESS);
}