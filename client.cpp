#include "common.hpp"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <bits/stdc++.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void usage(int argc, char **argv) {
	printf("usage: %s <server IP> <server port>\n", argv[0]);
	printf("example: %s 127.0.0.1 51511\n", argv[0]);
	exit(EXIT_FAILURE);
}

#define BUFSZ 500

struct connect_data{
	int csock;
};

void *manda(void *data){
	while(1){
		struct connect_data *cdata = (struct connect_data *)data;
		char buf[BUFSZ];
		memset(buf, 0, BUFSZ);
		fgets(buf, BUFSZ-1, stdin);
		std::string msg;// = "+dota\n+lol\n";
		size_t count = send(cdata->csock, buf, BUFSZ+1 , 0);
		// // std::cout << count << "\n";
		// msg = "+teste\n #teste isso e uma mensagem ";
		// count = send(cdata->csock, msg.c_str(), strlen(msg.c_str())+1 , 0);
		// std::cout << count << "\n";
		// msg = "particionada\n";
		// count = send(cdata->csock, msg.c_str(), strlen(msg.c_str())+1 , 0);
		if (count != strlen(buf)+1) {
			logexit("send");
		}
	}
	pthread_exit(EXIT_SUCCESS);
}

void *recebe(void *data){
	while(1){
		struct connect_data *cdata = (struct connect_data *)data;
		char buf[BUFSZ];
		memset(buf, 0, BUFSZ);
		int total = 0;
		size_t count = recv(cdata->csock, buf + total, BUFSZ - total, 0);
		if (count == 0)
		 	exit(0);
		
		std::cout << buf;
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
		
	struct connect_data *cdata = (connect_data *) malloc(sizeof(struct connect_data));
	cdata->csock = s;
	pthread_t in;
	pthread_t out;
	pthread_create(&in, NULL, manda, cdata);
	pthread_create(&out, NULL, recebe, cdata);

	pthread_join(in, NULL);
	pthread_join(out, NULL);
	
	close(s);

	exit(EXIT_SUCCESS);
}