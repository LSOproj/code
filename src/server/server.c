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
#include <libgen.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>

#define MAX_CLIENTS 				50
#define MAX_USERS 					100
#define MAX_FILMS					100
#define MAX_RESERVATIONS			100
#define MAX_USER_USERNAME_SIZE		100
#define MAX_USER_PASSWORD_SIZE		100
#define MAX_FILM_TITLE_SIZE			100
#define MAX_PATH					100
#define RENTAL_DURATION_DAYS		30
#define SECONDS_IN_DAY				(24 * 60 * 60)
#define SHOPKEEPER_USERNAME			"negoziante"
#define SHOPKEEPER_PASSWORD			"password"
#define DEFAULT_MAX_RENTED_FILMS	5

//protocol message definitions
//requests
#define REGISTER_PROTOCOL_MESSAGE 									"REGISTER"
#define LOGIN_PROTOCOL_MESSAGE 										"LOGIN"
#define GET_FILMS_PROTOCOL_MESSAGE  								"GET_FILMS"
#define RENT_FILM_PROTOCOL_MESSAGE  								"RENT_FILM"
#define RETURN_RENTED_FILM_PROTOCOL_MESSAGE							"RETURN_RENTED_FILM"
#define GET_USER_RENTED_FILMS_PROTOCOL_MESSAGE						"GET_USER_RENTED_FILMS"
#define GET_MAX_RENTED_FILMS_PROTOCOL_MESSAGE						"GET_MAX_RENTED_FILMS" //DA FARE SOLO SUL CLIENT
#define GET_USER_EXIRED_FILMS_NO_DUE_DATE_PROTOCOL_MESSAGE			"GET_USER_EXPIRED_FILMS_NO_DUE_DATE"
#define SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_PROTOCOL_MESSAGE			"SHOPKEEPER_CHANGE_MAX_RENTED_FILMS"
#define SHOPKEEPER_NOTIFY_EXPIRED_FILMS_PROTOCOL_MESSAGE			"SHOPKEEPER_NOTIFY_EXPIRED_FILMS"

//response
#define SUCCESS_REGISTER										"SUCCESS_REGISTER"
#define SUCCESS_LOGIN											"SUCCESS_LOGIN"
#define SUCCESS_GET_FILMS										"SUCCESS_GET_FILMS"
#define SUCCESS_RENT_FILM										"SUCCESS_RENT_FILM"
#define SUCCESS_RETURN_RENTED_FILM								"SUCCESS_RETURN_RENTEND_FILM"
#define SUCCESS_GET_USER_RENTED_FILMS							"SUCCESS_GET_USER_RENTED_FILMS"
#define SUCCESS_GET_MAX_RENTED_FILMS							"SUCCESS_GET_MAX_RENTED_FILMS"
#define SUCCESS_GET_USER_EXIRED_FILMS_NO_DUE_DATE				"SUCCESS_GET_USER_EXPIRED_FILMS_NO_DUE_DATE"
#define SUCCESS_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS				"SUCCESS_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS"
#define SUCCESS_SHOPKEEPER_NOTIFY_EXPIRED_FILMS					"SUCCESS_SHOPKEEPER_NOTIFY_EXPIRED_FILMS"

#define FAILED_USER_ALREDY_EXISTS								"FAILED_USER_ALREADY_EXISTS"
#define FAILED_USER_DOESNT_EXISTS								"FAILED_USER_DOESNT_EXISTS"
#define FAILED_USER_BAD_CREDENTIALS								"FAILED_USER_BAD_CREDENTIALS"

#define FAILED_RENT_FILM_MAX_ALLOWED							"FAILED_RENT_FILM_MAX_ALLOWED"
#define FAILED_RENT_FILM_NO_AVAILABLE_COPY						"FAILED_RENT_FILM_NO_AVAILABLE_COPY"
#define FAILED_RENT_ALREADY_EXISTS								"FAILED_RENT_ALREADY_EXISTS"

#define FAILED_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT			"FAILED_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT"

#define FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE		 	"FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE"
#define FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED 	"FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED"

#define FAILED_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE			 	"FAILED_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE"

#define PROTOCOL_MESSAGE_MAX_SIZE 								100

//semantic error definitions
typedef enum server_error {

    ERROR_USER_DOESNT_EXISTS = -1,
    ERROR_USER_ALREADY_EXISTS = -2, 
    ERROR_USER_BAD_CREDENTIALS = -3,

	ERROR_RENT_FILM_MAX_ALLOWED = -4,
	ERROR_RENT_FILM_NO_AVAILABLE_COPY = -5,
	ERROR_RENT_ALREADY_EXISTS = -6,
	
	ERROR_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT = -7,

	ERROR_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE = -8,
	ERROR_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED = -9,

	ERROR_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE = -10

} server_error_t;

typedef struct connection_data {

	unsigned int user_id;
	pthread_t server_thread_tid;
	pid_t client_pid;
	int client_socket_fd;

} connection_data_t;

//data types
typedef struct user_t {

	unsigned int id;
	char username[MAX_USER_USERNAME_SIZE];
	char password[MAX_USER_PASSWORD_SIZE];

} user_t;

typedef struct film_t {

	unsigned int id;
	char title[MAX_FILM_TITLE_SIZE];
	int available_copies;
	int rented_out_copies;

} film_t;

typedef struct reservation_t {

	unsigned int id;
	time_t rental_date;
	time_t expiring_date;
	time_t due_date;
	unsigned int user_id;
	unsigned int film_id;

} reservation_t; 

typedef struct connection_list_t {

	connection_data_t* connections[MAX_CLIENTS];
	int dim;
	int in_idx;
	int out_idx;
	pthread_mutex_t connections_mutex;

} connection_list_t;

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

//globals
connection_list_t *connection_list;
user_list_t *user_list;
film_list_t *film_list;
reservation_list_t *reservation_list;
sqlite3 *database;

int shopkeeper_max_rented_films_per_user = DEFAULT_MAX_RENTED_FILMS;
pthread_mutex_t shopkeeper_max_rented_films_mutex = PTHREAD_MUTEX_INITIALIZER;

//init global lists
connection_list_t* init_connection_list();
user_list_t* init_user_list();
film_list_t* init_film_list();
reservation_list_t* init_reservation_list();

//init shopkeeper
void init_shopkeeper(sqlite3* database);

//aggiunte alle liste globali
connection_data_t* add_connection_to_connection_list(unsigned int user_id, pthread_t server_thread_tid, pid_t client_pid, int client_socket_fd);
user_t* add_user_to_user_list(unsigned int id, char *username, char *password);
film_t* add_film_to_film_list(unsigned int id, char *title, int available_copies, int rented_out_copies);
reservation_t* add_reservation_to_reservation_list(unsigned int id, time_t rental_date, time_t expiring_date, time_t due_date, unsigned int user_id, unsigned int film_id);

