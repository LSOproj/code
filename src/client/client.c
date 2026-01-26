#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <fcntl.h>


int main(){

	// sd(socket descriptor), socket su cui il client riceve
	// messaggi da parte del server.
	int client_socket; 
	struct sockaddr_un server_address;
	socklen_t server_address_len = sizeof(server_address);

	server_address.sun_family = AF_LOCAL;
	// /tmp/sever_socket socket locale del server a cui
	// il client si connette
	strcpy(server_address.sun_path, "/tmp/server_socket");

	if((client_socket = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0){
		perror("[CLIENT] Impossibile aprire la socket!\n");
		exit(-1);
	}

	if(connect(client_socket, (struct sockaddr *) &server_address, server_address_len) < 0){
		perror("[CLIENT] Impossibile connettersi al server.\n");
		exit(-1);
	}

	printf("[CLIENT] Client connesso con succeso al server!\n");

	if(close(client_socket) < 0 ){
		perror("[CLIENT] Impossibile chiudere la socket!\n");
		exit(-1);
	}
	
	return 0;
}
