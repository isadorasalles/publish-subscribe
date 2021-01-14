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

#define BUFSZ 500

using namespace std; 

void usage(int argc, char **argv) {
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data {
    int csock;
    int id;
    struct sockaddr_storage storage;
};

map<int, int> id_to_csock;
pthread_mutex_t mutex_id;

map<string, vector<int> > tags;
pthread_mutex_t mutex_tags;

vector<int> actives_sockets;
pthread_mutex_t mutex_actives_socks;


void manda(int csock, string msg){
    cout << "mensagem a ser enviada: " << msg;
    size_t count = send(csock, msg.c_str(), strlen(msg.c_str()), 0);
    if (count != strlen(msg.c_str())) {
        logexit("send");
    }
}

string adiciona_tag(string msg, int id){
    pthread_mutex_lock(&mutex_tags);
    vector <int>::iterator it;
    it = find(tags[msg].begin(), tags[msg].end(), id);
    if (it != tags[msg].end()){
        pthread_mutex_unlock(&mutex_tags);
        return "already subscribed +"+msg+"\n";
    }
    else{
        tags[msg].push_back(id);
        pthread_mutex_unlock(&mutex_tags);
        return "subscribed +"+msg+"\n";
    }
}

string deleta_tag(string msg, int id){
    pthread_mutex_lock(&mutex_tags);
    vector <int>::iterator it;
    it = find(tags[msg].begin(), tags[msg].end(), id);
    if (it != tags[msg].end()){
        tags[msg].erase(it);
        pthread_mutex_unlock(&mutex_tags);
        return "unsubscribed -"+msg+"\n";
    }
    pthread_mutex_unlock(&mutex_tags);
    return "not subscribed -"+msg+"\n";
}

void encaminha_msg(string buf, int id){
    vector <string> tag;
        int notfound = 0;
        for (unsigned int i = 0; i < buf.size(); i++){
            if(buf[i] == '#' and (i == 0 or buf[i-1] == ' ')){
                vector <char> aux;
                unsigned int j;
                for (j = i+1; j < buf.size(); j++){
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
        set <int> visited;
        pthread_mutex_lock(&mutex_tags);
        for (unsigned int i = 0; i < tag.size(); i++){
            cout << "[log] quantidade de clientes com a tag " << tag[i] << " eh: " << tags[tag[i]].size() << "\n";
            for (unsigned int j = 0; j < tags[tag[i]].size(); j++){
                if (tags[tag[i]][j] != id){
                    if (visited.count(tags[tag[i]][j]) == 0){
                        pthread_mutex_lock(&mutex_id);
                        int csock = id_to_csock[tags[tag[i]][j]];
                        pthread_mutex_unlock(&mutex_id);
                        manda(csock, buf);
                        visited.insert(tags[tag[i]][j]);
                    }
                }
            }
        }
        pthread_mutex_unlock(&mutex_tags);
}

string recebe(int id, int csock){
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    int total = 0;
    size_t count;
    string msg_completa;
     
    while(1){
        count = recv(csock, buf+total, BUFSZ - total, 0);
        if (count == 0){
            for (unsigned int i = 0; i < actives_sockets.size(); i++){
                if (actives_sockets[i] == csock)
                    actives_sockets.erase(actives_sockets.begin() + i);
            }
            return "kill";
        }
        msg_completa += buf+total;
        total += count;
        if (msg_completa[msg_completa.size()-1] == '\n'){
            break;
        }            
        if (total == BUFSZ){
            break;
        }
    }
    return msg_completa;
}

vector <string> separa_mensagens(string msg_completa){
    vector <string> messages;
    char msg[BUFSZ];
    memset(msg, 0, BUFSZ);
    int j = 0;
    for (unsigned int i = 0; i < msg_completa.size(); i++){
        if (msg_completa[i] == '\n'){
            msg[j] = msg_completa[i];
            messages.push_back(msg);
            memset(msg, 0, BUFSZ);
            j = 0;
        }
        else{
            msg[j] = msg_completa[i];
            j++;
        }
    } 
    return messages;
}   
    

void * client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr);
    printf("[log] conectou no socket: %d\n", cdata->csock);
    printf("[log] ID: %d\n", cdata->id);

    pthread_mutex_lock(&mutex_id);
    id_to_csock[cdata->id] = cdata->csock;
    actives_sockets.push_back(cdata->csock);
    pthread_mutex_unlock(&mutex_id);

    
    while(1){
        string msg = recebe(cdata->id, cdata->csock);
        printf("[log] mensagem recebida: %s\n", msg.c_str());
        if (strcmp(msg.c_str(), "kill") == 0){
            break;
        }
        vector <string> msgs = separa_mensagens(msg);
        for (unsigned int i = 0; i < msgs.size(); i++){
            msg = msgs[i];
            if (strcmp(msg.c_str(), "##kill\n") == 0){
                for (unsigned int i = 0; i < actives_sockets.size(); i++){
                    close(actives_sockets[i]);
                }
                exit(0);
            } 
            else if (msg[0] == '+' and msg[msg.size()-1] == '\n'){
                msg.erase(0, 1);
                msg.erase(msg.end()-1);
                manda(cdata->csock, adiciona_tag(msg, cdata->id));    
            }

            else if (msg[0] == '-' and msg[msg.size()-1] == '\n'){
                msg.erase(0, 1);
                msg.erase(msg.end()-1);
                manda(cdata->csock, deleta_tag(msg, cdata->id));
            }

            else {
                encaminha_msg(msg, cdata->id);
            }
        }   
        
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

    pthread_mutex_init(&mutex_id, NULL);
    pthread_mutex_init(&mutex_tags, NULL);

    int id = 0; 

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
        cdata->id = id;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata);
        id++;
    }

    exit(EXIT_SUCCESS);
}
