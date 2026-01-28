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
#include <sqlite3.h>
#include <time.h>

#define MAX_ACCEPTED_CLIENT 	50
#define MAX_USERS 				100
#define MAX_FILMS				100
#define MAX_RESERVATIONS		100
#define MAX_USER_USERNAME_SIZE	100
#define MAX_USER_PASSWORD_SIZE	100
#define MAX_FILM_TITLE_SIZE		100

#define REGISTER_PROTOCOL_MESSAGE 					"REGISTER"
#define LOGIN_PROTOCOL_MESSAGE 						"LOGIN"
#define GET_FILMS_PROTOCOL_MESSAGE  				"GET_FILMS"
#define RENT_FILM_PROTOCOL_MESSAGE  				"RENT_FILM"
#define RETURN_RENTED_FILM_PROTOCOL_MESSAGE			"RETURN_RENTED_FILM"

#define SUCCESS_SERVER_RESPONSE						"SUCCESS"
#define FAILED_SERVER_RESPONSE						"FAILED"
#define PROTOCOL_MESSAGE_MAX_SIZE 					20

//data types
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

typedef struct reservation_t {

	int id;
	time_t rental_date;
	time_t due_date;
	int user_id;
	int film_id;

} reservation_t; 

typedef struct user_list_t {

	user_t* users[MAX_USERS];
	int dim;
	int in_idx;
	int out_idx;
	pthread_mutex_t users_mutex;

} user_list_t;

typedef struct film_list_t {

	film_t* films[MAX_FILMS];
	int dim;
	int in_idx;
	int out_idx;
	pthread_mutex_t films_mutex;

} film_list_t;

typedef struct reservation_list_t {

	reservation_t* reservations[MAX_RESERVATIONS];
	int dim;
	int in_idx;
	int out_idx;
	pthread_mutex_t reservations_mutex;

} reservation_list_t;

//global lists
user_list_t *user_list;
film_list_t *film_list;
reservation_list_t *reservation_list;
sqlite3 *database;

//init global lists
user_list_t* init_user_list();
film_list_t* init_film_list();
reservation_list_t* init_reservation_list();

//aggiunte alle liste globali
user_t* add_user_to_user_list(int id, char *username, char *password);
film_t* add_film_to_film_list(int id, char *title, int available_copies, int rented_out_copies);
reservation_t* add_reservation_to_reservation_list(int id, time_t rental_date, time_t due_date, int user_id, int film_id);

//database connection
void database_connection_init(sqlite3** database);

//database init tables
void database_user_table_init(sqlite3* database);
void database_film_table_init(sqlite3* database);
void database_reservation_table_init(sqlite3* database);

//sync dati presenti su DB in memoria
void database_user_list_sync(sqlite3* database);
void database_film_list_sync(sqlite3* database);
void database_reservation_list_sync(sqlite3* database);

//database inserts
int database_user_insert(sqlite3* database, char *user_username, char *user_password);
int database_film_insert(sqlite3* database, char *film_title, int film_available_copies);
int database_reservation_insert(sqlite3* database, time_t reservation_rental_date, int reservation_user_id, int film_id);

//database update
void database_film_add_rented_out_copy(sqlite3* database, int film_id);
void database_film_remove_rented_out_copy(sqlite3* database, int film_id);
void database_reservation_add_due_date(sqlite3* database, int reservation_id);

//wrappers
void create_new_user(sqlite3* database, char *user_username, char *user_password);
void create_new_film(sqlite3* database, char *film_title, int film_available_copies);
void create_new_reservation(sqlite3* database, int reservation_user_id, int reservation_film_id);

//search
user_t* search_user_by_id(int user_id);
film_t* serach_film_by_id(int film_id);
reservation_t* search_reservation_by_id(int reservation_id);

//threads
void* connection_handler(void* arg);

//miscellous
void error_handler(char *message);

