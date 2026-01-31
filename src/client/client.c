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

typedef struct cart_t {

	int film_id_to_rent[MAX_FILMS];
	int dim; //capped by seller

}cart_t;

int num_films_avaible;
int user_id;
user_t user;
film_t avaible_films[MAX_FILMS];
cart_t cart;

//MOMENTANEO, QUI SOLO PER TESTING
//IL VALORE VERO SI PRENDE DAL SERVER
//====================================================================================================================================
int cart_cap = 5;
//====================================================================================================================================

// Function prototypes
int start_up_menu(void);
void get_user_id(int client_socket);
int check_server_response(int client_socket);
void register_user(int client_socket);
void login_user(int client_socket);
void get_all_films(int client_socket);
void rental_menu(int client_socket);
void print_films(void);
void print_cart(void);
void init_cart(void);
void add_to_cart(int movie_id);
void remove_from_cart(void);
int get_movie_idx_by_id(int movie_id);
void parse_film_ids(char *input, int *film_ids, int *count);

void clear_screen(){
#ifdef _WIN32
	system("cls");
#else
	system("clear");
#endif
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

	//system("clear");

	while(1){	
		clear_screen();
		switch(start_up_menu()){
			case 1:
				register_user(client_socket);
				break;
			case 2:
				login_user(client_socket);
				break;
			case 0:
				printf("Arrivederci\n");
				exit(0);
				break;
			default:
				printf("Scelta non valida, ritentare.\n");
				sleep(2);
		}
	}

	return 0;
}