film_t* increment_film_available_copy_and_decrement_film_rented_out_copy(unsigned int film_id);
film_t* decrement_film_available_copy_and_increment_film_rented_out_copy(unsigned int film_id);
void update_connection_user_id(int new_user_id, pid_t client_pid);
void update_reservation_due_date(unsigned int reservation_id, time_t reservation_due_date);

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
int database_reservation_insert(sqlite3* database, time_t reservation_rental_date, time_t reservation_expiring_date, unsigned int reservation_user_id, unsigned int film_id);

//database update
void database_film_add_rented_out_copy(sqlite3* database, unsigned int film_id);
void database_film_remove_rented_out_copy(sqlite3* database, unsigned int film_id);
void database_film_add_available_copy(sqlite3* database, unsigned int film_id);
void database_film_remove_available_copy(sqlite3* database, unsigned int film_id);
int database_reservation_add_due_date(sqlite3* database, unsigned int user_id, unsigned int film_id, time_t *now_date);

//use cases
int create_new_user(sqlite3* database, char *user_username, char *user_password);
int login(sqlite3* database, char *user_username, char *user_password);
void create_new_film(sqlite3* database, char *film_title, int film_available_copies);
void create_new_reservation(sqlite3* database, unsigned int reservation_user_id, unsigned int reservation_film_id);
void send_all_films_to_client(int client_socket);
void get_rented_user_films(int client_socket, unsigned int user_id);
void send_all_user_expired_films_with_no_due_date(int client_socket, unsigned int user_id);
int rent_film(sqlite3* database, unsigned int user_id, unsigned int film_id);
int return_rented_film(sqlite3* database, unsigned int user_id, unsigned int film_id);
int get_max_rented_films();
film_list_t* get_all_rented_user_films(unsigned int user_id);
film_list_t* get_all_user_expired_films_with_no_due_date(unsigned int user_id);
int shopkeeper_change_max_rented_films(unsigned int shopkeeper_id, int new_max_rented_films);
int shopkeeper_notify_expired_films(unsigned int shopkeeper_id);

//ausiliari
int check_user_already_exists(char *user_username);
int check_user_does_not_exists(char *user_username);
int check_film_available_copies_less_than_or_equal_zero(unsigned int film_id);
int check_film_rented_out_copies_less_than_or_equal_zero(unsigned int film_id);
int check_user_already_rent_film(unsigned int user_id, unsigned int film_id);
int check_all_users_exceed_max_rented_film(int new_max_rented_films);
int check_user_exceed_max_rented_film(unsigned int new_max_rented_films);
int check_is_not_shopkeeper(unsigned int user_id);

connection_data_t* search_connection_by_client_pid(pid_t client_pid);
user_t* search_user_by_id(unsigned int user_id);
user_t* search_user_by_username(char *user_username);
user_t* search_user_by_username_and_password(char *user_username, char *user_password);
film_t* search_film_by_id(unsigned int film_id);
reservation_t* search_reservation_by_id(unsigned int reservation_id);

//threads
void* connection_handler(void* arg);

//miscellous
void error_handler(char *message);

