#include "common.hpp"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <bits/stdc++.h>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 1024

using namespace std; 

void usage(int argc, char **argv) {
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data {
    int csock;
    struct sockaddr_storage storage;
};

map<string, vector<int> > tags;

void manda(int csock, string msg){
    size_t count = send(csock, msg.c_str(), strlen(msg.c_str()) + 1, 0);
    if (count != strlen(msg.c_str()) + 1) {
        logexit("send");
    }
}

string adiciona_tag(string buf_str, int csock){
    if (tags.find(buf_str) == tags.end()) {
    tags[buf_str].push_back(csock);
    char retorno[BUFSZ];
    sprintf(retorno,  "subscribed +%s", buf_str.c_str());
    return retorno;
    } else {
        vector <int>::iterator it;
        it = find(tags[buf_str].begin(), tags[buf_str].end(), csock);
        if (it != tags[buf_str].end()){
            char retorno[BUFSZ];
            sprintf(retorno,  "already subscribed +%s", buf_str.c_str());
            return retorno;
        }
        else{
            tags[buf_str].push_back(csock);
            char retorno[BUFSZ];
            sprintf(retorno,  "subscribed +%s", buf_str.c_str());
            return retorno;
        }

    }

}

string deleta_tag(string buf_str, int csock){
    for (unsigned int i = 0; i < tags[buf_str].size(); i++){
        if (tags[buf_str][i] == csock){
            tags[buf_str].erase(tags[buf_str].begin() + i);
            char retorno[BUFSZ];
            sprintf(retorno,  "unsubscribed -%s", buf_str.c_str());
            return retorno;
        }
    }
    char retorno[BUFSZ];
    sprintf(retorno,  "not subscribed -%s", buf_str.c_str());
    return retorno;
}

void encaminha_msg(char buf[], int csock, string buf_str){
    vector <string> tag;
        int notfound = 0;
        for (unsigned int i = 0; i < strlen(buf); i++){
            if(buf[i] == '#' and (i == 0 or buf[i-1] == ' ')){
                vector <char> aux;
                unsigned int j;
                for (j = i+1; j < strlen(buf); j++){
                    if(buf[j] == ' ' or buf[j] == '\n') break;
                    if(buf[j] == '#'){
                        notfound=1; 
                        break;
                    } 
                    aux.push_back(buf[j]);
                }
                if (notfound == 0){
                    string t(aux.begin(), aux.end());
                    tag.push_back(t);
                }
                i += j;
            }
        }
        vector <int> visited;
        vector <int>::iterator it;
        for (unsigned int i = 0; i < tag.size(); i++){
            cout << "[log] quantidade de clientes com a tag " << tag[i] << " eh: " << tags[tag[i]].size() << "\n";
            for (unsigned int j = 0; j < tags[tag[i]].size(); j++){
                if (tags[tag[i]][j] != csock){
                    it = find(visited.begin(), visited.end(), tags[tag[i]][j]);
                    if (it == visited.end()){
                        manda(tags[tag[i]][j], buf_str);
                        visited.push_back(tags[tag[i]][j]);
                    }
                }
            }
        }
}

string recebe(int csock){
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    size_t count = recv(csock, buf, BUFSZ - 1, 0);
    
    string buf_str = buf;
    printf("[msg] %d bytes: %s", (int)count, buf);
    // if (strcmp(buf, "##kill\n"))
    //     //do something
    if (buf[0] == '+'){
        buf_str.erase(0, 1);
        buf_str.erase(buf_str.end()-1);
        return adiciona_tag(buf_str, csock);    
    }

    else if (buf[0] == '-'){
        buf_str.erase(0, 1);
        buf_str.erase(buf_str.end()-1);
        return deleta_tag(buf_str, csock);
    }

    else {
        encaminha_msg(buf, csock, buf_str);
        return "[log] Mensagem enviada com sucesso!";
    }
}

void * client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr);

    while(1){
        string msg= recebe(cdata->csock);
        manda(cdata->csock, msg);
    }
    close(cdata->csock);

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("bind");
    }

    if (0 != listen(s, 10)) {
        logexit("listen");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    while (1) {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("accept");
        }

        struct client_data *cdata = (client_data *) malloc(sizeof(struct client_data));
        if (!cdata) {
            logexit("malloc");
        }
        cdata->csock = csock;
        memcpy(&(cdata->storage), &cstorage, sizeof(cstorage));

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata);

        // for(map<string,vector<int> >::iterator it = tags.begin(); it != tags.end(); ++it) {
        //     cout << "Key: " << it->first << "\n";
        //     cout << "Value: " << it->second.size() << "\n";
        // }
    }

    exit(EXIT_SUCCESS);
}
