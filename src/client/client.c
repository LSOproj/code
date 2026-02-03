#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <signal.h>
#include "client_protocol.h"
#include "client_logic.h"

// Global variables definition
unsigned int user_id;
int client_socket;
cart_t cart;

int film_reminder = 0;

int num_films_avaible;
film_t avaible_films[MAX_FILMS];

int num_expired_films;
film_t expired_films[MAX_FILMS];

int num_rented_films;
film_t rented_films[MAX_FILMS];

//MOMENTANEO, QUI SOLO PER TESTING
//====================================================================================================================================
int cart_cap = 5;

// Function prototypes
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
void expired_films_signal_handler(int signum);
void get_all_user_expired_films_with_no_due_date(int client_socket);
void shopkeeper_notify_expired_films(int client_socket);

int start_up_menu(void);
void register_user(int client_socket);
void login_user(int client_socket);
void get_all_films(int client_socket);
void rental_menu(int client_socket);
void print_films(void);
void print_cart(void);
int get_cart_count_by_id(int movie_id);

void shopkeeper_menu(int client_socket);
void set_cap_films(int client_socket);
void proceed_to_checkout(int client_socket);
void rent_film(int client_socket, int idx);
void get_user_rented_films(int client_socket);
void check_rented_films();
void renturn_rented_film(int client_socket);

void clear_screen(){
#ifdef _WIN32
	system("cls");
#else
	system("clear");
#endif
}

void show_main_view(void){
	clear_screen();
	print_films();
	printf("\n=== CARRELLO ATTUALE ===\n");
	print_cart();
}