int main(){
	
	int server_socket;

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
	connection_list = init_connection_list();

	database_user_list_sync(database);
	database_film_list_sync(database);
	database_reservation_list_sync(database);

	printf("\n[SERVER] Dati sincronizzati dal database.\n");

	init_shopkeeper(database);

	printf("\n[SERVER] Account negoziante creato/recuperato dal database con successo.\n");

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

	if(listen(server_socket, MAX_CLIENTS) < 0)
		error_handler("[SERVER] Errore listen socket");

	printf("\n[SERVER] Successo socket listen.\n");

	while(1){

		int* client_socket = (int*)malloc(sizeof(int));

		if(client_socket == NULL)
			error_handler("[SERVER] Errore allocazione dinamica");

		if((*client_socket = accept(server_socket, (struct sockaddr *) &server_address, &server_address_len)) < 0){
			free(client_socket);
			error_handler("[SERVER] Errore accept socket");
		}

		printf("\n[SERVER] Ricevuta connessione di un client.\n");

		pthread_t tid;
		if(pthread_create(&tid, NULL, connection_handler, (void *)client_socket) < 0){
			free(client_socket);
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

	pid_t client_pid;
	if(read(client_socket, &client_pid, sizeof(client_pid)) < 0){
		close(client_socket);
		error_handler("[SERVER] Errore lettura CLIENT pid");
	}

	printf("\n[SERVER] Ottenuto il pid %d del client.\n", client_pid);

	add_connection_to_connection_list(0, pthread_self(), client_pid, client_socket);

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

			char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
			strcpy(success_message, SUCCESS_REGISTER);
			
			char error_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

			int result = create_new_user(database, user_username, user_password);

			if(result > 0){

				if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore server write");
				}

			} else if (result == ERROR_USER_ALREADY_EXISTS){

				strcpy(error_message, FAILED_USER_ALREDY_EXISTS);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore server write");
				}
			}

		} else if (strncmp(protocol_message, LOGIN_PROTOCOL_MESSAGE, strlen(LOGIN_PROTOCOL_MESSAGE)) == 0){

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

			int user_id = login(database, user_username, user_password);

			char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
			strcpy(success_message, SUCCESS_LOGIN);

			char error_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

			if(user_id > 0){

				if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
				}

				if(write(client_socket, &user_id, sizeof(user_id)) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura USER id");
				}

				update_connection_user_id(user_id, client_pid);

			} else if (user_id == ERROR_USER_DOESNT_EXISTS){

				strcpy(error_message, FAILED_USER_DOESNT_EXISTS);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_USER_DOESNT_EXISTS protocol message");
				}

			} else if (user_id == ERROR_USER_BAD_CREDENTIALS){

				strcpy(error_message, FAILED_USER_BAD_CREDENTIALS);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_USER_BAD_CREDENTIALS protocol message");
				}

			}

		} else if (strncmp(protocol_message, GET_FILMS_PROTOCOL_MESSAGE, strlen(GET_FILMS_PROTOCOL_MESSAGE)) == 0){

			printf("\n[SERVER] Ricevuta richiesta tutti i film.\n");

			send_all_films_to_client(client_socket);

		} else if (strncmp(protocol_message, GET_USER_RENTED_FILMS_PROTOCOL_MESSAGE, strlen(GET_USER_RENTED_FILMS_PROTOCOL_MESSAGE)) == 0){

			unsigned int user_id;
			if(read(client_socket, &user_id, sizeof(user_id)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura USER id da socket");
			}

			printf("\n[SERVER] Ricevuta richiesta tutti i film attualmente noleggiati USER.\n");

			get_rented_user_films(client_socket, user_id);

		} else if (strncmp(protocol_message, RENT_FILM_PROTOCOL_MESSAGE, strlen(RENT_FILM_PROTOCOL_MESSAGE)) == 0){

			unsigned int user_id;
			unsigned int film_id;

			if(read(client_socket, &user_id, sizeof(user_id)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura USER id da socket");
			}

			if(read(client_socket, &film_id, sizeof(film_id)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura FILM id da socket");
			}

			char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
			strcpy(success_message, SUCCESS_RENT_FILM);

			char error_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

			int result = rent_film(database, user_id, film_id);

			if(result > 0){

				if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
				}

			} else if (result == ERROR_RENT_ALREADY_EXISTS){

				strcpy(error_message, FAILED_RENT_ALREADY_EXISTS);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_RENT_ALREADY_EXISTS protocol message");
				}

			} else if (result == ERROR_RENT_FILM_NO_AVAILABLE_COPY){

				strcpy(error_message, FAILED_RENT_FILM_NO_AVAILABLE_COPY);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_RENT_FILM_NO_AVAILABLE_COPY protocol message");
				}

			} else if (result == ERROR_RENT_FILM_MAX_ALLOWED){

				strcpy(error_message, FAILED_RENT_FILM_MAX_ALLOWED);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_RENT_FILM_MAX_ALLOWED protocol message");
				}
			}

		} else if (strncmp(protocol_message, RETURN_RENTED_FILM_PROTOCOL_MESSAGE, strlen(RETURN_RENTED_FILM_PROTOCOL_MESSAGE)) == 0){

			unsigned int user_id;
			unsigned int film_id;

			if(read(client_socket, &user_id, sizeof(user_id)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura USER id da socket");
			}

			if(read(client_socket, &film_id, sizeof(film_id)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura FILM id da socket");
			}

			char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
			strcpy(success_message, SUCCESS_RETURN_RENTED_FILM);

			char error_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

			int result = return_rented_film(database, user_id, film_id);

			if(result > 0){

				if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
				}

			} else if (result == ERROR_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT){

				strcpy(error_message, FAILED_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT protocol message");
				}

			}

		} else if (strncmp(protocol_message, GET_MAX_RENTED_FILMS_PROTOCOL_MESSAGE, strlen(GET_MAX_RENTED_FILMS_PROTOCOL_MESSAGE)) == 0){

			char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
			strcpy(success_message, SUCCESS_GET_MAX_RENTED_FILMS);

			int max_rented_films = get_max_rented_films();

			if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
			}

			if(write(client_socket, &max_rented_films, sizeof(int)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore scrittura MAX_RENTED_FILMS");
			}
			
		} else if (strncmp(protocol_message, GET_USER_EXIRED_FILMS_NO_DUE_DATE_PROTOCOL_MESSAGE, strlen(GET_USER_EXIRED_FILMS_NO_DUE_DATE_PROTOCOL_MESSAGE)) == 0){

			unsigned int user_id;
			if(read(client_socket, &user_id, sizeof(user_id)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura USER id da socket");
			}

			printf("\n[SERVER] Ricevuta richiesta tutti i film scaduti per uno USER.\n");

			send_all_user_expired_films_with_no_due_date(client_socket, user_id);

		} else if (strncmp(protocol_message, SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_PROTOCOL_MESSAGE, strlen(SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_PROTOCOL_MESSAGE)) == 0){

			unsigned int shopkeeper_id;
			int new_max_rented_films;

			if(read(client_socket, &shopkeeper_id, sizeof(shopkeeper_id)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura SHOPKEEPER(USER) id da socket");
			}

			if(read(client_socket, &new_max_rented_films, sizeof(new_max_rented_films)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura NEW_MAX_RENTED_FILMS da socket");
			}

			char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
			strcpy(success_message, SUCCESS_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS);

			char error_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

			int result = shopkeeper_change_max_rented_films(shopkeeper_id, new_max_rented_films);

			if(result > 0){

				if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
				}

			} else if (result == ERROR_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE){

				strcpy(error_message, FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE protocol message");
				}

			} else if (result == ERROR_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED){

				strcpy(error_message, FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED protocol message");
				}
			}

		} else if (strncmp(protocol_message, SHOPKEEPER_NOTIFY_EXPIRED_FILMS_PROTOCOL_MESSAGE, strlen(SHOPKEEPER_NOTIFY_EXPIRED_FILMS_PROTOCOL_MESSAGE)) == 0){

			unsigned int shopkeeper_id;

			if(read(client_socket, &shopkeeper_id, sizeof(shopkeeper_id)) < 0){
				close(client_socket);
				error_handler("[SERVER] Errore lettura SHOPKEEPER(USER) id da socket");
			}

			char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
			strcpy(success_message, SUCCESS_SHOPKEEPER_NOTIFY_EXPIRED_FILMS);

			char error_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};

			int result = shopkeeper_notify_expired_films(shopkeeper_id);

			if(result > 0){

				if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
				}

			} else if (result == ERROR_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE){

				strcpy(error_message, FAILED_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE);

				if(write(client_socket, error_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
					close(client_socket);
					error_handler("[SERVER] Errore scrittura FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE protocol message");
				}

			}
		
		}
	}

	close(client_socket);

	pthread_exit(NULL);
}

//gestione aggiunte liste
connection_data_t* add_connection_to_connection_list(unsigned int user_id, pthread_t server_thread_tid, pid_t client_pid, int client_socket_fd){

	pthread_mutex_lock(&connection_list->connections_mutex);

	if(connection_list->dim >= MAX_CLIENTS){
		pthread_mutex_unlock(&connection_list->connections_mutex);
		printf("\n[SERVER] Raggiunto il numero massimo di CLIENT supportati, inserimento ignorato.\n");
		return NULL;
	}

	connection_data_t *connection_to_insert = (connection_data_t *)malloc(sizeof(connection_data_t));
	if(connection_to_insert == NULL){
		pthread_mutex_unlock(&connection_list->connections_mutex);
		error_handler("[SERVER] Errore allocazione dinamica inserimento CONNECTION");
	}

	connection_to_insert->user_id = user_id;
	connection_to_insert->server_thread_tid = server_thread_tid;
	connection_to_insert->client_pid = client_pid;
	connection_to_insert->client_socket_fd = client_socket_fd;

	connection_list->connections[connection_list->in_idx] = connection_to_insert;
	connection_list->in_idx = (connection_list->in_idx + 1) % MAX_CLIENTS;
	connection_list->dim = connection_list->dim + 1;

	pthread_mutex_unlock(&connection_list->connections_mutex);

	return connection_to_insert;
}

