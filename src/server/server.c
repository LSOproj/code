#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define MAX_ACCEPTED_CLIENT 50

void error_handler(char *message);
void* connection_handler(void* arg);

int main(){
	
	int server_socket, client_socket;
	struct sockaddr_un server_address;
	socklen_t server_address_len = sizeof(server_address);

	server_address.sun_family = AF_LOCAL;
	strcpy(server_address.sun_path, "/tmp/server_socket");

	if((server_socket = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0)
		error_handler("[SERVER] Errore creazione socket");

	printf("\n[SERVER] Successo socket create.\n");

	unlink(server_address.sun_path);
	if(bind(server_socket, (struct sockaddr *) &server_address, server_address_len) < 0)
		error_handler("[SERVER] Errore bind socket");

	printf("\n[SERVER] Successo socket bind.\n");

	if(listen(server_socket, MAX_ACCEPTED_CLIENT) < 0)
		error_handler("[SERVER] Errore listen socket");

	printf("\n[SERVER] Successo socket listen.\n");

	while(1){

		int* client_socket = (int*)malloc(sizeof(int));

		if(client_socket == NULL)
			error_handler("[SERVER] Errore allocazione dinamica");

		if((*client_socket = accept(server_socket, (struct sockaddr *) &server_address, &server_address_len)) < 0)
			error_handler("[SERVER] Errore accept socket");

		printf("\n[SERVER] Ricevuta connessione di un client.\n");

		pthread_t tid;
		if(pthread_create(&tid, NULL, connection_handler, (void *)client_socket) < 0){
			free(client_socket);
			close(*client_socket);
			error_handler("[SERVER] Errore creazione thread");
		}

	}

	close(server_socket);
	unlink(server_address.sun_path);

	return 0;
}

void* connection_handler(void* client_socket_arg){
	
	printf("\n[SERVER] Assegnato il thread %d al client.\n", (int)pthread_self());
	
	pthread_detach((int)pthread_self());

	int client_socket = *(int*)client_socket_arg;
	free(client_socket_arg);

	//codice da rimuovere
	char message[100] = "\nBenvenuto nel server!\0\n";
	ssize_t bytes_read;

	if((bytes_read = write(client_socket, message, 100)) < 0)
		error_handler("[SERVER] Errore write");

	close(client_socket);

	pthread_exit(NULL);
}

void error_handler(char *message){
	printf("\n%s.\n", message);
	exit(-1);
}
