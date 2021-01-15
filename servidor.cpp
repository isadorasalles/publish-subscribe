#include "common.hpp"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>   
#include <vector>
#include <string>
#include <set>
#include <map>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 500

using namespace std; 

// estrutura para armazenar as tags e os clientes
map<string, vector<int> > tags;
pthread_mutex_t mutex_tags;

// estrutura para armazenar clientes ativos
vector<int> actives_sockets;
pthread_mutex_t mutex_actives_socks;

struct client_data {
    int csock;
    struct sockaddr_storage storage;
};

void usage(int argc, char **argv) {
    printf("usage: %s <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

// funcao para mandar a mensagem pro cliente
void manda(int csock, string msg){
    printf("[log] mensagem a ser enviada: %s", msg.c_str());
    size_t count = send(csock, msg.c_str(), strlen(msg.c_str()), 0);
    if (count != strlen(msg.c_str())) {
        logexit("send");
    }
}

// funcao para adicionar cliente a uma tag
string adiciona_tag(string msg, int csock){
    pthread_mutex_lock(&mutex_tags);
    vector <int>::iterator it;
    it = find(tags[msg].begin(), tags[msg].end(), csock);
    // verifica se cliente ja esta inscrito na tag
    if (it != tags[msg].end()){                 
        pthread_mutex_unlock(&mutex_tags);
        return "already subscribed +"+msg+"\n";
    }
    // se o cliente nao se inscreveu ainda, inscreve
    tags[msg].push_back(csock);
    pthread_mutex_unlock(&mutex_tags);
    return "subscribed +"+msg+"\n";
}

// funcao para deletar cliente de uma tag
string deleta_tag(string msg, int csock){
    pthread_mutex_lock(&mutex_tags);
    vector <int>::iterator it;
    it = find(tags[msg].begin(), tags[msg].end(), csock);
    // verifica se cliente esta na tag e remove se estiver
    if (it != tags[msg].end()){
        tags[msg].erase(it);
        pthread_mutex_unlock(&mutex_tags);
        return "unsubscribed -"+msg+"\n";
    }
    pthread_mutex_unlock(&mutex_tags);
    return "not subscribed -"+msg+"\n";
}

// funcao que repassa mensagens aos clientes inscritos nas tags
void encaminha_msg(string buf, int csock){
    vector <string> tag;
    // busca pelas tags existentes na mensagem e coloca num vetor de strings
    for (unsigned int i = 0; i < buf.size(); i++){
        if(buf[i] == '#' and (i == 0 or buf[i-1] == ' ')){
            vector <char> aux;
            unsigned int j;
            for (j = i+1; j < buf.size(); j++){
                if(buf[j] == ' ' or buf[j] == '\n') break;
                aux.push_back(buf[j]);
            }
            string t(aux.begin(), aux.end());
            tag.push_back(t);
            i = j;
        }
    }
    set <int> visited;
    pthread_mutex_lock(&mutex_tags);
    // busca por todos os clientes inscritos nas tags que aparecem na mensagem 
    // e repassa a mensagem para eles apenas uma vez
    for (unsigned int i = 0; i < tag.size(); i++){
        cout << "[log] quantidade de clientes com a tag " << tag[i] << " eh: " << tags[tag[i]].size() << "\n";
        for (unsigned int j = 0; j < tags[tag[i]].size(); j++){
            if (tags[tag[i]][j] != csock){
                if (visited.count(tags[tag[i]][j]) == 0){
                    manda(tags[tag[i]][j], buf);
                    visited.insert(tags[tag[i]][j]);
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex_tags);
}

// funcao que recebe a mensagem
string recebe(int csock, string save){
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    // copia o resto da mensagem anterior que nao tinha \n no fim
    // para esperar que ela seja completada agora
    for (unsigned int i = 0; i <= save.size(); i++)
        buf[i] = save[i];
    int total = save.size();
    size_t count;
    string msg_completa = save;

    // loop para receber mensagens particionadas
    while(1){
        count = recv(csock, buf+total, BUFSZ - total, 0);
        if (count == 0){
            // conexao terminou! 
            map<string, vector<int> >::iterator i;
            vector <int>::iterator it;
            pthread_mutex_lock(&mutex_tags);
            // excluir o cliente da estrutura de tags
            for(i = tags.begin(); i != tags.end(); i++){
                it = find(i->second.begin(), i->second.end(), csock);
                if (it != i->second.end())
                    i->second.erase(it);
            }
            pthread_mutex_unlock(&mutex_tags);
            pthread_mutex_lock(&mutex_actives_socks);
            // excluir o cliente da estrutura de sockets ativos
            for (unsigned int i = 0; i < actives_sockets.size(); i++){
                if (actives_sockets[i] == csock)
                    actives_sockets.erase(actives_sockets.begin() + i);
            }
            pthread_mutex_unlock(&mutex_actives_socks);
            return "cliente desconectou\n";
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

// funcao usada para separar multiplas mensagens recebidas no mesmo recv
vector <string> separa_mensagens(string msg_completa, string &buf){
    vector <string> messages;
    char msg[BUFSZ];
    memset(msg, 0, BUFSZ);
    int j = 0;
    for (unsigned int i = 0; i < msg_completa.size(); i++){
        // cada \n representa o fim de uma mensagem
        if (msg_completa[i] == '\n'){
            msg[j] = msg_completa[i];
            messages.push_back(msg);
            memset(msg, 0, BUFSZ);
            j = 0;
        }
        else if (i == msg_completa.size()-1){
            msg[j] = msg_completa[i];
            buf = msg;
        }
        else{
            msg[j] = msg_completa[i];
            j++;
        }
    } 
    return messages;
}   

// funcao para verificar caracteres invalidos
bool checa_caracteres_validos(string msg){
    for (unsigned int i = 0; i < msg.size(); i++){
        if (msg[i] != 32 and msg[i] != 33 and !(msg[i] >= 35 and msg[i] <= 37) and msg[i] != '\n' and 
            !(msg[i] >= 40 and msg[i] <= 59) and msg[i] != 61 and !(msg[i] >= 63 and msg[i] <= 91) and
            msg[i] != 93 and !(msg[i] >= 97 and msg[i] <=123) and msg[i] != 125)
                return false;
            
    }
    return true;
}

// funcao para verificar se tem apenas letras na tag
bool checa_tag(string msg){
    for (unsigned int i = 0; i < msg.size(); i++){
        if (!(msg[i] >= 65 and msg[i] <= 90) and !(msg[i] >= 97 and msg[i] <= 122)){
            return false;
        }
    }
    return true;
}
    

void * client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("\n[log] conetou com: %s\n", caddrstr);
    printf("[log] conectou no socket: %d\n", cdata->csock);

    pthread_mutex_lock(&mutex_actives_socks);
    actives_sockets.push_back(cdata->csock);
    pthread_mutex_unlock(&mutex_actives_socks);

    string save;
    // loop para receber mensagens enquanto o cliente estiver conectado
    while(1){
        string msg = recebe(cdata->csock, save);
        save = "";
        printf("[log] mensagem recebida: %s", msg.c_str());
        if (strcmp(msg.c_str(), "cliente desconectou\n") == 0)
            break;
        
        if (!checa_caracteres_validos(msg))
            break;
        
        vector <string> msgs = separa_mensagens(msg, save);

        // loop entre as mensagens recebidas
        for (unsigned int i = 0; i < msgs.size(); i++){
            msg = msgs[i];
            // se a mensagem for ##kill fecha todos os clientes e o servidor
            if (strcmp(msg.c_str(), "##kill\n") == 0){
                pthread_mutex_lock(&mutex_actives_socks);
                for (unsigned int i = 0; i < actives_sockets.size(); i++){
                    close(actives_sockets[i]);
                }
                pthread_mutex_unlock(&mutex_actives_socks);
                exit(0);
            } 
            // avalia se eh uma mensagem de inscricao em tag
            else if (msg[0] == '+' and msg[msg.size()-1] == '\n'){
                msg.erase(0, 1);
                msg.erase(msg.end()-1);
                if (!checa_tag(msg)) continue;
                manda(cdata->csock, adiciona_tag(msg, cdata->csock));    
            }
            // avalia se eh uma mensagem para remover tag
            else if (msg[0] == '-' and msg[msg.size()-1] == '\n'){
                msg.erase(0, 1);
                msg.erase(msg.end()-1);
                if (!checa_tag(msg)) continue;
                manda(cdata->csock, deleta_tag(msg, cdata->csock));
            }
            // se nao for nenhuma das anteriores avalia se tem que repassar
            else {
                encaminha_msg(msg, cdata->csock);
            }
        }   
        
    }
    close(cdata->csock);

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argc, argv);
    }

    const char *proto = "v4";

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(proto, argv[1], &storage)) {
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

    pthread_mutex_init(&mutex_actives_socks, NULL);
    pthread_mutex_init(&mutex_tags, NULL);

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
    }

    exit(EXIT_SUCCESS);
}
