#ifndef CLIENT_PROTOCOL_H
#define CLIENT_PROTOCOL_H

#define MAX_USER_USERNAME_SIZE	100
#define MAX_USER_PASSWORD_SIZE	100

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

// Global variables
extern int user_id;
extern user_t user;

// Function prototypes
void get_user_id(int client_socket);
int check_server_response(int client_socket);

#endif