int main(){
	
	int server_socket, client_socket;

	database_connection_init(&database);
	sqlite3_busy_timeout(database, 10000);

	printf("\n[SERVER] Database connesso.\n");
	
	database_user_table_init(database);
	database_film_table_init(database);
	database_reservation_table_init(database);
	
	printf("\n[SERVER] Tabelle del database create.\n");

	user_list = init_user_list();
	film_list = init_film_list();
	reservation_list = init_reservation_list();

	database_user_list_sync(database);
	database_film_list_sync(database);
	database_reservation_list_sync(database);

	printf("\n[SERVER] Dati sincronizzati dal database.\n");

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
	sqlite3_close(database);
	unlink(server_address.sun_path);

	return 0;
}

//threads
void* connection_handler(void* client_socket_arg){
	
	printf("\n[SERVER] Assegnato il thread %d al client.\n", (int)pthread_self());

	pthread_detach(pthread_self());

	int client_socket = *(int*)client_socket_arg;
	free(client_socket_arg);

	char protocol_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

	while(read(client_socket, protocol_message, PROTOCOL_MESSAGE_MAX_SIZE) > 0){
		
		if (strncmp(protocol_message, REGISTER_PROTOCOL_MESSAGE, strlen(REGISTER_PROTOCOL_MESSAGE)) == 0){

			char user_username[MAX_USER_USERNAME_SIZE] = {0};
			char user_password[MAX_USER_PASSWORD_SIZE] = {0};

			if(read(client_socket, user_username, MAX_USER_USERNAME_SIZE) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura USER username da socket");
			}
		
			if(read(client_socket, user_password, MAX_USER_PASSWORD_SIZE) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura USER password da socket");
			}

			create_new_user(database, user_username, user_password);

			char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
			strcpy(success_message, SUCCESS_SERVER_RESPONSE);

			if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore server write");
			}

		} else if (strncmp(protocol_message, LOGIN_PROTOCOL_MESSAGE, strlen(LOGIN_PROTOCOL_MESSAGE) == 0)){

			char user_username[MAX_USER_USERNAME_SIZE] = {0};
			char user_password[MAX_USER_PASSWORD_SIZE] = {0};

			if(read(client_socket, user_username, MAX_USER_USERNAME_SIZE) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura USER username da socket");
			}
		
			if(read(client_socket, user_password, MAX_USER_PASSWORD_SIZE) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura USER password da socket");
			}



		} else if (strncmp(protocol_message, GET_FILMS_PROTOCOL_MESSAGE, strlen(GET_FILMS_PROTOCOL_MESSAGE) == 0)){
			
		} else if (strncmp(protocol_message, RENT_FILM_PROTOCOL_MESSAGE, strlen(RENT_FILM_PROTOCOL_MESSAGE) == 0)){
			
		} else if (strncmp(protocol_message, RETURN_RENTED_FILM_PROTOCOL_MESSAGE, strlen(RETURN_RENTED_FILM_PROTOCOL_MESSAGE) == 0)){
			
		}

	}

	//
	close(client_socket);

	pthread_exit(NULL);
}

//gestione aggiunte liste
user_t* add_user_to_user_list(int id, char *username, char *password){

	pthread_mutex_lock(&user_list->users_mutex);

	if(user_list->dim >= MAX_USERS){
		printf("\n[SERVER] Raggiunto il numero massimo di USER supportati, inserimento ignorato.\n");
		pthread_mutex_unlock(&user_list->users_mutex);
		return NULL;
	}

	user_t *user_to_insert = (user_t *)malloc(sizeof(user_t));
	if(user_to_insert == NULL){
		pthread_mutex_unlock(&user_list->users_mutex);
		error_handler("[SERVER] Errore allocazione dinamica inserimento USER");
	}

	user_to_insert->id = id;

	strncpy(user_to_insert->username, username, MAX_USER_USERNAME_SIZE - 1);
	user_to_insert->username[MAX_USER_USERNAME_SIZE - 1] = '\0';

	strncpy(user_to_insert->password, password, MAX_USER_PASSWORD_SIZE - 1);
	user_to_insert->password[MAX_USER_PASSWORD_SIZE - 1] = '\0';

	user_list->users[user_list->in_idx] = user_to_insert;
	user_list->in_idx = (user_list->in_idx + 1) % MAX_USERS;
	user_list->dim = user_list->dim + 1;

	pthread_mutex_unlock(&user_list->users_mutex);

	return user_to_insert;
}

