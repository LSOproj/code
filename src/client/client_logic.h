#ifndef CLIENT_LOGIC_H
#define CLIENT_LOGIC_H

#define MAX_FILMS				100
#define MAX_FILM_TITLE_SIZE		100
#define MAX_RESERVATIONS		200

#include <time.h>

typedef struct film_t {
	int id;
	char title[MAX_FILM_TITLE_SIZE];
	int available_copies;
	int rented_out_copies;
} film_t;

typedef struct cart_t {
	int film_id_to_rent[MAX_FILMS];
	int dim; //capped by seller
} cart_t;

typedef struct reservation_t {

	unsigned int id;
	time_t rental_date;
	time_t expiring_date;
	time_t due_date;
	unsigned int user_id;
	unsigned int film_id;

} reservation_t; 

// Global variables
extern int num_films_avaible;
extern film_t avaible_films[MAX_FILMS];
extern cart_t cart;
extern int cart_cap;
extern int num_rented_films;
extern film_t rented_films[MAX_FILMS];

extern int num_expired_films;
extern film_t expired_films[MAX_FILMS];

extern int num_reservations;
extern reservation_t reservations[MAX_RESERVATIONS];

// Function prototypes
void init_cart(void);
void add_to_cart(int movie_id);
void remove_from_cart(char *report, size_t report_size);
int get_movie_idx_by_id(int movie_id);
int get_cart_count_by_id(int movie_id);
void parse_film_ids(char *input, int *film_ids, int *count);
void parse_film_ids_to_return(char *input, int *film_ids, int *count);

#endif
