#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

// Macro utili per i per gli array di caratteri
#define MAX_USER_USERNAME_SIZE	100
#define MAX_USER_PASSWORD_SIZE	100
#define MAX_FILM_TITLE_SIZE		100
#define MAX_FILMS				100

// Macro per il text protocol
#define REGISTER_PROTOCOL_MESSAGE 			"REGISTER"
#define LOGIN_PROTOCOL_MESSAGE 				"LOGIN"
#define GET_FILMS_PROTOCOL_MESSAGE  		"GET_FILMS"
#define RENT_FILM_PROTOCOL_MESSAGE  		"RENT_FILM"
#define RETURN_RENTED_FILM_PROTOCOL_MESSAGE	"RETURN_RENTED_FILM"
#define SUCCESS_SERVER_RESPONSE				"SUCCESS"
#define FAILED_SERVER_RESPONSE				"FAILED"
#define PROTOCOL_MESSAGE_MAX_SIZE			20

typedef struct user_t {

	int id;
	char username[MAX_USER_USERNAME_SIZE];
	char password[MAX_USER_PASSWORD_SIZE];

} user_t;

typedef struct film_t {

	int id;
	char title[MAX_FILM_TITLE_SIZE];
	int available_copies;
	int rented_out_copies;

} film_t;

user_t user;
film_t avaible_films[MAX_FILMS];

typedef enum PROTOCOL_MESSAGE {
	
	REGISTER,
	LOGIN,
	GET_FILMS,
	RENT_FILM,
	RUTERN_RENTED_FILM,
	CHECKOUT

} protocol_message;

int start_up_menu(void){
	printf(
			"1 - Register\n"
			"2 - Login\n"
			"0 - Exit\n"
			);
	int choice = -1;

	printf("Inserire un numero per proseguire: ");
	scanf("%d", &choice);

	return choice;
}

void register_user(int client_socket){

	system("clear");

	char username[MAX_USER_USERNAME_SIZE] = {0};
	printf("Inserire il username che si desidera usare: ");
	scanf("%s", username);

	char password[MAX_USER_PASSWORD_SIZE] = {0};
	printf("Inserire la password che si desidera usare: ");
	scanf("%s", password);

	char protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(protocol_command, REGISTER_PROTOCOL_MESSAGE);

	if(write(client_socket, protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("Impossibilile mandare il messaggio di protocollo\n");
		exit(-1);
	}

	if(write(client_socket, username, MAX_USER_USERNAME_SIZE) < 0){
		printf("Impossibilile mandare il username\n");
		exit(-1);
	}

	if(write(client_socket, password, MAX_USER_PASSWORD_SIZE) < 0){
		printf("Impossibilile mandare la password\n");
		exit(-1);
	}

	char response[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	if((read(client_socket, response, PROTOCOL_MESSAGE_MAX_SIZE)) < 0){
		perror("[CLIENT] Impossibile leggere il messaggio in arrivo\n");
		exit(-1);
	}

	if(strncmp(response, SUCCESS_SERVER_RESPONSE, strlen(SUCCESS_SERVER_RESPONSE)) == 0)
		printf("Registrazione avvenuta con successo!\n");
}

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

	system("clear");
	
	while(1){	
		switch(start_up_menu()){
			case 1:
				register_user(client_socket);
				break;
			case 2:
				printf("funzione login");
				// login();
				break;
			case 0:
				printf("Arrivederci");
				exit(0);
				break;
			defualt:
				printf("Scelta non valida, ritentare.");
				sleep(2);
		}
	}

	return 0;
}