film_t* add_film_to_film_list(int id, char *title, int available_copies, int rented_out_copies){

	pthread_mutex_lock(&film_list->films_mutex);

	if(film_list->dim >= MAX_FILMS){
		printf("\n[SERVER] Raggiunto il numero massimo di FILM supportati, inserimento ignorato.\n");
		pthread_mutex_unlock(&film_list->films_mutex);
		return NULL;
	}

	film_t *film_to_insert = (film_t *)malloc(sizeof(film_t));
	if(film_to_insert == NULL){
		pthread_mutex_unlock(&film_list->films_mutex);
		error_handler("[SERVER] Errore allocazione dinamica inserimento FILM");
	}

	film_to_insert->id = id;

	strncpy(film_to_insert->title, title, MAX_FILM_TITLE_SIZE - 1);
	film_to_insert->title[MAX_FILM_TITLE_SIZE - 1] = '\0';

	film_to_insert->available_copies = available_copies;

	film_to_insert->rented_out_copies = rented_out_copies;

	film_list->films[film_list->in_idx] = film_to_insert;
	film_list->in_idx = (film_list->in_idx + 1) % MAX_FILMS;
	film_list->dim = film_list->dim + 1;

	pthread_mutex_unlock(&film_list->films_mutex);

	return film_to_insert;	
}

reservation_t* add_reservation_to_reservation_list(int id, time_t rental_date, time_t due_date, int user_id, int film_id){

	pthread_mutex_lock(&reservation_list->reservations_mutex);

	if(reservation_list->dim >= MAX_RESERVATIONS){
		printf("\n[SERVER] Raggiunto il numero massimo di RESERVATION supportate, inserimento ignorato.\n");
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		return NULL;
	}

	reservation_t *reservation_to_insert = (reservation_t *)malloc(sizeof(reservation_t));
	if(reservation_to_insert == NULL){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		error_handler("[SERVER] Errore allocazione dinamica inserimento RESERVATION");
	}

	// ??? SI DOVREBBE CONTROLLARE E NEL CASO SEGNALARE ERRORE L'ESISTENZA DI USER E FILM CON ID PARAMETRO ???
	reservation_to_insert->id = id;
	reservation_to_insert->rental_date = rental_date;
	reservation_to_insert->due_date = due_date;
	reservation_to_insert->user_id = user_id;
	reservation_to_insert->film_id = film_id;

	reservation_list->reservations[reservation_list->in_idx] = reservation_to_insert;
	reservation_list->in_idx = (reservation_list->in_idx + 1) % MAX_RESERVATIONS;
	reservation_list->dim = reservation_list->dim + 1;

	pthread_mutex_unlock(&reservation_list->reservations_mutex);

	return reservation_to_insert;
}

//init global data list
user_list_t* init_user_list(){

	user_list_t *list = (user_list_t *)malloc(sizeof(user_list_t));
	if(list == NULL)
		error_handler("[SERVER] Errore di allocazione dinamica users list");
	
	list->dim = 0;
	list->in_idx = 0;
	list->out_idx = 0;

	if(pthread_mutex_init(&list->users_mutex, NULL) != 0)
		error_handler("[SERVER] Errore di allocazione dinamica users list mutex");

	return list;
}

film_list_t* init_film_list(){

	film_list_t *list = (film_list_t *)malloc(sizeof(film_list_t));
	if(list == NULL)
		error_handler("[SERVER] Errore di allocazione dinamica film list");

	list->dim = 0;
	list->in_idx = 0;
	list->out_idx = 0;

	if(pthread_mutex_init(&list->films_mutex, NULL) != 0)
		error_handler("[SERVER] Errore di allocazione dinamica films list mutex");

	return list;
}

reservation_list_t* init_reservation_list(){

	reservation_list_t *list = (reservation_list_t *)malloc(sizeof(reservation_list_t));
	if(list == NULL)
		error_handler("[SERVER] Errore di allocazione dinamica reservation list");

	list->dim = 0;
	list->in_idx = 0;
	list->out_idx = 0;

	if(pthread_mutex_init(&list->reservations_mutex, NULL) != 0)
		error_handler("[SERVER] Errore di allocazione dinamica reservations list mutex");

	return list;
}