user_t* add_user_to_user_list(unsigned int id, char *username, char *password){

	if(user_list->dim >= MAX_USERS){
		printf("\n[SERVER] Raggiunto il numero massimo di USER supportati, inserimento ignorato.\n");
		return NULL;
	}

	user_t *user_to_insert = (user_t *)malloc(sizeof(user_t));
	if(user_to_insert == NULL){
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

	return user_to_insert;
}

film_t* add_film_to_film_list(unsigned int id, char *title, int available_copies, int rented_out_copies){

	if(film_list->dim >= MAX_FILMS){
		printf("\n[SERVER] Raggiunto il numero massimo di FILM supportati, inserimento ignorato.\n");
		return NULL;
	}

	film_t *film_to_insert = (film_t *)malloc(sizeof(film_t));
	if(film_to_insert == NULL){
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

	return film_to_insert;	
}

//init shopkeeper
void init_shopkeeper(sqlite3* database){

	pthread_mutex_lock(&user_list->users_mutex);

	user_t* shopkeeper = search_user_by_username(SHOPKEEPER_USERNAME);

	pthread_mutex_unlock(&user_list->users_mutex);

	if(shopkeeper == NULL)
		create_new_user(database, SHOPKEEPER_USERNAME, SHOPKEEPER_PASSWORD);
}

reservation_t* add_reservation_to_reservation_list(unsigned int id, time_t rental_date, time_t expiring_date, time_t due_date, unsigned int user_id, unsigned int film_id){

	if(reservation_list->dim >= MAX_RESERVATIONS){
		printf("\n[SERVER] Raggiunto il numero massimo di RESERVATION supportate, inserimento ignorato.\n");
		return NULL;
	}

	reservation_t *reservation_to_insert = (reservation_t *)malloc(sizeof(reservation_t));
	if(reservation_to_insert == NULL){
		error_handler("[SERVER] Errore allocazione dinamica inserimento RESERVATION");
	}

	// ??? SI DOVREBBE CONTROLLARE E NEL CASO SEGNALARE ERRORE L'ESISTENZA DI USER E FILM CON ID PARAMETRO ???
	reservation_to_insert->id = id;
	reservation_to_insert->rental_date = rental_date;
	reservation_to_insert->expiring_date = expiring_date;
	reservation_to_insert->due_date = due_date;
	reservation_to_insert->user_id = user_id;
	reservation_to_insert->film_id = film_id;

	reservation_list->reservations[reservation_list->in_idx] = reservation_to_insert;
	reservation_list->in_idx = (reservation_list->in_idx + 1) % MAX_RESERVATIONS;
	reservation_list->dim = reservation_list->dim + 1;

	return reservation_to_insert;
}

film_t* increment_film_available_copy_and_decrement_film_rented_out_copy(unsigned int film_id){

	film_t* film_to_update = search_film_by_id(film_id);

	if(film_to_update != NULL && film_to_update->rented_out_copies > 0){
		
		film_to_update->available_copies++;
		film_to_update->rented_out_copies--;

	} else film_to_update = NULL;

	return film_to_update;
}

film_t* decrement_film_available_copy_and_increment_film_rented_out_copy(unsigned int film_id){

	film_t* film_to_update = search_film_by_id(film_id);

	if(film_to_update != NULL && film_to_update->available_copies > 0){
		
		film_to_update->available_copies--;
		film_to_update->rented_out_copies++;

	} else film_to_update = NULL;

	return film_to_update;
}

void update_connection_user_id(int new_user_id, pid_t client_pid){
	pthread_mutex_lock(&connection_list->connections_mutex);

	connection_data_t* connection_to_update = search_connection_by_client_pid(client_pid);

	if(connection_to_update != NULL){
		connection_to_update->user_id = new_user_id;
	}

	pthread_mutex_unlock(&connection_list->connections_mutex);
}

void update_reservation_due_date(unsigned int reservation_id, time_t reservation_due_date){

	reservation_t* reservation_to_update = search_reservation_by_id(reservation_id);

	if(reservation_to_update != NULL){
		reservation_to_update->due_date = reservation_due_date;
	}
}

//init global data list
connection_list_t* init_connection_list(){

	connection_list_t *list = (connection_list_t *)malloc(sizeof(connection_list_t));
	if(list == NULL)
		error_handler("[SERVER] Errore di allocazione dinamica connections list");
	
	list->dim = 0;
	list->in_idx = 0;
	list->out_idx = 0;

	if(pthread_mutex_init(&list->connections_mutex, NULL) != 0)
		error_handler("[SERVER] Errore di allocazione dinamica connection list mutex");

	return list;
}

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

	char exe_path[MAX_PATH];
    char db_full_path[MAX_PATH];


    //Prende come riferimento l'eseguibile""
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    
    if (len != -1) {
        exe_path[len] = '\0';
        //Ottiene la cartella dell'eseguibile
        char *dir = dirname(exe_path);
        //Costruisce il percorso completo del database
        snprintf(db_full_path, sizeof(db_full_path), "%s/database.db", dir);
    } else {
        // Fallback di emergenza se readlink fallisce
        strcpy(db_full_path, "database.db");
    }

    if (sqlite3_open(db_full_path, database) != SQLITE_OK) {
        fprintf(stderr, "[SERVER] Errore critico apertura SQLite: %s\n", sqlite3_errmsg(*database));
        exit(-1);
    }
    
    printf("[SERVER] Database aperto con successo in: %s\n", db_full_path);

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
		"expiring_date INTEGER NOT NULL, "
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

		unsigned int user_id = (unsigned int)sqlite3_column_int64(prepared, 0);
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

		unsigned int film_id = (unsigned int)sqlite3_column_int64(prepared, 0);
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

		unsigned int reservation_id = (unsigned int)sqlite3_column_int64(prepared, 0);
		time_t reservation_rental_date = sqlite3_column_int64(prepared, 1);
		time_t reservation_expiring_date = sqlite3_column_int64(prepared, 2);
		time_t reservation_due_date = sqlite3_column_int64(prepared, 3);
		unsigned int reservation_user_id = (unsigned int)sqlite3_column_int64(prepared, 4);
		unsigned int reservation_film_id = (unsigned int)sqlite3_column_int64(prepared, 5);

		if(add_reservation_to_reservation_list(reservation_id, reservation_rental_date, reservation_expiring_date, reservation_due_date, reservation_user_id, reservation_film_id) == NULL){
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

int database_reservation_insert(sqlite3* database, time_t reservation_rental_date, time_t reservation_expiring_date, unsigned int reservation_user_id, unsigned int reservation_film_id){
	
	sqlite3_stmt* prepared;
	const char *statement_sql = "INSERT INTO RESERVATION(rental_date, expiring_date, user_id, film_id) VALUES (?,?,?,?);";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore inserimento RESERVATION table sqlite");
	}

	int reservation_id = -1;
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)reservation_rental_date);
	sqlite3_bind_int64(prepared, 2, (sqlite3_int64)reservation_expiring_date);
	sqlite3_bind_int64(prepared, 3, (sqlite3_int64)reservation_user_id);
	sqlite3_bind_int64(prepared, 4, (sqlite3_int64)reservation_film_id);

	if(sqlite3_step(prepared) == SQLITE_DONE){
		reservation_id = (int)sqlite3_last_insert_rowid(database);
		printf("\n[SERVER] Inserito RESERVATION(%lld, %lld, %d, %d).\n", (long long)reservation_rental_date, (long long)reservation_expiring_date, reservation_user_id, reservation_film_id);
	} else {
		sqlite3_close(database);
		printf("[SERVER-DB] Step error: %s\n", sqlite3_errmsg(database));
		error_handler("[SERVER] Errore inserimento RESERVATION table sqlite");
	}
	
	sqlite3_finalize(prepared);
	return reservation_id;
}

