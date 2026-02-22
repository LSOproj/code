#ifndef CLIENT_PROTOCOL_H
#define CLIENT_PROTOCOL_H

#define DEFAULT_SERVER_HOST 	"127.0.0.1"
#define DEFAULT_SERVER_PORT		"5200"
#define MAX_USER_USERNAME_SIZE	100
#define MAX_USER_PASSWORD_SIZE	100

// ============================================================================
// TEXT PROTOCOL MACROS (REQUESTS)
// ============================================================================
#define REGISTER_PROTOCOL_MESSAGE 									"REGISTER"
#define LOGIN_PROTOCOL_MESSAGE 										"LOGIN"
#define GET_FILMS_PROTOCOL_MESSAGE  								"GET_FILMS"
#define SHOPKEEPER_GET_ALL_RESERVATIONS_PROTOCOL_MESSAGE			"SHOPKEEPER_GET_ALL_RESERVATIONS"
#define RENT_FILM_PROTOCOL_MESSAGE  								"RENT_FILM"
#define RETURN_RENTED_FILM_PROTOCOL_MESSAGE							"RETURN_RENTED_FILM"
#define GET_USER_RENTED_FILMS_PROTOCOL_MESSAGE						"GET_USER_RENTED_FILMS"
#define GET_MAX_RENTED_FILMS_PROTOCOL_MESSAGE						"GET_MAX_RENTED_FILMS"
#define GET_USER_EXPIRED_FILMS_NO_DUE_DATE_PROTOCOL_MESSAGE			"GET_USER_EXPIRED_FILMS_NO_DUE_DATE"
#define SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_PROTOCOL_MESSAGE			"SHOPKEEPER_CHANGE_MAX_RENTED_FILMS"
#define SHOPKEEPER_NOTIFY_EXPIRED_FILMS_PROTOCOL_MESSAGE			"SHOPKEEPER_NOTIFY_EXPIRED_FILMS"
#define SHOW_EXPIRED_FILMS_NOTIFICATION_PROTOCOL_MESSAGE			"SHOW_EXPIRED_FILMS_NOTIFICATION"

// ============================================================================
// TEXT PROTOCOL MACROS (RESPONSES)
// ============================================================================
#define SUCCESS_REGISTER										"SUCCESS_REGISTER"
#define SUCCESS_LOGIN											"SUCCESS_LOGIN"
#define SUCCESS_GET_FILMS										"SUCCESS_GET_FILMS"
#define SUCCESS_SHOPKEEPER_GET_ALL_RESERVATIONS					"SUCCESS_SHOPKEEPER_GET_ALL_RESERVATIONS"
#define SUCCESS_RENT_FILM										"SUCCESS_RENT_FILM"
#define SUCCESS_RETURN_RENTED_FILM								"SUCCESS_RETURN_RENTEND_FILM"
#define SUCCESS_GET_USER_RENTED_FILMS							"SUCCESS_GET_USER_RENTED_FILMS"
#define SUCCESS_GET_MAX_RENTED_FILMS							"SUCCESS_GET_MAX_RENTED_FILMS"
#define SUCCESS_GET_USER_EXPIRED_FILMS_NO_DUE_DATE				"SUCCESS_GET_USER_EXPIRED_FILMS_NO_DUE_DATE"
#define SUCCESS_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS				"SUCCESS_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS"
#define SUCCESS_SHOPKEEPER_NOTIFY_EXPIRED_FILMS					"SUCCESS_SHOPKEEPER_NOTIFY_EXPIRED_FILMS"

#define FAILED_USER_ALREADY_EXISTS								"FAILED_USER_ALREADY_EXISTS"
#define FAILED_USER_DOESNT_EXISTS								"FAILED_USER_DOESNT_EXISTS"
#define FAILED_USER_BAD_CREDENTIALS								"FAILED_USER_BAD_CREDENTIALS"

#define FAILED_RENT_FILM_MAX_ALLOWED							"FAILED_RENT_FILM_MAX_ALLOWED"
#define FAILED_RENT_FILM_NO_AVAILABLE_COPY						"FAILED_RENT_FILM_NO_AVAILABLE_COPY"
#define FAILED_RENT_ALREADY_EXISTS								"FAILED_RENT_ALREADY_EXISTS"

#define FAILED_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT			"FAILED_RETURN_RENTED_FILM_NO_AVIABLE_RENTED_OUT"

#define FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE		 	"FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_ROLE"
#define FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED 	"FAILED_SHOPKEEPER_CHANGE_MAX_RENTED_FILMS_USER_EXEEDED"

#define FAILED_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE			 	"FAILED_SHOPKEEPER_NOTIFY_EXPIRED_FILMS_ROLE"

#define FAILED_SHOPKEEPER_GET_ALL_RESERVATIONS_ROLE				"FAILED_SHOPKEEPER_GET_ALL_RESERVATIONS_ROLE"

#define PROTOCOL_MESSAGE_MAX_SIZE 								100

//sincronizzazione tra thread che ascolta per la ricezione notifiche e main thread
// ===========================================================================
// THREAD SYNCHRONIZATION DATA TYPE
// ===========================================================================
typedef struct threads_sync_t {
    pthread_mutex_t sync_mutex;
	pthread_cond_t wake_main_thread_cv;
	pthread_cond_t wake_listener_thread_cv;

	int data_for_main_thread_ready;
	int listener_suspended;

	char server_response[PROTOCOL_MESSAGE_MAX_SIZE];
	
} threads_sync_t;

extern threads_sync_t* threads_sync;

// ============================================================================
// DATA TYPES
// ============================================================================
typedef struct user_t {
	int id;
	char username[MAX_USER_USERNAME_SIZE];
	char password[MAX_USER_PASSWORD_SIZE];
} user_t;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
extern unsigned int user_id;
extern int client_socket;
extern int film_reminder;

// ============================================================================
// THREAD FUNCTIONS
// ============================================================================
void* listener_thread(void *arg);
threads_sync_t* init_threads_sync();
void wait_for_server_protocol_message(int client_socket, char* buffer);
void resume_listener_thread();

// ============================================================================
// PROTOCOL I/O FUNCTION PROTOTYPES
// ============================================================================
int register_user_request(int client_socket, const char *username, const char *password);
int login_user_request(int client_socket, const char *username, const char *password);
void get_user_id(int client_socket);
int check_server_response(int client_socket);
void get_max_rented_films(int client_socket);
void get_all_films(int client_socket);
void get_all_reservations(int client);
void get_user_rented_films(int client_socket);
void get_all_user_expired_films_with_no_due_date(int client_socket);
void shopkeeper_notify_expired_films(int client_socket);
int shopkeeper_change_max_rented_films(int client_socket, int new_film_cap);
void rent_film(int client_socket, int idx);
int return_film(int client_socket, int film_id);

#endif