//creazione tabelle database
void database_connection_init(sqlite3 **database){

	if(sqlite3_open("database.db", database) != 0){
		sqlite3_close(*database);
		error_handler("[SERVER] Errore creazione/apertura database");
	}
}

void database_user_table_init(sqlite3* database){

	char *statement_sql =
		"CREATE TABLE IF NOT EXISTS USER("
		"id INTEGER PRIMARY KEY AUTOINCREMENT, "
		"username VARCHAR NOT NULL UNIQUE, "
		"password VARCHAR NOT NULL"
		");";
		
	if(sqlite3_exec(database, statement_sql, 0, 0, NULL) != 0){
		sqlite3_close(database);
		error_handler("[SERVER] Errore creazione USER table sqlite");
	}
}

void database_film_table_init(sqlite3* database){

	char *statement_sql =
		"CREATE TABLE IF NOT EXISTS FILM("
		"id INTEGER PRIMARY KEY AUTOINCREMENT, "
		"title VARCHAR NOT NULL, "
		"available_copies INTEGER NOT NULL, "
		"rented_out_copies INTEGER NOT NULL DEFAULT 0"
		");";
		
	if(sqlite3_exec(database, statement_sql, 0, 0, NULL) != 0){
		sqlite3_close(database);
		error_handler("[SERVER] Errore creazione FILM table sqlite");
	}
}

void database_reservation_table_init(sqlite3* database){

	char *statement_sql =
		"CREATE TABLE IF NOT EXISTS RESERVATION("
		"id INTEGER PRIMARY KEY AUTOINCREMENT, "
		"rental_date INTEGER NOT NULL, "
		"due_date INTEGER, "
		"user_id INTEGER NOT NULL, "
		"film_id INTEGER NOT NULL, "
		"FOREIGN KEY (user_id) REFERENCES USER(id), "
        "FOREIGN KEY (film_id) REFERENCES FILM(id)"
        ");";
		
	if(sqlite3_exec(database, statement_sql, 0, 0, NULL) != 0){
		sqlite3_close(database);
		error_handler("[SERVER] Errore creazione RESERVATION table sqlite");
	}
}

//recupero tutte risorse al loading
void database_user_list_sync(sqlite3* database){

	sqlite3_stmt *prepared;
	const char *statement_sql = "SELECT * FROM USER;";

	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore recupero degli USER allo startup");
	}

	while (sqlite3_step(prepared) == SQLITE_ROW){

		int user_id = sqlite3_column_int(prepared, 0);
		char *user_username = (char *) sqlite3_column_text(prepared, 1);
		char *user_password = (char *) sqlite3_column_text(prepared, 2);

		if(add_user_to_user_list(user_id, user_username, user_password) == NULL){
			printf("\n[SERVER] Ulteriori USER su database ignorati.\n");
			break;
		}
	}

	sqlite3_finalize(prepared);
}

void database_film_list_sync(sqlite3* database){

	sqlite3_stmt *prepared;
	const char *statement_sql = "SELECT * FROM FILM;";

	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore recupero dei FILM allo startup");
	}

	while (sqlite3_step(prepared) == SQLITE_ROW){

		int film_id = sqlite3_column_int(prepared, 0);
		char *film_title = (char *) sqlite3_column_text(prepared, 1);
		int film_available_copies = sqlite3_column_int(prepared, 2);
		int film_rented_out_copies = sqlite3_column_int(prepared, 3); 

		if(add_film_to_film_list(film_id, film_title, film_available_copies, film_rented_out_copies) == NULL){
			printf("\n[SERVER] Ulteriori FILM su database ignorati.\n");
			break;
		}
	}

	sqlite3_finalize(prepared);
}

