#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "client_protocol.h"
#include "client_logic.h"

void get_user_id(int client_socket){

	if(read(client_socket, &user_id, sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibilile leggere lo USER id\n");
		exit(-1);
	}

	printf("[CLIENT] Assegnato USER id %u\n", user_id);

	/*
	pid_t client_pid = getpid();
	if(write(client_socket, &client_pid, sizeof(client_pid)) < 0){
		printf("[CLIENT] Impossibile inviare pid %d del processo al server.\n", client_pid);
		exit(-1);
	}

	printf("[CLIENT] Inviato pid %d al server.\n", client_pid);
	*/
}

int check_server_response(int client_socket){

	char response[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

	if((read(client_socket, response, PROTOCOL_MESSAGE_MAX_SIZE)) < 0){
		perror("[CLIENT] Impossibile leggere il messaggio in arrivo\n");
		exit(-1);
	}

	if (strncmp(response, SUCCESS_REGISTER, strlen(SUCCESS_REGISTER)) == 0){

		printf("[CLIENT] Registrazione avvenuta con successo!\n");

		return 0;

	}else if (strncmp(response, SUCCESS_LOGIN, strlen(SUCCESS_LOGIN)) == 0) {

		printf("[CLIENT] Login avvenuta con successo!\n");
		get_user_id(client_socket);

		return 0;

	} else if (strncmp(response, FAILED_USER_ALREDY_EXISTS, strlen(FAILED_USER_ALREDY_EXISTS)) == 0){

		printf("[CLIENT] L'username specificato già appartiene ad un altro utente!\n");
		return -1;

	}else if (strncmp(response, FAILED_USER_DOESNT_EXISTS, strlen(FAILED_USER_DOESNT_EXISTS)) == 0){

		printf("[CLIENT] L'username specificato non appartiene a nessun utente!\n");
		return -1;

	}else if (strncmp(response, FAILED_USER_BAD_CREDENTIALS, strlen(FAILED_USER_BAD_CREDENTIALS)) == 0){


		printf("[CLIENT] Credenziali errate!\n");
		return -1;

	}

	return 0;

}
