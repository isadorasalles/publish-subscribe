all:
	g++ -Wall -c common.cpp
	g++ -Wall client.cpp common.o -lpthread -o client
	g++ -Wall server.cpp common.o -lpthread -o servidor

clean:
	rm common.o client server