void database_reservation_list_sync(sqlite3* database){

	sqlite3_stmt *prepared;
	const char *statement_sql = "SELECT * FROM RESERVATION;";

	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore recupero delle RESERVATION allo startup");
	}

	while (sqlite3_step(prepared) == SQLITE_ROW){

		int reservation_id = sqlite3_column_int(prepared, 0);
		time_t reservation_rental_date = sqlite3_column_int64(prepared, 1);
		time_t reservation_due_date = sqlite3_column_int64(prepared, 2);
		int reservation_user_id = sqlite3_column_int(prepared, 3);
		int reservation_film_id = sqlite3_column_int(prepared, 4);

		if(add_reservation_to_reservation_list(reservation_id, reservation_rental_date, reservation_due_date, reservation_user_id, reservation_film_id) == NULL){
			printf("\n[SERVER] Ulteriori RESERVATION su database ignorate.\n");
			break;
		}
	}

	sqlite3_finalize(prepared);
}

//inserimento
int database_user_insert(sqlite3* database, char *user_username, char *user_password){

	sqlite3_stmt* prepared;
	const char *statement_sql = "INSERT INTO USER(username, password) VALUES (?,?);";

	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento USER table sqlite");
	}
	
	sqlite3_bind_text(prepared, 1, user_username, -1, SQLITE_STATIC);
	sqlite3_bind_text(prepared, 2, user_password, -1, SQLITE_STATIC);
	
	int user_id = -1;

	if(sqlite3_step(prepared) == SQLITE_DONE){
		user_id = (int)sqlite3_last_insert_rowid(database);
		printf("\n[SERVER] Inserito USER(%s, %s).\n", user_username, user_password);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento USER table sqlite");
	}
	
	sqlite3_finalize(prepared);
	return user_id;
}

int database_film_insert(sqlite3* database, char *film_title, int film_available_copies){

	sqlite3_stmt* prepared;
	const char *statement_sql = "INSERT INTO FILM(title, available_copies) VALUES (?,?);";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento FILM table sqlite");
	}
	
	int film_id = -1;

	sqlite3_bind_text(prepared, 1, film_title, -1, SQLITE_STATIC);
	sqlite3_bind_int(prepared, 2, film_available_copies);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		film_id = sqlite3_last_insert_rowid(database);
		printf("\n[SERVER] Inserito FILM(%s, %d).\n", film_title, film_available_copies);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento FILM table sqlite");
	}
	
	sqlite3_finalize(prepared);
	return film_id;
}

int database_reservation_insert(sqlite3* database, time_t reservation_rental_date, int reservation_user_id, int reservation_film_id){
	
	sqlite3_stmt* prepared;
	const char *statement_sql = "INSERT INTO RESERVATION(rental_date, user_id, film_id) VALUES (?,?,?);";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento RESERVATION table sqlite");
	}

	int reservation_id = -1;
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)reservation_rental_date);
	sqlite3_bind_int(prepared, 2, reservation_user_id);
	sqlite3_bind_int(prepared, 3, reservation_film_id);

	if(sqlite3_step(prepared) == SQLITE_DONE){
		reservation_id = (int)sqlite3_last_insert_rowid(database);
		printf("\n[SERVER] Inserito RESERVATION(%lld, %d, %d).\n", (long long)reservation_rental_date, reservation_user_id, reservation_film_id);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento RESERVATION table sqlite");
	}
	
	sqlite3_finalize(prepared);
	return reservation_id;
}

//update
void database_film_add_rented_out_copy(sqlite3* database, int film_id) {

	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE FILM SET rented_out_copies = rented_out_copies + 1 WHERE id = ?;";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore update FILM table sqlite");
	}
	
	sqlite3_bind_int(prepared, 1, film_id);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Aggiunta al FILM(%d) +1 copia noleggiata.\n", film_id);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore aggiornamento FILM table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

void database_film_remove_rented_out_copy(sqlite3* database, int film_id) {

	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE FILM SET rented_out_copies = rented_out_copies - 1 WHERE id = ?;";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore update FILM table sqlite");
	}
	
	sqlite3_bind_int(prepared, 1, film_id);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Aggiunta al FILM(%d) -1 copia noleggiata.\n", film_id);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore aggiornamento FILM table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

void database_reservation_add_due_date(sqlite3* database, int reservation_id) {
	
	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE RESERVATION SET due_date = ? WHERE id = ?;";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore update RESERVATION table sqlite");
	}
	
	//calcolo della data esatta di adesso
	time_t now = time(NULL);
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)now);
	sqlite3_bind_int(prepared, 2, reservation_id);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Aggiunta alla RESERVATION(%d) due_date = %lld.\n", reservation_id, (long long)now);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore aggiornamento RESERVATION table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

