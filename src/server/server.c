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
#define MAX_USERS 		100
#define MAX_FILMS		100
#define MAX_RESERVATIONS	100
#define MAX_USER_USERNAME_SIZE	100
#define MAX_USER_PASSWORD_SIZE	100
#define MAX_FILM_TITLE_SIZE	100

typedef struct user {

	int id;
	char username[MAX_USER_USERNAME_SIZE];
	char password[MAX_USER_PASSWORD_SIZE];

} user;

typedef struct film {

	int id;
	char title[MAX_FILM_TITLE_SIZE];
	int available_copies;
	int rented_out_copies;

} film;

typedef struct reservation {

	int id;
	time_t rental_date;
	time_t due_date;
	int user_id;
	int film_id;

} reservation; 

typedef struct user_list {

	user users[MAX_USERS];
	int dim;
	int in_idx;
	int out_idx;
	pthread_mutext_t users_mutex;	
}

typedef struct film_list {

	film films[MAX_FILMS];
	int dim;
	int in_idx;
	int out_idx;
	pthread_mutex_t films_mutex;
}

typedef struct reservation_list {

	reservation reservations[MAX_RESERVATIONS];
	int dim;
	int in_idx;
	int out_idx;
	pthread_mutex_t reservations_mutex;
}

user_list *user_list;
film_list *film_list;
reservation_list *reservation_list;

//init global data list
user_list* init_user_list();
film_list* init_film_list();
reservation_list* init_reservation_list();

//database connection
void database_connection_init(sqlite3** database);

//database init tables
void database_user_table_init(sqlite3* database);
void database_film_table_init(sqlite3* database);
void database_reservation_table_init(sqlite3* database);

//database inserts
void database_user_insert(sqlite3* database, user* user_to_insert);
void database_film_insert(sqlite3* database, film* film_to_insert);
void database_reservation_insert(sqlite3* database, reservation* reservation_to_insert);

//database update
void database_film_add_rented_out_copy(sqlite3* database, int film_id);
void database_film_remove_rented_out_copy(sqlite3* database, int film_id);
void database_reservation_add_due_date(sqlite3* database, int reservation_id);

//threads
void* connection_handler(void* arg);

//miscellous
void error_handler(char *message);

int main(){
	
	int server_socket, client_socket;
	sqlite3 *database;

	database_connection_init(&database);
	sqlite3_busy_timeout(database, 10000);

	printf("\n[SERVER] Database connesso.\n");
	
	database_user_table_init(database);
	database_film_table_init(database);
	database_reservation_table_init(database);
	
	printf("\n[SERVER] Tabelle del database create.\n");

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

//init global data list
user_list* init_user_list(){

	user_list *list = (user_list *)malloc
}

film_list* init_film_list();
reservation_list* init_reservation_list();

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

//inserimento
void database_user_insert(sqlite3* database, user* user_to_insert){

	sqlite3_stmt* prepared;
	const char *statement_sql = "INSERT INTO USER(username, password) VALUES (?,?);";

	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento USER table sqlite");
	}
	
	sqlite3_bind_text(prepared, 1, user_to_insert->username, -1, SQLITE_STATIC);
	sqlite3_bind_text(prepared, 2, user_to_insert->password, -1, SQLITE_STATIC);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Inserito USER(%s, %s)\n.", user_to_insert->username, user_to_insert->password);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento USER table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

void database_film_insert(sqlite3* database, film* film_to_insert){

	sqlite3_stmt* prepared;
	const char *statement_sql = "INSERT INTO FILM(title, available_copies) VALUES (?,?);";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento FILM table sqlite");
	}
	
	sqlite3_bind_text(prepared, 1, film_to_insert->title, -1, SQLITE_STATIC);
	sqlite3_bind_int(prepared, 2, film_to_insert->available_copies);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Inserito FILM(%s, %d)\n.", film_to_insert->title, film_to_insert->available_copies);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento FILM table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

void database_reservation_insert(sqlite3* database, reservation* reservation_to_insert){
	
	sqlite3_stmt* prepared;
	const char *statement_sql = "INSERT INTO RESERVATION(rental_date, user_id, film_id) VALUES (?,?,?);";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento RESERVATION table sqlite");
	}
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)reservation_to_insert->rental_date);
	sqlite3_bind_int(prepared, 2, reservation_to_insert->user_id);
	sqlite3_bind_int(prepared, 3, reservation_to_insert->film_id);

	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Inserito RESERVATION(%lld, %d, %d)\n.", (long long)reservation_to_insert->rental_date, reservation_to_insert->user_id, reservation_to_insert->film_id);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento RESERVATION table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

//update
void database_film_add_rented_out_copy(sqlite3* database, int film_id) {

	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE FILM SET rented_out_copies = rented_out_copies + 1 WHERE film_id = ?;";
	
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
	const char *statement_sql = "UPDATE FILM SET rented_out_copies = rented_out_copies - 1 WHERE film_id = ?;";
	
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
	const char *statement_sql = "UPDATE RESERVATION SET due_date = ? WHERE reservation_id = ?;";
	
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

//recupero tutte risorse al loading


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