//update
void database_film_add_rented_out_copy(sqlite3* database, unsigned int film_id) {

	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE FILM SET rented_out_copies = rented_out_copies + 1 WHERE id = ?;";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore update FILM table sqlite");
	}
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)film_id);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Aggiunta al FILM(%u) +1 copia noleggiata.\n", film_id);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore aggiornamento FILM table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

void database_film_remove_rented_out_copy(sqlite3* database, unsigned int film_id) {

	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE FILM SET rented_out_copies = rented_out_copies - 1 WHERE id = ?;";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore update FILM table sqlite");
	}
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)film_id);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Aggiunta al FILM(%u) -1 copia noleggiata.\n", film_id);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore aggiornamento FILM table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

void database_film_add_available_copy(sqlite3* database, unsigned int film_id){

	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE FILM SET available_copies = available_copies + 1 WHERE id = ?;";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore update FILM table sqlite");
	}
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)film_id);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Aggiunta al FILM(%u) +1 copia disponibile.\n", film_id);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore aggiornamento FILM table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

void database_film_remove_available_copy(sqlite3* database, unsigned int film_id){
	
	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE FILM SET available_copies = available_copies - 1 WHERE id = ?;";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore update FILM table sqlite");
	}
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)film_id);
	
	if(sqlite3_step(prepared) == SQLITE_DONE){
		printf("\n[SERVER] Aggiunta al FILM(%u) -1 copia disponibile.\n", film_id);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore aggiornamento FILM table sqlite");
	}
	
	sqlite3_finalize(prepared);
}

int database_reservation_add_due_date(sqlite3* database, unsigned int user_id, unsigned int film_id, time_t *now_date) {
	
	sqlite3_stmt* prepared;
	const char *statement_sql = "UPDATE RESERVATION SET due_date = ? WHERE user_id = ? AND film_id = ? RETURNING id;";
	
	if(sqlite3_prepare_v2(database, statement_sql, -1, &prepared, NULL) != SQLITE_OK){
		sqlite3_close(database);
		error_handler("[SERVER] Errore update RESERVATION table sqlite");
	}
	
	int reservation_id = -1;

	//calcolo della data esatta di adesso
	*now_date = time(NULL);
	
	sqlite3_bind_int64(prepared, 1, (sqlite3_int64)*now_date);
	sqlite3_bind_int64(prepared, 2, (sqlite3_int64)user_id);
	sqlite3_bind_int64(prepared, 3, (sqlite3_int64)film_id);

	int step_result = sqlite3_step(prepared);
	
	if(step_result == SQLITE_ROW){
		reservation_id = sqlite3_column_int(prepared, 0);
		printf("\n[SERVER] Aggiunta alla RESERVATION(user_id=%u, film_id=%u) due_date = %lld.\n", user_id, film_id, (long long)*now_date);
	} else {
		sqlite3_close(database);
		error_handler("[SERVER] Errore aggiornamento RESERVATION table sqlite");
	}
	
	sqlite3_finalize(prepared);

	return reservation_id;
}

//wrapper per aggiunta in memory + database
int create_new_user(sqlite3* database, char *user_username, char *user_password){

	pthread_mutex_lock(&user_list->users_mutex);

	if(check_user_already_exists(user_username)){
		pthread_mutex_unlock(&user_list->users_mutex);
		return ERROR_USER_ALREADY_EXISTS;
	}

	int user_id_result = database_user_insert(database, user_username, user_password);

	if(user_id_result == -1){
		pthread_mutex_unlock(&user_list->users_mutex);
		error_handler("[SERVER] Errore creazione nuovo USER");
	}

	unsigned int user_id = (unsigned int)user_id_result;

	if(add_user_to_user_list(user_id, user_username, user_password) != NULL){
		printf("\n[SERVER] Nuovo USER(%u, %s, %s) sincronizzato in memoria.\n", user_id, user_username, user_password);
	} else {
		pthread_mutex_unlock(&user_list->users_mutex);
		error_handler("[SERVER] USER salvato su database ma lista piena in memoria. L'USER esiste ma non è in memoria");
	}

	pthread_mutex_unlock(&user_list->users_mutex);

	return (int)user_id;
}

void create_new_film(sqlite3* database, char *film_title, int film_available_copies){

	pthread_mutex_lock(&film_list->films_mutex);

	int film_id_result = database_film_insert(database, film_title, film_available_copies);

	if(film_id_result == -1){
		pthread_mutex_unlock(&film_list->films_mutex);
		error_handler("[SERVER] Errore creazione nuovo FILM");
	}

	unsigned int film_id = (unsigned int)film_id_result;

	if(add_film_to_film_list(film_id, film_title, film_available_copies, 0) != NULL){
		printf("\n[SERVER] Nuovo FILM(%u, %s, %d) sincronizzato in memoria.\n", film_id, film_title, film_available_copies);
	} else {
		pthread_mutex_unlock(&film_list->films_mutex);
		error_handler("[SERVER] FILM salvato sul database ma lista piena in memoria. Il FILM esiste ma non è in memoria");
	}

	pthread_mutex_unlock(&film_list->films_mutex);
}

void create_new_reservation(sqlite3* database, unsigned int reservation_user_id, unsigned int reservation_film_id){

	time_t reservation_rental_date = time(NULL);
	//time_t reservation_expiring_date = reservation_rental_date + (RENTAL_DURATION_DAYS * SECONDS_IN_DAY);
	//SOLO TEST ABILITARE SOPRA
	time_t reservation_expiring_date = reservation_rental_date + 10;
	int reservation_id_result = database_reservation_insert(database, reservation_rental_date, reservation_expiring_date, reservation_user_id, reservation_film_id);

	if(reservation_id_result == -1){
		error_handler("[SERVER] Errore creazione nuova RESERVATION");
	}

	unsigned int reservation_id = (unsigned int)reservation_id_result;

	if(add_reservation_to_reservation_list(reservation_id, reservation_rental_date, reservation_expiring_date, 0, reservation_user_id, reservation_film_id) != NULL){
		printf("\n[SERVER] Nuova RESERVATION(%u, %lld, %lld, %u, %u) sincronizzata in memoria.\n", reservation_id, (long long)reservation_rental_date, (long long)reservation_expiring_date, reservation_user_id, reservation_film_id);
	} else {
		error_handler("[SERVER] RESERVATION salvata sul database ma lista piena in memoria. La RESERVATION esiste ma non è in memoria");
	}

}

int login(sqlite3* database, char *user_username, char *user_password){

	pthread_mutex_lock(&user_list->users_mutex);

	if(check_user_does_not_exists(user_username)){
		pthread_mutex_unlock(&user_list->users_mutex);
		return ERROR_USER_DOESNT_EXISTS;
	}

	user_t* logged_user = search_user_by_username_and_password(user_username, user_password);

	if(logged_user == NULL){
		pthread_mutex_unlock(&user_list->users_mutex);
		return ERROR_USER_BAD_CREDENTIALS;
	}

	pthread_mutex_unlock(&user_list->users_mutex);
	
	return (int)logged_user->id;
}