//wrapper per aggiunta in memory + database
void create_new_user(sqlite3* database, char *user_username, char *user_password){

	int user_id = database_user_insert(database, user_username, user_password);

	if(user_id == -1)
		error_handler("[SERVER] Errore creazione nuovo USER");

	if(add_user_to_user_list(user_id, user_username, user_password) != NULL){
		printf("\n[SERVER] Nuovo USER(%d, %s, %s) sincronizzato in memoria.\n", user_id, user_username, user_password);
	} else {
		error_handler("[SERVER] USER salvato su database ma lista piena in memoria. L'USER esiste ma non è in memoria");
	}
}

void create_new_film(sqlite3* database, char *film_title, int film_available_copies){

	int film_id = database_film_insert(database, film_title, film_available_copies);

	if(film_id == -1)
		error_handler("[SERVER] Errore creazione nuovo FILM");

	if(add_film_to_film_list(film_id, film_title, film_available_copies, 0) != NULL){
		printf("\n[SERVER] Nuovo FILM(%d, %s, %d) sincronizzato in memoria.\n", film_id, film_title, film_available_copies);
	} else {
		error_handler("[SERVER] FILM salvato sul database ma lista piena in memoria. Il FILM esiste ma non è in memoria");
	}
}

void create_new_reservation(sqlite3* database, int reservation_user_id, int reservation_film_id){

	time_t reservation_rental_date = time(NULL);
	int reservation_id = database_reservation_insert(database, reservation_rental_date, reservation_user_id, reservation_film_id);

	if(reservation_id == -1)
		error_handler("[SERVER] Errore creazione nuova RESERVATION");

	if(add_reservation_to_reservation_list(reservation_id, reservation_rental_date, 0, reservation_user_id, reservation_film_id) != NULL){
		printf("\n[SERVER] Nuova RESERVATION(%d, %lld, %d, %d) sincronizzata in memoria.\n", reservation_id, (long long)reservation_rental_date, reservation_user_id, reservation_film_id);
		database_film_add_rented_out_copy(database, reservation_film_id);
	} else {
		error_handler("[SERVER] RESERVATION salvata sul database ma lista piena in memoria. La RESERVATION esiste ma non è in memoria");
	}
}

//search
user_t* search_user_by_id(int user_id){

	pthread_mutex_lock(&user_list->users_mutex);

	user_t* searched_user = NULL;

	for(int i = 0; i < user_list->dim; i++){
		if(user_list->users[i]->id == user_id){
			searched_user = user_list->users[i];
			break;
		}
	}

	pthread_mutex_unlock(&user_list->users_mutex);

	return searched_user;
}

user_t* search_user_by_username(char *user_username){

	pthread_mutex_lock(&user_list->users_mutex);

	user_t* searched_user = NULL;

	for(int i = 0; i < user_list->dim; i++){

		if(strncmp(user_list->users[i]->username == )){
			searched_user = user_list->users[i];
			break;
		}
	}

	pthread_mutex_unlock(&user_list->users_mutex);

	return searched_user;
}

film_t* serach_film_by_id(int film_id){

	pthread_mutex_lock(&film_list->films_mutex);

	film_t* searched_film = NULL;

	for(int i = 0; i < film_list->dim; i++){
		if(film_list->films[i]->id == film_id){
			searched_film = film_list->films[i];
			break;
		}
	}

	pthread_mutex_unlock(&film_list->films_mutex);

	return searched_film;
}

reservation_t* search_reservation_by_id(int reservation_id){

	pthread_mutex_lock(&reservation_list->reservations_mutex);

	reservation_t* searched_reservation = NULL;

	for(int i = 0; i < reservation_list->dim; i++){
		if(reservation_list->reservations[i]->id == reservation_id){
			searched_reservation = reservation_list->reservations[i];
			break;
		}
	}

	pthread_mutex_unlock(&reservation_list->reservations_mutex);

	return searched_reservation;
}

void error_handler(char *message){
	printf("\n%s.\n", message);
	exit(-1);
}