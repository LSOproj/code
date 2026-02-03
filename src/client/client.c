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
#include "client.h"
#include "client_protocol.h"
#include "client_logic.h"

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

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

int cart_cap = 5;

// ============================================================================
// MAIN
// ============================================================================

int main(){
	struct sockaddr_un server_address;
	socklen_t server_address_len = sizeof(server_address);

	server_address.sun_family = AF_LOCAL;
	// /tmp/server_socket socket locale del server a cui
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

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

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

void consume_stdin_line(void){
	int c;
	while((c = getchar()) != '\n' && c != EOF){ }
}

int read_menu_choice(const char *prompt){
	int choice = -1;
	printf("%s", prompt);
	int result = scanf("%d", &choice);
	if(result != 1){
		consume_stdin_line();
		return -1;
	}
	consume_stdin_line();
	return choice;
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
	printf("\n=== FILM NOLEGGIATI ===\n");
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

void empty_out_cart(){
	for(int i = 0; i < cart.dim; i++){
		cart.film_id_to_rent[i] = 0;
	}
	cart.dim = 0;
}

void print_expired_films(){
	printf("-----------------------------------------------------------------------\n");
	printf("Restituire i seguenti film, in quanto e' terminato il periodo di noleggio.\n");
	printf("-----------------------------------------------------------------------\n");
	printf("%-3s %-30s\n",
			"ID", "Title");
	printf("-----------------------------------------------------------------------\n");
	for(int i = 0; i < num_expired_films; i++){
		printf("%-3d %-30s\n",
		expired_films[i].id,
		expired_films[i].title);
	}
}

// ============================================================================
// MENU UI
// ============================================================================

int start_up_menu(void){
	printf("1 - Register\n2 - Login\n0 - Exit\n");
	return read_menu_choice("Inserire un numero per proseguire: ");
}

int renting_menu(){
	printf("\n=== MENU NOLEGGIO ===\n");
	printf("1 - Inserisci ID film da noleggiare (separati da virgola, max 5)\n");
	printf("2 - Modifica carrello\n");
	printf("3 - Procedere al checkout\n");
	printf("4 - Stampa film noleggiati\n");
	printf("5 - Restituzione film noleggiato\n");
	printf("6 - Logout\n");
	return read_menu_choice("Inserire un numero per proseguire: ");
}

int shopkeeper_menu_display(){
	printf("\n=== MENU NEGOZIANTE ===\n");
	printf("1 - Invia notifica per film non restituiti\n");
	printf("2 - Imposta limite del carrello\n");
	printf("0 - Esci\n");
	return read_menu_choice("Inserire un numero per proseguire: ");
}

void rental_menu(int client_socket){
	int choice;

	show_main_view();
	
	while(1){
		// Aggiorna sempre la lista dei film noleggiati all'inizio del loop
		get_all_films(client_socket);
		get_user_rented_films(client_socket);

		if(film_reminder){
			clear_screen();
			get_all_user_expired_films_with_no_due_date(client_socket);
			print_expired_films();

			printf("\nPremere INVIO per tornare al menu...");
			getchar();

			film_reminder = 0;
		}

		show_main_view();

		choice = renting_menu();

		switch(choice){
			case 1:
				handle_add_films();
				break;
			case 2:
				handle_modify_cart();
				break;
			case 3:
				handle_checkout(client_socket);
				break;
			case 4:
				handle_show_rented(client_socket);
				break;
			case 5:
				handle_return(client_socket);
				break;
			case 6:
				if(handle_logout()){
					return;
				}
				break;
			default:
				printf("Scelta non valida.\n");
				sleep(2);
		}
	}
}


// ============================================================================
// AUTHENTICATION & USER FLOW
// ============================================================================

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

// ============================================================================
// RENTAL HANDLERS
// ============================================================================

void handle_add_films(void){
	clear_screen();
	if(cart.dim >= cart_cap){
		printf("Carrello pieno! Non è possibile aggiungere altri film.\n");
		sleep(2);
		return;
	}
	show_main_view();

	printf("\nInserire gli ID dei film da noleggiare (separati da virgola):\n");
	printf("Esempio: 1,3,5 oppure 2, 4, 7\n");
	printf("ID film: ");

	char film_input[100] = {0};
	scanf(" %99[^\n]", film_input);
	consume_stdin_line();

	if(film_input[0] == '\0'){
		printf("Nessun ID inserito.\n");
		sleep(1);
		return;
	}

	int film_ids[cart_cap];
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
}

void handle_modify_cart(void){
	if(cart.dim == 0){
		printf("Il carrello è vuoto.\n");
		sleep(2);
		show_main_view();
		return;
	}
	while(1){
		clear_screen();
		printf("=== MODIFICA CARRELLO ===\n");
		print_cart();
		printf("\n1 - Rimuovi film dal carrello\n");
		printf("2 - Svuota carrello\n");
		printf("0 - Torna al menu noleggio\n");
		int sub_choice = read_menu_choice("Scelta: ");
		if(sub_choice == 1){
			if(cart.dim > 0){
				printf("Inserire gli ID separati da virgola (es: 1,3,5)\n");
				char report[512] = {0};
				remove_from_cart(report, sizeof(report));
				clear_screen();
				printf("=== CARRELLO AGGIORNATO ===\n");
				print_cart();
				if(report[0] != '\0'){
					printf("\nRimozioni:\n%s", report);
				}
			} else {
				printf("Il carrello è vuoto.\n");
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
}

void handle_checkout(int client_socket){
	if(cart.dim == 0){
		printf("Il carrello è vuoto! Non è possibile procedere al checkout.\n");
		sleep(2);
		return;
	}

	clear_screen();
	printf("=== CHECKOUT ===\n");
	print_cart();
	printf("\n");

	char choice; 
	printf("Procedere al checkout (s/n)? ");
	scanf(" %c", &choice);
	getchar();

	if(choice == 'n' || choice == 'N')
		return;
	else if((choice == 's' || choice == 'S') && cart.dim > 0){
		for(int i = 0; i < cart.dim; i++){
			printf("Noleggiando film con ID %d...\n", cart.film_id_to_rent[i]);
			rent_film(client_socket, i);
		}
		empty_out_cart();
	}else{
		printf("Opzione non valida!\n");
		return;
	}

	printf("\nPremere INVIO per tornare al menu principale...\n");
	getchar();
	show_main_view();
}

void handle_show_rented(int client_socket){
	clear_screen();
	get_user_rented_films(client_socket);
	print_rented_films();
	printf("\nPremere INVIO per tornare al menu...\n");
	getchar();
}

void handle_return(int client_socket){
	clear_screen();
	print_rented_films();

	printf("\nInserisci gli ID dei film da restituire (separati da virgola):\n");
	printf("Esempio: 1,3,5\n");
	printf("ID film: ");
	
	char film_input[100] = {0};
	scanf(" %99[^\n]", film_input);
	consume_stdin_line();
	
	if(film_input[0] == '\0'){
		printf("Nessun ID inserito.\n");
		sleep(2);
		return;
	}
	
	int film_ids[MAX_FILMS];
	int count = 0;
	parse_film_ids_to_return(film_input, film_ids, &count);
	
	if(count == 0){
		printf("Nessun ID valido inserito.\n");
		sleep(2);
		return;
	}
	
	printf("\n");
	for(int i = 0; i < count; i++){
		// Trova il titolo del film
		char *film_title = "Film sconosciuto";
		for(int j = 0; j < num_rented_films; j++){
			if(rented_films[j].id == film_ids[i]){
				film_title = rented_films[j].title;
				break;
			}
		}
		printf("Restituendo '%s' (ID: %d)...\n", film_title, film_ids[i]);
		return_film(client_socket, film_ids[i]);
	}
	
	printf("\n✓ Operazione completata!\n");
	printf("\nPremere INVIO per tornare al menu principale...\n");
	getchar();
	show_main_view();
}

int handle_logout(void){
	clear_screen();
	printf("=== CARRELLO ===\n");
	print_cart();
	printf("\nConfermare il logout? (s/n): ");
	char confirm;
	scanf(" %c", &confirm);
	consume_stdin_line();
	if(confirm == 's' || confirm == 'S'){
		printf("Logout in corso...\n");
		cart.dim = 0;
		return 1;
	}
	show_main_view();
	return 0;
}

// ============================================================================
// SHOPKEEPER MENU
// ============================================================================

void shopkeeper_menu(int client_socket){
	int choice;

	while(1){
		clear_screen();
		choice = shopkeeper_menu_display();

		switch(choice){
			case 1:
				shopkeeper_notify_expired_films(client_socket);
				sleep(2);
				break;
			case 2:
				set_cap_films(client_socket);
				break;
			case 0:
				printf("Arrivederci!\n");
				exit(0);
			default:
				printf("Scelta non valida.\n");
				sleep(2);
		}
	}
}

void set_cap_films(int client_socket){
	int new_film_cap; 

	printf("A quanto si vuole impostare il nuovo limite di film da affittare: ");
	scanf("%d", &new_film_cap);
	consume_stdin_line();

	char protocol_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(protocol_message, SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_PROTOCOL_MESSAGE);

	if(write(client_socket, protocol_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo.\n");
		exit(-1);
	}

	if(write(client_socket, &user_id, sizeof(user_id)) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo.\n");
		exit(-1);
	}

	if(write(client_socket, &new_film_cap, sizeof(new_film_cap)) < 0){
		printf("[CLIENT] Impossibile inviare il nuovo limite per i film.\n");
		exit(-1);
	}

	if(check_server_response(client_socket) < 0){
		sleep(2);
		return;
	}
}

// ============================================================================
// SIGNAL HANDLERS
// ============================================================================

void expired_films_signal_handler(int signum){
	if(signum == SIGUSR1){
		film_reminder = 1;
	}
}

/*
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
*/
