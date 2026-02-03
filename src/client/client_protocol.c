#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "client_protocol.h"
#include "client_logic.h"

// ============================================================================
// PROTOCOL I/O
// ============================================================================

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

// ============================================================================
// SERVER RESPONSE HANDLING
// ============================================================================

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

	} else if (strncmp(response, SUCCESS_GET_FILMS, strlen(SUCCESS_GET_FILMS)) == 0){

		printf("[CLIENT] Ottenuti tutti i film della videoteca con successo!\n");
		return 0;

	} else if (strncmp(response, SUCCESS_RENT_FILM, strlen(SUCCESS_RENT_FILM)) == 0){

		printf("[CLIENT] Film noleggato con successo!\n");
		return 0;

	} else if (strncmp(response, SUCCESS_RETURN_RENTED_FILM, strlen(SUCCESS_RETURN_RENTED_FILM)) == 0){

		printf("[CLIENT] Film restituito con successo!\n");
		return 0;

	} else if (strncmp(response, SUCCESS_GET_MAX_RENTED_FILMS, strlen(SUCCESS_GET_MAX_RENTED_FILMS)) == 0){

		printf("[CLIENT] Ottenuto il numero massimo di film noleggiabili imposto dal negoziante con successo!\n");
		return 0;

	} else if (strncmp(response, SUCCESS_GET_USER_EXIRED_FILMS_NO_DUE_DATE, strlen(SUCCESS_GET_USER_EXIRED_FILMS_NO_DUE_DATE)) == 0){

		printf("[CLIENT] Ottenuti tutti i film il cui noleggio scaduto per l'utente con successo!\n");
		return 0;

	} else if (strncmp(response, SUCCESS_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS, strlen(SUCCESS_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS)) == 0){

		printf("[CLIENT] Modificato il numero massimo di film noleggiabili con successo!\n");
		return 0;

	} else if (strncmp(response, SUCCESS_SHOPKEEPER_NOTIFY_EXPIRED_FILMS, strlen(SUCCESS_SHOPKEEPER_NOTIFY_EXPIRED_FILMS)) == 0){

		printf("[CLIENT] Inviata notifica di restituzione dei film noleggiati e scaduti a tutti gli utenti con successo!\n");
		return 0;

	} else if (strncmp(response, FAILED_USER_ALREADY_EXISTS, strlen(FAILED_USER_ALREADY_EXISTS)) == 0){

		printf("[CLIENT] L'username specificato già appartiene ad un altro utente!\n");
		return -1;

	}else if (strncmp(response, FAILED_USER_DOESNT_EXISTS, strlen(FAILED_USER_DOESNT_EXISTS)) == 0){

		printf("[CLIENT] L'username specificato non appartiene a nessun utente!\n");
		return -1;

	}else if (strncmp(response, FAILED_USER_BAD_CREDENTIALS, strlen(FAILED_USER_BAD_CREDENTIALS)) == 0){


		printf("[CLIENT] Credenziali errate!\n");
		return -1;

	} else if (strncmp(response, FAILED_RENT_FILM_MAX_ALLOWED, strlen(FAILED_RENT_FILM_MAX_ALLOWED)) == 0){

		printf("[CLIENT] Raggiunto il numero massimo di film noleggiabili imposto dal venditore, non è possibile noleggiare altri film!\n");
		return -1;

	} else if (strncmp(response, FAILED_RENT_FILM_NO_AVAILABLE_COPY, strlen(FAILED_RENT_FILM_NO_AVAILABLE_COPY)) == 0){

		printf("[CLIENT] Per il film selezionato, non sono attualmente disponibili delle copie noleggiabili, quindi non è possibile procedere con il noleggio!\n");
		return -1;

	} else if (strncmp(response, FAILED_RENT_ALREADY_EXISTS, strlen(FAILED_RENT_ALREADY_EXISTS)) == 0){

		printf("[CLIENT] Il film è già stato noleggiato e non risulta restituito, non è possibile procedere con il noleggio!\n");
		return -1;

	} else if (strncmp(response, FAILED_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT, strlen(FAILED_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT)) == 0){

		printf("[CLIENT] Non è possibile restituire il film in quanto il numero di copie noleggiate è già 0!\n");
		return -1;

	} else if (strncmp(response, FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE, strlen(FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE)) == 0){

		printf("[CLIENT] Non è possibile modificare il massimo numero di film noleggiabili dagli utenti se non si è autenticati come negoziante!\n");
		return -1;

	} else if (strncmp(response, FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED, strlen(FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED)) == 0){

		printf("[CLIENT] Non è possibile modificare il numero massimo di film noleggiabili dagli utenti in quanto ci sono utenti che stanno attualmente noleggiando un numero maggiore di film rispetto al numero proposto!\n");
		return -1;

	} else if (strncmp(response, FAILED_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE, strlen(FAILED_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE)) == 0){

		printf("[CLIENT] Non è possibile notificare tutti gli utenti per restituire i film con noleggio scaduto se non si è autenticati come negoziante!\n");
		return -1;

	}

	return 0;

}

void rent_film(int client_socket, int idx){
	char protocol_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(protocol_message, RENT_FILM_PROTOCOL_MESSAGE);

	if(write(client_socket, protocol_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		printf("[CLIENT] Impossibile inviare il messaggio di protocollo.\n");
		exit(-1);
	}

	if(write(client_socket, &user_id, sizeof(user_id)) < 0){
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

void return_film(int client_socket, int film_id){
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

	if(write(client_socket, &film_id, sizeof(film_id)) < 0){
		printf("[CLIENT] Impossibile inviare l'id del film da restituire\n");
		exit(-1);
	}

	if(check_server_response(client_socket) < 0){
		sleep(2);
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