film_list_t* get_all_user_expired_films_with_no_due_date(unsigned int user_id){

	time_t now = time(NULL);

	int user_film_ids[MAX_FILMS];
	int dim = 0;

	for(int i = 0; i < reservation_list->dim; i++){

		reservation_t* current_reservation = reservation_list->reservations[i];

		if(
			current_reservation->user_id == user_id
			&&
			current_reservation->expiring_date < now
			&&
			current_reservation->due_date == 0
		){
			if(dim < MAX_FILMS){
				user_film_ids[dim] = current_reservation->film_id;
				dim++;
			} else {
				printf("\n[SERVER] Trovati più film scaduti dello USER %u del limite massimo gestibile.\n", user_id);
				return NULL;
			}
		}
	}

	film_list_t* user_films = init_film_list();
	if(user_films == NULL){
		printf("\n[SERVER] Errore recupero film da restituire dell'utente %u.\n", user_id);
		return NULL;
	}

	for(int i = 0; i < dim; i++){

		film_t* film = search_film_by_id(user_film_ids[i]);

		if(film == NULL || user_films->dim >= MAX_FILMS){

			for(int j = 0; j < user_films->dim; j++){
				free(user_films->films[j]);
			}

			pthread_mutex_destroy(&user_films->films_mutex);
			free(user_films);

			printf("\n[SERVER] Raggiunto il numero massimo di FILM supportati per il recupero di film scaduti dell'utente %u.\n", user_id);
			return NULL;
		}

		film_t *film_to_insert = (film_t *)malloc(sizeof(film_t));
		if(film_to_insert == NULL){

			for(int j = 0; j < user_films->dim; j++){
				free(user_films->films[j]);
			}

			pthread_mutex_destroy(&user_films->films_mutex);
			free(user_films);

			printf("\n[SERVER] Errore allocazione dinamica inserimento FILM.\n");
			return NULL;
		}

		film_to_insert->id = film->id;

		strncpy(film_to_insert->title, film->title, MAX_FILM_TITLE_SIZE - 1);
		film_to_insert->title[MAX_FILM_TITLE_SIZE - 1] = '\0';

		film_to_insert->available_copies = film->available_copies;

		film_to_insert->rented_out_copies = film->rented_out_copies;

		user_films->films[user_films->in_idx] = film_to_insert;
		user_films->in_idx = (user_films->in_idx + 1) % MAX_FILMS;
		user_films->dim = user_films->dim + 1;
	}

	return user_films;
}

film_list_t* get_all_rented_user_films(unsigned int user_id){

	int user_film_ids[MAX_FILMS];
	int dim = 0;

	for(int i = 0; i < reservation_list->dim; i++){

		reservation_t* current_reservation = reservation_list->reservations[i];

		if(
			current_reservation->user_id == user_id
			&&
			current_reservation->due_date == 0
		){
			if(dim < MAX_FILMS){
				user_film_ids[dim] = current_reservation->film_id;
				dim++;
			} else {
				printf("\n[SERVER] Trovati più film scaduti dello USER %u del limite massimo gestibile.\n", user_id);
				return NULL;
			}
		}
	}

	film_list_t* user_films = init_film_list();
	if(user_films == NULL){
		printf("\n[SERVER] Errore recupero film da restituire dell'utente %u.\n", user_id);
		return NULL;
	}

	for(int i = 0; i < dim; i++){

		film_t* film = search_film_by_id(user_film_ids[i]);

		if(film == NULL || user_films->dim >= MAX_FILMS){

			for(int j = 0; j < user_films->dim; j++){
				free(user_films->films[j]);
			}

			pthread_mutex_destroy(&user_films->films_mutex);
			free(user_films);

			printf("\n[SERVER] Raggiunto il numero massimo di FILM supportati per il recupero di film scaduti dell'utente %u.\n", user_id);
			return NULL;
		}

		film_t *film_to_insert = (film_t *)malloc(sizeof(film_t));
		if(film_to_insert == NULL){

			for(int j = 0; j < user_films->dim; j++){
				free(user_films->films[j]);
			}

			pthread_mutex_destroy(&user_films->films_mutex);
			free(user_films);

			printf("\n[SERVER] Errore allocazione dinamica inserimento FILM.\n");
			return NULL;
		}

		film_to_insert->id = film->id;

		strncpy(film_to_insert->title, film->title, MAX_FILM_TITLE_SIZE - 1);
		film_to_insert->title[MAX_FILM_TITLE_SIZE - 1] = '\0';

		film_to_insert->available_copies = film->available_copies;

		film_to_insert->rented_out_copies = film->rented_out_copies;

		user_films->films[user_films->in_idx] = film_to_insert;
		user_films->in_idx = (user_films->in_idx + 1) % MAX_FILMS;
		user_films->dim = user_films->dim + 1;
	}

	return user_films;
}

void send_all_user_expired_films_with_no_due_date(int client_socket, unsigned int user_id){

	pthread_mutex_lock(&film_list->films_mutex);
	pthread_mutex_lock(&reservation_list->reservations_mutex);

	char film_title[MAX_FILM_TITLE_SIZE] = {0};

	char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(success_message, SUCCESS_GET_USER_EXIRED_FILMS_NO_DUE_DATE);

	if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		close(client_socket);
		error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
	}

	film_list_t* user_expired_films_with_no_due_date = get_all_user_expired_films_with_no_due_date(user_id);

	 //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	film_list_t* list = user_expired_films_with_no_due_date;
	 for(int i = 0; i < list->dim; i++){
		printf("%-3d %-30s\n",
			list->films[i]->id,
			list->films[i]->title);
	 }
	 //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	if(user_expired_films_with_no_due_date == NULL){
		close(client_socket);
		error_handler("[SERVER] Errore recupero film dell'utente da restituire");
	}

	int films_dim = user_expired_films_with_no_due_date->dim;
	if(write(client_socket, &films_dim, sizeof(films_dim)) < 0){
		close(client_socket);
		error_handler("[SERVER] Errore scrittura user_expired_films_with_no_due_date dim");
	}

	for(int i = 0; i < user_expired_films_with_no_due_date->dim; i++){

		unsigned int film_id = user_expired_films_with_no_due_date->films[i]->id;
		strcpy(film_title, user_expired_films_with_no_due_date->films[i]->title);

		if(write(client_socket, &film_id, sizeof(film_id)) < 0){
			close(client_socket);
			error_handler("[SERVER] Errore scrittura FILM id");
		}

		if(write(client_socket, film_title, MAX_FILM_TITLE_SIZE) < 0){
			close(client_socket);
			error_handler("[SERVER] Errore scrittura FILM title");
		}

	}

	pthread_mutex_destroy(&user_expired_films_with_no_due_date->films_mutex);

	for(int i = 0; i < user_expired_films_with_no_due_date->dim; i++){
        free(user_expired_films_with_no_due_date->films[i]);
    }
    free(user_expired_films_with_no_due_date);

	pthread_mutex_unlock(&reservation_list->reservations_mutex);
	pthread_mutex_unlock(&film_list->films_mutex);
}