int main(){

	// sd(socket descriptor), socket su cui il client riceve
	// messaggi da parte del server.
	//int client_socket; 
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

	pid_t client_pid = getpid();
	if(write(client_socket, &client_pid, sizeof(client_pid)) < 0){
		printf("[CLIENT] Impossibile inviare pid %d del processo al server.\n", client_pid);
		exit(-1);
	}

	printf("[CLIENT] Inviato pid %d al server.\n", client_pid);

	signal(SIGUSR1, expired_films_signal_handler);

	printf("[CLIENT] Registrata funzione signal handler per gestire i messaggi da parte del negoziante.\n");

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
	int result = scanf("%d", &choice);
	if(result != 1){
		int c;
		while((c = getchar()) != '\n' && c != EOF); // Consumare il resto della riga
		return -1; // Scelta non valida
	}
	return choice;
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

	char password[MAX_USER_PASSWORD_SIZE] = {0};
	printf("Inserisci password: ");
	scanf("%s", password);

	char login_protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(login_protocol_command, LOGIN_PROTOCOL_MESSAGE);

	if(write(client_socket, login_protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo %s\n", login_protocol_command);
		exit(-1);
	}

	if(write(client_socket, username, MAX_USER_USERNAME_SIZE) < 0){
		printf("[CLIENT] Impossibilile inviare il username\n");
		exit(-1);
	}

	if(write(client_socket, password, MAX_USER_PASSWORD_SIZE) < 0){
		printf("[CLIENT] Impossibilile inviare la password\n");
		exit(-1);
	}

	if(check_server_response(client_socket) < 0){
		sleep(2);
		return;
	}

	get_all_films(client_socket);

	if(strncmp(username, "negoziante", 10) == 0){
		shopkeeper_menu(client_socket);
	}else{
		init_cart();
		rental_menu(client_socket);
	}
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

void print_rented_films(void){
	printf("%-3s %-30s\n",
			"ID", "Title");
	printf("-----------------------------------------------------------------------\n");
	for(int i = 0; i < num_rented_films; i++){
		printf("%-3d %-30s\n",
				rented_films[i].id,
				rented_films[i].title);
	}
}

void print_cart(void){
	if(cart.dim == 0){
		printf("Il carrello e' vuoto.\n");
		return;
	}
	printf("%-3s %-30s %10s\n", "ID", "Title", "Qta");
	printf("-----------------------------------------------------------------------\n");
	int printed_ids[MAX_FILMS] = {0};
	int printed_count = 0;
	for(int i = 0; i < cart.dim; i++){
		int movie_id = cart.film_id_to_rent[i];
		int already_printed = 0;
		for(int j = 0; j < printed_count; j++){
			if(printed_ids[j] == movie_id){
				already_printed = 1;
				break;
			}
		}
		if(already_printed)
			continue;

		int idx = get_movie_idx_by_id(movie_id);
		int qty = get_cart_count_by_id(movie_id);
		if(idx >= 0){
			printf("%-3d %-30s %10d\n", avaible_films[idx].id, avaible_films[idx].title, qty);
		} else {
			printf("%-3d %-30s %10d\n", movie_id, "(sconosciuto)", qty);
		}
		printed_ids[printed_count++] = movie_id;
	}
	printf("Numero film nel carrello: %d/%d\n", cart.dim, cart_cap);
}

void get_all_films(int client_socket){

	char get_films_protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(get_films_protocol_command, GET_FILMS_PROTOCOL_MESSAGE);

	if(write(client_socket, get_films_protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo\n");
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

void get_user_rented_films(int client_socket){

	char get_user_rented_films_protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(get_user_rented_films_protocol_command, GET_USER_RENTED_FILMS_PROTOCOL_MESSAGE);

	if(write(client_socket, get_user_rented_films_protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo\n");
		exit(-1);
	}

	if(write(client_socket, &user_id, sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibile inviare lo USER id\n");
		exit(-1);
	}

	char response[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

	if(read(client_socket, response, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		perror("[CLIENT] Impossibile leggere il messaggio in arrivo\n");
		exit(-1);
	}

	if(read(client_socket, &num_rented_films, sizeof(num_rented_films)) < 0){
		printf("[CLIENT] Errore nella ricezione del numero in arrivo film\n");
		exit(-1);
	}

	for(int i = 0; i < num_rented_films; i++){

		if(read(client_socket, &rented_films[i].id, sizeof(rented_films[i].id)) < 0){
			printf("[CLIENT] Errore nella ricezione del film id\n");
			exit(-1);
		}

		if(read(client_socket, rented_films[i].title, MAX_FILM_TITLE_SIZE) < 0){
			printf("[CLIENT] Errore nella ricezione del titolo film\n");
			exit(-1);
		}

		/*
		if(read(client_socket, &rented_films[i].available_copies, sizeof(rented_films[i].available_copies)) < 0){
			printf("[CLIENT] Errore nella ricezione del numero di copie disponibile\n");
			exit(-1);
		}

		if(read(client_socket, &rented_films[i].rented_out_copies, sizeof(rented_films[i].rented_out_copies)) < 0){
			printf("[CLIENT] Errore nella ricezione del numero di copie affitate\n");
			exit(-1);
		}
		*/
	}

	//print_films();
}

int renting_menu(){
	printf("\n=== MENU NOLEGGIO ===\n");
	printf("1 - Inserisci ID film da noleggiare (separati da virgola, max 5)\n");
	printf("2 - Mostra lista film\n");
	printf("3 - Modifica carrello\n");
	printf("4 - Procedere al checkout\n");
	printf("5 - Stampa film noleggiati\n");
	printf("6 - Restituzione film noleggiato\n");
	printf("7 - Logout\n");
	int choice = -1;
	printf("Inserire un numero per proseguire: ");
	int result = scanf("%d", &choice);
	if(result != 1){ //fix se non viene inserito un numero
		int c;
		while((c = getchar()) != '\n' && c != EOF); // Consumare il resto della riga
		return -1; // Scelta non valida
	}
	return choice;
}

void rental_menu(int client_socket){
	int choice;
	int films_shown = 0; // Indica se i film sono attualmente visualizzati

	show_main_view();
	films_shown = 1;
	
	while(1){

		get_all_films(client_socket);
		get_user_rented_films(client_socket);
		//GET_MAX_RENTED_FILMS_PROTOCOL_MESSAGE

		if(film_reminder){
			film_reminder = 0;
        	get_all_user_expired_films_with_no_due_date(client_socket);
			//FARE POI LA PRINT DEI FILMS SALVATI IN
			//int num_expired_films;
			//film_t expired_films[MAX_FILMS];
		}

		choice = renting_menu();

		
		switch(choice){
			case 1: {
				// Inserire film da noleggiare
				if(cart.dim >= cart_cap){
					printf("Carrello pieno! Non è possibile aggiungere altri film.\n");
					sleep(2);
					break;
				}
				if(!films_shown){
					show_main_view();
					films_shown = 1;
				}
				show_main_view();
				
				printf("\nInserire gli ID dei film da noleggiare (separati da virgola):\n");
				printf("Esempio: 1,3,5 oppure 2, 4, 7\n");
				printf("ID film: ");
				
				char film_input[100] = {0};
				scanf(" %99[^\n]", film_input);
				getchar(); // Consumare il newline rimasto
				
				if(film_input[0] == '\0'){
					printf("Nessun ID inserito.\n");
					sleep(1);
					break;
				}
				
				int film_ids[5];
				int count = 0;
				parse_film_ids(film_input, film_ids, &count);
				
				if(count > 0){
					for(int i = 0; i < count; i++){
						int idx = get_movie_idx_by_id(film_ids[i]);
						add_to_cart(film_ids[i]);
						printf("Film '%s' aggiunto al carrello.\n", avaible_films[idx].title);
					}
				}
				sleep(3);
				show_main_view();
				films_shown = 1;
				break;
			}
			case 2: {
				// Mostra lista film
				show_main_view();
				films_shown = 1;
				break;
			}
			case 3: {
				// Modifica carrello
				while(1){
					if(cart.dim == 0){
						printf("Il carrello e' vuoto.\n");
						sleep(1);
						break;
					}
					clear_screen();
					printf("=== MODIFICA CARRELLO ===\n");
					print_cart();
					printf("\n1 - Rimuovi film dal carrello\n");
					printf("2 - Svuota carrello\n");
					printf("0 - Torna al menu noleggio\n");
					int sub_choice = -1;
					printf("Scelta: ");
					scanf("%d", &sub_choice);
					getchar(); // Consumare il newline rimasto
					if(sub_choice == 1){
						if(cart.dim > 0){
							printf("Inserire gli ID separati da virgola (es: 1,3(2),5)\n");
							char report[512] = {0};
							remove_from_cart(report, sizeof(report));
							clear_screen();
							printf("=== CARRELLO AGGIORNATO ===\n");
							print_cart();
							if(report[0] != '\0'){
								printf("\nRimozioni:\n%s", report);
							}
						} else {
							printf("Il carrello e' vuoto.\n");
						}
						printf("Premere INVIO per continuare...\n");
						getchar();
					} else if(sub_choice == 2){
						cart.dim = 0;
						clear_screen();
						printf("Carrello svuotato.\n");
						sleep(1);
						break;
					} else if(sub_choice == 0){
						break;
					} else {
						printf("Scelta non valida.\n");
						sleep(1);
					}
				}
				show_main_view();
				films_shown = 1;
				break;
			}
			case 4: {
				proceed_to_checkout(client_socket);
				break;
			   }
			case 5: {
				check_rented_films();
				break;
				}
			case 6: {
				renturn_rented_film(client_socket);
				break;
				}
			case 7: {
				// Logout
				clear_screen();
				printf("=== CARRELLO ===\n");
				print_cart();
				printf("\nConfermare il logout? (s/n): ");
				char confirm;
				scanf(" %c", &confirm);
				if(confirm == 's' || confirm == 'S'){
					printf("Logout in corso...\n");
					cart.dim = 0;
					return;
				}
				break;
			}
			default:
				printf("Scelta non valida.\n");
				sleep(2);
		}
	}
}

void expired_films_signal_handler(int signum){

	if(signum == SIGUSR1){
		film_reminder = 1;
	}
}

void get_all_user_expired_films_with_no_due_date(int client_socket){

	char get_user_expired_films_with_no_due_date_protocol_command[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(get_user_expired_films_with_no_due_date_protocol_command, GET_USER_EXIRED_FILMS_NO_DUE_DATE_PROTOCOL_MESSAGE);

	if(write(client_socket, get_user_expired_films_with_no_due_date_protocol_command, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo\n");
		exit(-1);
	}

	if(write(client_socket, &user_id, sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibile inviare lo user_id dell'utente correntemente autenticato.\n");
		exit(-1);
	}

	char response[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

	int num_expired_films;
	film_t expired_films[MAX_FILMS];

	if(read(client_socket, response, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		perror("[CLIENT] Impossibile leggere il messaggio in arrivo\n");
		exit(-1);
	}

	if(read(client_socket, &num_expired_films, sizeof(num_expired_films)) < 0){
		printf("[CLIENT] Errore nella ricezione del numero di film con data di scadenza passata e non restituiti ancora.\n");
		exit(-1);
	}

	for(int i = 0; i < num_expired_films; i++){

		if(read(client_socket, &expired_films[i].id, sizeof(expired_films[i].id)) < 0){
			printf("[CLIENT] Errore nella ricezione del film id\n");
			exit(-1);
		}

		if(read(client_socket, expired_films[i].title, MAX_FILM_TITLE_SIZE) < 0){
			printf("[CLIENT] Errore nella ricezione del titolo film\n");
				exit(-1);
		}
	}
}

void shopkeeper_notify_expired_films(int client_socket){

	char notify_expired_films_protocol_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(notify_expired_films_protocol_message, SHOPKEEPER_NOTIFY_EXPIRED_FILMS_PROTOCOL_MESSAGE);

	if(write(client_socket, notify_expired_films_protocol_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo\n");
		exit(-1);
	}

	if(write(client_socket, &user_id, sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibile inviare lo user_id dell'utente correntemente autenticato.\n");
		exit(-1);
	}

	if(check_server_response(client_socket) < 0){
		sleep(2);
		return;
	}

}

void shopkeeper_menu(int client_socket){

	int choice = -1;

	while(choice != 3){
		printf("1) Invia notifica per film non restituiti.\n");
		printf("2) Imposta limite del carrello.\n");
		printf("3) Esci.\n");
		printf("Scelta: ");
		scanf("%d", &choice);
		getchar(); // consuma \n
		switch(choice){
			case 1:
				shopkeeper_notify_expired_films(client_socket);
				break;
			case 2:
				set_cap_films(client_socket);
				break;
			case 3:
				printf("Arrivederci!\n");
				exit(0);
			default:
				printf("Scelta non valida.\n");
		}
	}	
}

void set_cap_films(int client_socket){

	int new_film_cap; 

	printf("A quanto si vuole impostare il nuovo limite di film da affittare: ");
	scanf("%d", &new_film_cap);
	getchar(); // Consuma il \n


	char protocol_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(protocol_message, SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_PROTOCOL_MESSAGE);

	// Invio il messaggio di protocollo per cambiare il cap dei film "affitabili"
	if(write(client_socket, protocol_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo.\n");
		exit(-1);
	}

	if(write(client_socket, &user_id, sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo.\n");
		exit(-1);
	}

	// Invio il nuovo limite di film affitabili
	if(write(client_socket, &new_film_cap, sizeof(new_film_cap)) < 0){
		printf("[CLIENT] Impossibile inviare il nuovo limite per i film.\n");
		exit(-1);
	}

	if(check_server_response(client_socket) < 0){
		sleep(2);
		return;
	}

}


void empty_out_cart(){

	for(int i = 0; i < cart.dim; i++){
		cart.film_id_to_rent[i] = 0;
	}
	cart.dim = 0;
}

void rent_film(int client_socket, int idx){

	char protocol_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(protocol_message, RENT_FILM_PROTOCOL_MESSAGE);

	if(write(client_socket, protocol_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo.\n");
		exit(-1);
	}


	if(write(client_socket, &(user_id), sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibile inviare user_id.\n");
		exit(-1);
	}
	if(write(client_socket, &(cart.film_id_to_rent[idx]), sizeof(cart.film_id_to_rent[idx])) < 0){
		printf("[CLIENT] Impossibile inviare l'id del film.\n");
		exit(-1);
	}

	if(check_server_response(client_socket) < 0){
		sleep(2);
		return;
	}

	
}

void proceed_to_checkout(int client_socket){

	int choice; 
	printf("Procedere al checkout?\n");
	printf("1 - si\n");
	printf("2 - no\n");
	printf("Scelta: ");
	scanf("%d", &choice);
	getchar();

	if(choice == 2)
		return;
	else if(choice == 1 && cart.dim > 0){

		for(int i = 0; i < cart.dim; i++){
			rent_film(client_socket, i);
		}
		empty_out_cart();

	}else{
		printf("Opzione non valida!\n");
		return;
	}

}

void check_rented_films(){
	get_user_rented_films(client_socket);

	print_rented_films();
}

void renturn_rented_film(int client_socket){
	print_rented_films();

	int id_film_to_return;
	printf("Inserisci l'id del film da resituire: ");
	scanf("%d", &id_film_to_return);
	getchar(); //consuma \n

	char protocol_message[PROTOCOL_MESSAGE_MAX_SIZE];
	strcpy(protocol_message, RETURN_RENTED_FILM_PROTOCOL_MESSAGE);
	
	if(write(client_socket, protocol_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare messaggio di protocollo: %s\n", protocol_message);
		exit(-1);
	}

	if(write(client_socket, &user_id, sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibile inviare lo USER id\n");
		exit(-1);
	}

	if(write(client_socket, &id_film_to_return, sizeof(id_film_to_return)) < 0){
		printf("[CLIENT] Impossibile inviare l'id del film da restituire\n");
		exit(-1);
	}

	if(check_server_response(client_socket) < 0){
		sleep(2);
		return;
	}
}