int start_up_menu(void){
	printf("1 - Register\n2 - Login\n0 - Exit\n");
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

void register_user(int client_socket){
	clear_screen();

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

	check_server_response(client_socket);
	sleep(2);
	return;
}

void login_user(int client_socket){
	clear_screen();

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

	if(check_server_response(client_socket) < 0){
		sleep(2);
		return;
	}

	init_cart();
	get_all_films(client_socket);
	rental_menu(client_socket);
}

void print_films(void){
	printf("%-3s %-30s %15s %15s\n",
			"ID", "Title", "Available", "Rented");
	printf("-----------------------------------------------------------------------\n");
	for(int i = 0; i < num_films_avaible; i++){
		printf("%-3d %-30s %15d %15d\n",
				avaible_films[i].id,
				avaible_films[i].title,
				avaible_films[i].available_copies,
				avaible_films[i].rented_out_copies);
	}
}

void print_cart(void){
	if(cart.dim == 0){
		printf("Il carrello e' vuoto.\n");
		return;
	}
	printf("%-3s %-30s\n", "ID", "Title");
	printf("-----------------------------------------------------------------------\n");
	for(int i = 0; i < cart.dim; i++){
		int idx = get_movie_idx_by_id(cart.film_id_to_rent[i]);
		if(idx >= 0){
			printf("%-3d %-30s\n", avaible_films[idx].id, avaible_films[idx].title);
		}
	}
	printf("Numero film nel carrello: %d/%d\n", cart.dim, cart_cap);
}

void get_all_films(int client_socket){
	char get_films_protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(get_films_protocol_command, GET_FILMS_PROTOCOL_MESSAGE);

	if(write(client_socket, get_films_protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile mandare il messaggio di protocollo\n");
		exit(-1);
	}

	char response[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

	if(read(client_socket, response, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		perror("[CLIENT] Impossibile leggere il messaggio in arrivo\n");
		exit(-1);
	}

	if(read(client_socket, &num_films_avaible, sizeof(num_films_avaible)) < 0){
		printf("[CLIENT] Errore nella ricezione del numero in arrico film\n");
		exit(-1);
	}

	for(int i = 0; i < num_films_avaible; i++){
		if(read(client_socket, &avaible_films[i].id, sizeof(avaible_films[i].id)) < 0){
			printf("[CLIENT] Errore nella ricezione del film id\n");
			exit(-1);
		}
		if(read(client_socket, avaible_films[i].title, MAX_FILM_TITLE_SIZE) < 0){
			printf("[CLIENT] Errore nella ricezione del titolo film\n");
			exit(-1);
		}
		if(read(client_socket, &avaible_films[i].available_copies, sizeof(avaible_films[i].available_copies)) < 0){
			printf("[CLIENT] Errore nella ricezione del numero di copie disponibile\n");
			exit(-1);
		}
		if(read(client_socket, &avaible_films[i].rented_out_copies, sizeof(avaible_films[i].rented_out_copies)) < 0){
			printf("[CLIENT] Errore nella ricezione del numero di copie affitate\n");
			exit(-1);
		}
	}

	

	//print_films();

	//rent_movie(client_socket);
}

/* Struttura carrello:
 * - array di int contente gli id dei film da affittare
 * - intero che indica la dimensione del carrello
 */

int get_movie_idx_by_id(int movie_id){
	int i = 0;
	while(i < num_films_avaible){
		if((avaible_films[i].id) == movie_id)
			return i;
		i++;
	}

	return -1;
}

int renting_menu(){
	printf("\n=== MENU NOLEGGIO ===\n");
	printf("1 - Inserisci ID film da noleggiare (separati da virgola, max 5)\n");
	printf("2 - Mostra lista film\n");
	printf("3 - Visualizza carrello\n");
	printf("4 - Logout\n");
	int choice = -1;
	printf("Inserire un numero per proseguire: ");
	scanf("%d", &choice);
	return choice;
}

void parse_film_ids(char *input, int *film_ids, int *count){
	*count = 0;
	char temp[10];
	int temp_idx = 0;
	
	for(int i = 0; input[i] != '\0' && *count < 5; i++){
		if(input[i] == ',' || input[i] == ' '){
			if(temp_idx > 0){
				temp[temp_idx] = '\0';
				film_ids[*count] = atoi(temp);
				(*count)++;
				temp_idx = 0;
			}
		} else {
			temp[temp_idx++] = input[i];
		}
	}
	if(temp_idx > 0){
		temp[temp_idx] = '\0';
		film_ids[*count] = atoi(temp);
		(*count)++;
	}
}

void rental_menu(int client_socket){
	int choice;
	int first_time = 1;
	
	while(1){
		clear_screen();
		
		if(first_time){
			print_films();
			first_time = 0;
		}
		
		choice = renting_menu();
		
		switch(choice){
			case 1: {
				// Inserire film da noleggiare
				clear_screen();
				print_films();
				
				printf("\nInserire gli ID dei film da noleggiare (separati da virgola):\n");
				printf("Esempio: 1,3,5 oppure 2, 4, 7\n");
				printf("ID film: ");
				
				char film_input[100] = {0};
				scanf(" %99[^\n]", film_input);
				getchar(); // Consumare il newline rimasto
				
				int film_ids[5] = {0};
				int count = 0;
				parse_film_ids(film_input, film_ids, &count);
				
				if(count == 0){
					printf("Nessun ID inserito.\n");
					sleep(1);
					break;
				}
				
				for(int i = 0; i < count; i++){
					int idx = get_movie_idx_by_id(film_ids[i]);
					if(idx < 0){
						printf("ID %d non valido.\n", film_ids[i]);
					} else if(cart.dim >= cart_cap){
						printf("Carrello pieno! Non è possibile aggiungere altri film.\n");
						break;
					} else {
						add_to_cart(film_ids[i]);
						printf("Film '%s' aggiunto al carrello.\n", avaible_films[idx].title);
					}
				}
				sleep(2);
				break;
			}
			case 2: {
				// Mostra lista film
				clear_screen();
				print_films();
				printf("\nPremere INVIO per continuare...");
				getchar(); // Consumare il newline rimasto
				getchar();
				break;
			}
			case 3: {
				// Visualizza carrello
				clear_screen();
				printf("=== CARRELLO ===\n");
				print_cart();
				printf("\nPremere INVIO per continuare...\n");
				getchar(); // Consumare il newline rimasto
				getchar();
				break;
			}
			case 4: {
				// Logout
				printf("Logout in corso...\n");
				cart.dim = 0;
				return;
			}
			default:
				printf("Scelta non valida.\n");
				sleep(2);
		}
	}
}

void init_cart(void){
	//teoricamente ogni volta al boot up 
	//si salva il cap imposto dal venditore
	cart.dim = 0;
}

void add_to_cart(int movie_id){
	if(cart.dim >= cart_cap)
		printf("Impossibile aggiungere  al carrello, limite raggiunto.");
	else{
		cart.film_id_to_rent[cart.dim] = movie_id;
		cart.dim++;
	}
}

void remove_from_cart(void){
	if(cart.dim <= 0){
		printf("Il Carrello e' vuoto, nulla da rimuovere.\n");
		return;
	}

	int movie_id;
	printf("Inserire id del film da rimuovere: ");
	scanf("%d", &movie_id);

	int	movie_to_remove = get_movie_idx_by_id(movie_id);

	if(movie_to_remove < 0)
		printf("Non c'e' nessuno carrelo con id %d\n", movie_to_remove);
	else{
		for(int i = movie_to_remove; i < cart.dim-1; i++)
			cart.film_id_to_rent[i] = cart.film_id_to_rent[i+1];
		cart.film_id_to_rent[cart.dim] = 0;
		cart.dim--;
	}
}