void send_all_films_to_client(int client_socket){

	pthread_mutex_lock(&film_list->films_mutex);

	char film_title[MAX_FILM_TITLE_SIZE] = {0};

	char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(success_message, SUCCESS_GET_FILMS);

	if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		close(client_socket);
		error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
	}

	int films_dim = film_list->dim;
	if(write(client_socket, &films_dim, sizeof(films_dim)) < 0){
		close(client_socket);
		error_handler("[SERVER] Errore scrittura FILM dim");
	}

	for(int i = 0; i < film_list->dim; i++){

		unsigned int film_id = film_list->films[i]->id;
		strcpy(film_title, film_list->films[i]->title);
		int film_available_copies = film_list->films[i]->available_copies;
		int film_rented_out_copies = film_list->films[i]->rented_out_copies;

		if(write(client_socket, &film_id, sizeof(film_id)) < 0){
			close(client_socket);
			error_handler("[SERVER] Errore scrittura FILM id");
		}

		if(write(client_socket, film_title, MAX_FILM_TITLE_SIZE) < 0){
			close(client_socket);
			error_handler("[SERVER] Errore scrittura FILM title");
		}

		if(write(client_socket, &film_available_copies, sizeof(film_available_copies)) < 0){
			close(client_socket);
			error_handler("[SERVER] Errore scrittura FILM available_copies");
		}

		if(write(client_socket, &film_rented_out_copies, sizeof(film_rented_out_copies)) < 0){
			close(client_socket);
			error_handler("[SERVER] Errore scrittura FILM rented_out_copies");
		}
	}

	pthread_mutex_unlock(&film_list->films_mutex);
}

void get_rented_user_films(int client_socket, unsigned int user_id){

	pthread_mutex_lock(&film_list->films_mutex);
	pthread_mutex_lock(&reservation_list->reservations_mutex);

	char film_title[MAX_FILM_TITLE_SIZE] = {0};

	char success_message[PROTOCOL_MESSAGE_MAX_SIZE] = {0};
	strcpy(success_message, SUCCESS_GET_USER_RENTED_FILMS);

	if(write(client_socket, success_message, PROTOCOL_MESSAGE_MAX_SIZE) < 0){
		close(client_socket);
		error_handler("[SERVER] Errore scrittura SUCCESS protocol message");
	}

	film_list_t* user_rented_films = get_all_rented_user_films(user_id);

	if(user_rented_films == NULL){
		close(client_socket);
		error_handler("[SERVER] Errore recupero film noleggiati dell'utente da restituire");
	}

	int films_dim = user_rented_films->dim;
	if(write(client_socket, &films_dim, sizeof(films_dim)) < 0){
		close(client_socket);
		error_handler("[SERVER] Errore scrittura user_rented_films dim");
	}

	for(int i = 0; i < user_rented_films->dim; i++){

		unsigned int film_id = user_rented_films->films[i]->id;
		strcpy(film_title, user_rented_films->films[i]->title);

		if(write(client_socket, &film_id, sizeof(film_id)) < 0){
			close(client_socket);
			error_handler("[SERVER] Errore scrittura FILM id");
		}

		if(write(client_socket, film_title, MAX_FILM_TITLE_SIZE) < 0){
			close(client_socket);
			error_handler("[SERVER] Errore scrittura FILM title");
		}

	}

	pthread_mutex_destroy(&user_rented_films->films_mutex);

	for(int i = 0; i < user_rented_films->dim; i++){
        free(user_rented_films->films[i]);
    }
    free(user_rented_films);

	pthread_mutex_unlock(&reservation_list->reservations_mutex);
	pthread_mutex_unlock(&film_list->films_mutex);
}

int rent_film(sqlite3* database, unsigned int user_id, unsigned int film_id){

	pthread_mutex_lock(&shopkeeper_max_rented_films_mutex);
	pthread_mutex_lock(&film_list->films_mutex);
	pthread_mutex_lock(&reservation_list->reservations_mutex);

	if(check_user_exceed_max_rented_film(user_id)){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		pthread_mutex_unlock(&film_list->films_mutex);
		pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);
		return ERROR_RENT_FILM_MAX_ALLOWED;
	}

	if(check_user_already_rent_film(user_id, film_id)){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		pthread_mutex_unlock(&film_list->films_mutex);
		pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);
		return ERROR_RENT_ALREADY_EXISTS;
	}


	if((check_film_available_copies_less_than_or_equal_zero(film_id) == 1) || (check_film_available_copies_less_than_or_equal_zero(film_id) == -1)){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		pthread_mutex_unlock(&film_list->films_mutex);
		pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);
		return ERROR_RENT_FILM_NO_AVAILABLE_COPY;
	}

	if (decrement_film_available_copy_and_increment_film_rented_out_copy(film_id) == NULL){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		pthread_mutex_unlock(&film_list->films_mutex);
		pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);
		return ERROR_RENT_FILM_NO_AVAILABLE_COPY;
	}

	database_film_remove_available_copy(database, film_id);
	database_film_add_rented_out_copy(database, film_id);

	create_new_reservation(database, user_id, film_id);

	pthread_mutex_unlock(&reservation_list->reservations_mutex);
	pthread_mutex_unlock(&film_list->films_mutex);
    pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);

	return 1;
}

int return_rented_film(sqlite3* database, unsigned int user_id, unsigned int film_id){

	pthread_mutex_lock(&film_list->films_mutex);
	pthread_mutex_lock(&reservation_list->reservations_mutex);

	if((check_film_rented_out_copies_less_than_or_equal_zero(film_id) == 1) || (check_film_rented_out_copies_less_than_or_equal_zero(film_id) == -1)){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		pthread_mutex_unlock(&film_list->films_mutex);
		return ERROR_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT;
	}

	if (increment_film_available_copy_and_decrement_film_rented_out_copy(film_id) == NULL){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		pthread_mutex_unlock(&film_list->films_mutex);
		return ERROR_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT;
	}

	database_film_remove_rented_out_copy(database, film_id);
	database_film_add_available_copy(database, film_id);

	time_t now_reservation_due_date;
	int reservation_id = database_reservation_add_due_date(database, user_id, film_id, &now_reservation_due_date);
	
	update_reservation_due_date(reservation_id, now_reservation_due_date);

	pthread_mutex_unlock(&reservation_list->reservations_mutex);
	pthread_mutex_unlock(&film_list->films_mutex);

	return 1;
}

int get_max_rented_films(){

	pthread_mutex_lock(&shopkeeper_max_rented_films_mutex);

	int max_rented_films = shopkeeper_max_rented_films_per_user;

	pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);

	return max_rented_films;
}

