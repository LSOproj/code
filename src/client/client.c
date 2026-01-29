#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define SUCCESS_REGISTER					"SUCCESS_REGISTER"
#define SUCCESS_LOGIN						"SUCCESS_LOGIN"
#define SUCCESS_GET_FILMS					"SUCCESS_GET_FILMS"
#define FAILED_USER_ALREDY_EXISTS			"FAILED_USER_ALREADY_EXISTS"
#define FAILED_USER_DOESNT_EXISTS			"FAILED_USER_DOESNT_EXISTS"
#define FAILED_USER_BAD_CREDENTIALS			"FAILED_USER_BAD_CREDENTIALS"
#define PROTOCOL_MESSAGE_MAX_SIZE			50

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

int user_id;
user_t user;
film_t avaible_films[MAX_FILMS];

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

void get_user_id(int client_socket){

	if(read(client_socket, &user_id, sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibilile leggere lo USER id\n");
		exit(-1);
	}

	printf("[CLIENT] Assegnato USER id %u\n", user_id);
}

void check_server_respose(int client_socket){

	char response[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

	if((read(client_socket, response, PROTOCOL_MESSAGE_MAX_SIZE)) < 0){
		perror("[CLIENT] Impossibile leggere il messaggio in arrivo\n");
		exit(-1);
	}

	if (strncmp(response, SUCCESS_REGISTER, strlen(SUCCESS_REGISTER)) == 0)

		printf("[CLIENT] Registrazione avvenuta con successo!\n");
	
	else if (strncmp(response, SUCCESS_LOGIN, strlen(SUCCESS_LOGIN)) == 0) {

		printf("[CLIENT] Login avvenuta con successo!\n");
		get_user_id(client_socket);
	
	} else if (strncmp(response, FAILED_USER_ALREDY_EXISTS, strlen(FAILED_USER_ALREDY_EXISTS)) == 0)

		printf("[CLIENT] L'username specificato già appartiene ad un altro utente!\n");

	else if (strncmp(response, FAILED_USER_DOESNT_EXISTS, strlen(FAILED_USER_DOESNT_EXISTS)) == 0)

		printf("[CLIENT] L'username specificato non appartiene a nessun utente!\n");

	else if (strncmp(response, FAILED_USER_BAD_CREDENTIALS, strlen(FAILED_USER_BAD_CREDENTIALS)) == 0)

		printf("[CLIENT] Credenziali errate!\n");
	
}

void register_user(int client_socket){

	system("clear");

	char username[MAX_USER_USERNAME_SIZE] = {0};
	printf("Inserire il username che si desidera usare: ");
	scanf("%s", username);

	char password[MAX_USER_PASSWORD_SIZE] = {0};
	printf("Inserire la password che si desidera usare: ");
	scanf("%s", password);

	char register_protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(register_protocol_command, REGISTER_PROTOCOL_MESSAGE);

	if(write(client_socket, register_protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile mandare il messaggio di protocollo %s\n", register_protocol_command);
		exit(-1);
	}

	if(write(client_socket, username, MAX_USER_USERNAME_SIZE) < 0){
		printf("[CLIENT] Impossibilile mandare il username\n");
		exit(-1);
	}

	if(write(client_socket, password, MAX_USER_PASSWORD_SIZE) < 0){
		printf("[CLIENT] Impossibilile mandare la password\n");
		exit(-1);
	}

	/*
	char response[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	if((read(client_socket, response, PROTOCOL_MESSAGE_MAX_SIZE)) < 0){
		perror("[CLIENT] Impossibile leggere il messaggio in arrivo\n");
		exit(-1);
	}

	if(strncmp(response, SUCCESS_SERVER_RESPONSE, strlen(SUCCESS_SERVER_RESPONSE)) == 0)
		printf("[CLIENT] Registrazione avvenuta con successo!\n");
	*/

	check_server_respose(client_socket);
}

void login_user(int client_socket){
	system("clear");

	char username[MAX_USER_USERNAME_SIZE] = {0};
	printf("Inserisci username: ");
	scanf("%s", username);


	char password[MAX_USER_USERNAME_SIZE] = {0};
	printf("Inserisci password: ");
	scanf("%s", password);

	char login_protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(login_protocol_command, LOGIN_PROTOCOL_MESSAGE);

	if(write(client_socket, login_protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile mandare il messaggio di protocollo %s\n", login_protocol_command);
		exit(-1);
	}

	if(write(client_socket, username, MAX_USER_USERNAME_SIZE) < 0){
		printf("[CLIENT] Impossibilile mandare il username\n");
		exit(-1);
	}

	if(write(client_socket, password, MAX_USER_PASSWORD_SIZE) < 0){
		printf("[CLIENT] Impossibilile mandare la password\n");
		exit(-1);
	}

	check_server_respose(client_socket);

}

void main_menu(int client_socket){
	system("clear");

	char get_films_protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(get_films_protocol_command, GET_FILMS_PROTOCOL_MESSAGE);

	if(write(client_socket, get_films_protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile mandare il messaggio di protocollo: %s\n", get_films_protocol_command);
		exit(-1);
	}

	int num_films;
	if(read(client_socket, &num_films, sizeof(num_films)) < 0){
		printf("[CLIENT] Errore nella ricezione del numero in arrico film\n");
		exit(-1);
	}
	
	int momentary_film_id;
	char momentary_film_title[MAX_FILM_TITLE_SIZE];
	int momentary_film_available_copies;
	int momentary_film_rented_out_copies;
	int i = 0;
	while(i < num_films){
		if(read(client_socket, &momentary_film_id, sizeof(momentary_film_id)) < 0){
			printf("[CLIENT] Errore nella ricezione del film id\n");
			exit(-1);
		}
		if(read(client_socket, momentary_film_title, MAX_FILM_TITLE_SIZE) < 0){
			printf("[CLIENT] Errore nella ricezione del titolo film\n");
			exit(-1);
		}
		if(read(client_socket, &momentary_film_available_copies, sizeof(momentary_film_available_copies)) < 0){
			printf("[CLIENT] Errore nella ricezione del numero di copie disponibile del film\n");
			exit(-1);
		}
		if(read(client_socket, &momentary_film_rented_out_copies, sizeof(momentary_film_rented_out_copies)) < 0){
			printf("[CLIENT] Errore nella ricezione del numero di copie affitate del film\n");
			exit(-1);
		}

		avaible_films[i].id = momentary_film_id;
		strcpy(avaible_films[i].title, momentary_film_title);
		avaible_films[i].available_copies = momentary_film_available_copies;
		avaible_films[i].rented_out_copies = momentary_film_rented_out_copies;

		i++;
	}

	for(int i = 0; i < num_films; i++){
		printf("%d %10s %10d %10d", 
					avaible_films[i].id,
					avaible_films[i].title,
					avaible_films[i].available_copies,
					avaible_films[i].rented_out_copies);
	}
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
				login_user(client_socket);
				break;
			case 3:
				main_menu(client_socket);
				break;
			case 0:
				printf("Arrivederci\n");
				exit(0);
				break;
			default:
				printf("Scelta non valida, ritentare.");
				sleep(2);
		}
	}

	return 0;
}