int shopkeeper_change_max_rented_films(unsigned int shopkeeper_id, int new_max_rented_films){

	pthread_mutex_lock(&shopkeeper_max_rented_films_mutex);
	pthread_mutex_lock(&user_list->users_mutex);
	pthread_mutex_lock(&reservation_list->reservations_mutex);

	if(check_is_not_shopkeeper(shopkeeper_id)){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		pthread_mutex_unlock(&user_list->users_mutex);
		pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);
		return ERROR_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE;
	}

	if(check_all_users_exceed_max_rented_film(new_max_rented_films)){
		pthread_mutex_unlock(&reservation_list->reservations_mutex);
		pthread_mutex_unlock(&user_list->users_mutex);
		pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);
		return ERROR_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED;
	}

	shopkeeper_max_rented_films_per_user = new_max_rented_films;

	pthread_mutex_unlock(&reservation_list->reservations_mutex);
	pthread_mutex_unlock(&user_list->users_mutex);
	pthread_mutex_unlock(&shopkeeper_max_rented_films_mutex);

	return 1;
}

int shopkeeper_notify_expired_films(unsigned int shopkeeper_id){
	
	pthread_mutex_lock(&user_list->users_mutex);
	pthread_mutex_lock(&connection_list->connections_mutex);

	if(check_is_not_shopkeeper(shopkeeper_id)){
		pthread_mutex_unlock(&connection_list->connections_mutex);
		pthread_mutex_unlock(&user_list->users_mutex);
		return ERROR_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE;
	}

	for(int i = 0; i < connection_list->dim; i++){
		if(kill(connection_list->connections[i]->client_pid, SIGUSR1) < 0)
			error_handler("[SERVER] Errore invio kill SIGUSR1");
	}

	pthread_mutex_unlock(&connection_list->connections_mutex);
	pthread_mutex_unlock(&user_list->users_mutex);

	return 1;
}

//ausiliari
int check_user_already_exists(char *user_username){

	user_t* user_to_search = search_user_by_username(user_username);
	
	if(user_to_search != NULL)
		return 1;
	else
		return 0;
}

int check_user_does_not_exists(char *user_username){

	user_t* user_to_search = search_user_by_username(user_username);
	
	if(user_to_search == NULL)
		return 1;
	else
		return 0;
}

int check_user_already_rent_film(unsigned int user_id, unsigned int film_id){

    for(int i = 0; i < reservation_list->dim; i++){

        if(
			reservation_list->reservations[i]->user_id == user_id
			&&
           	reservation_list->reservations[i]->film_id == film_id
			&&
           	reservation_list->reservations[i]->due_date == 0
		)
        	return 1;
        
    }

    return 0;
}

int check_film_available_copies_less_than_or_equal_zero(unsigned int film_id){

	film_t* film_to_serch = search_film_by_id(film_id);

	if(film_to_serch != NULL){

		if(film_to_serch->available_copies <= 0)
			return 1;
		else
			return 0;

	} else return -1;
}

int check_film_rented_out_copies_less_than_or_equal_zero(unsigned int film_id){

	film_t* film_to_serch = search_film_by_id(film_id);

	if(film_to_serch != NULL){

		if(film_to_serch->rented_out_copies <= 0)
			return 1;
		else
			return 0;

	} else return -1;
}

int check_all_users_exceed_max_rented_film(int new_max_rented_films){

	int* user_reservations_count = (int*)calloc(user_list->dim, sizeof(int));
	if(user_reservations_count == NULL)
		error_handler("[SERVER] Errore allocazione dinamica user reservations count");

	int exceeded = 0;

    for(int i = 0; i < user_list->dim; i++){
        
        user_t* current_user = user_list->users[i];

        for(int j = 0; j < reservation_list->dim; j++){
            
            reservation_t* current_reservation = reservation_list->reservations[j];

            if(
                current_reservation->user_id == current_user->id
                &&
                current_reservation->due_date == 0
            ){
                user_reservations_count[i]++;
            }
        }
    }

    for(int i = 0; i < user_list->dim; i++){
        if(user_reservations_count[i] > new_max_rented_films){
            exceeded = 1;
            break;
        }
    }

    free(user_reservations_count);

	return exceeded;
}

int check_user_exceed_max_rented_film(unsigned int user_id){

    user_t* current_user = search_user_by_id(user_id);
	if(current_user == NULL)
		return 1;

	int exceeded = 0;    
	int rented_films_count = 0;

    for(int i = 0; i < reservation_list->dim; i++){

        reservation_t* current_reservation = reservation_list->reservations[i];

        if(
            current_reservation->user_id == current_user->id
            &&
            current_reservation->due_date == 0
        ){
            rented_films_count++;
        }

		if(rented_films_count >= shopkeeper_max_rented_films_per_user){
            exceeded = 1;
            break; 
        }
    }

	return exceeded;
}

int check_is_not_shopkeeper(unsigned int user_id){
	
	user_t* shopkeeper_to_check = search_user_by_id(user_id);
	int is_not_shopkeeper = 1;

	if(
		shopkeeper_to_check != NULL
		&&
		strncmp(shopkeeper_to_check->username, SHOPKEEPER_USERNAME, MAX_USER_USERNAME_SIZE) == 0
	)
		is_not_shopkeeper = 0;

	return is_not_shopkeeper;
}

connection_data_t* search_connection_by_client_pid(pid_t client_pid){

	connection_data_t* searched_connection = NULL;

	for(int i = 0; i < connection_list->dim; i++){
		if(connection_list->connections[i]->client_pid == client_pid){
			searched_connection = connection_list->connections[i];
			break;
		}
	}

	return searched_connection;
}

user_t* search_user_by_id(unsigned int user_id){

	user_t* searched_user = NULL;

	for(int i = 0; i < user_list->dim; i++){
		if(user_list->users[i]->id == user_id){
			searched_user = user_list->users[i];
			break;
		}
	}

	return searched_user;
}

user_t* search_user_by_username(char *user_username){

	user_t* searched_user = NULL;

	for(int i = 0; i < user_list->dim; i++){

		if(strncmp(user_list->users[i]->username, user_username, MAX_USER_USERNAME_SIZE) == 0){
			searched_user = user_list->users[i];
			break;
		}
	}

	return searched_user;
}

user_t* search_user_by_username_and_password(char *user_username, char *user_password){

	user_t* searched_user = NULL;

	for(int i = 0; i < user_list->dim; i++){

		if(
			(strncmp(user_list->users[i]->username, user_username, MAX_USER_USERNAME_SIZE) == 0)
			&&
			(strncmp(user_list->users[i]->password, user_password, MAX_USER_PASSWORD_SIZE) == 0)
		
		){
			searched_user = user_list->users[i];
			break;
		}
	}

	return searched_user;
}

film_t* search_film_by_id(unsigned int film_id){

	film_t* searched_film = NULL;

	for(int i = 0; i < film_list->dim; i++){
		if(film_list->films[i]->id == film_id){
			searched_film = film_list->films[i];
			break;
		}
	}

	return searched_film;
}

reservation_t* search_reservation_by_id(unsigned int reservation_id){

	reservation_t* searched_reservation = NULL;

	for(int i = 0; i < reservation_list->dim; i++){
		if(reservation_list->reservations[i]->id == reservation_id){
			searched_reservation = reservation_list->reservations[i];
			break;
		}
	}

	return searched_reservation;
}

void error_handler(char *message){
	printf("\n%s.\n", message);
	exit(-1);
}
