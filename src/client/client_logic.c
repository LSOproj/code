#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_logic.h"

int get_movie_idx_by_id(int movie_id){
	int i = 0;
	while(i < num_films_available){
		if((available_films[i].id) == movie_id)
			return i;
		i++;
	}

	return -1;
}

int get_cart_count_by_id(int movie_id){
	int count = 0;
	for(int i = 0; i < cart.dim; i++){
		if(cart.film_id_to_rent[i] == movie_id)
			count++;
	}
	return count;
}

void parse_film_ids(const char *user_input, int *film_ids, int *count){
	*count = 0;

	if(user_input == NULL)
		return;

	char temp[10];
	int temp_idx = 0;
	
	for(int i = 0; user_input[i] != '\0' && *count < cart_cap; i++){
		if(user_input[i] == ',' || user_input[i] == ' '){
			if(temp_idx > 0){
				temp[temp_idx] = '\0';
				int movie_id = atoi(temp);
				if(movie_id > 0){
					// Validazione: film esiste?
					int idx = get_movie_idx_by_id(movie_id);
					if(idx >= 0 && available_films[idx].available_copies > 0){
						int in_cart = get_cart_count_by_id(movie_id);
						if(in_cart <= 0){
							film_ids[*count] = movie_id;
							(*count)++;
						}
					}
				}
				temp_idx = 0;
			}
		} else {
			temp[temp_idx++] = user_input[i];
		}
	}
	if(temp_idx > 0){
		temp[temp_idx] = '\0';
		int movie_id = atoi(temp);
		if(movie_id > 0){
			// Validazione: film esiste?
			int idx = get_movie_idx_by_id(movie_id);
			if(idx >= 0 && available_films[idx].available_copies > 0){
				int in_cart = get_cart_count_by_id(movie_id);
				if(in_cart <= 0){
					film_ids[*count] = movie_id;
					(*count)++;
				}
			}
		}
	}
}

// ============================================================================
// RENTAL OPERATIONS
// ============================================================================

void init_cart(void){
	// teoricamente ogni volta al boot up
	//si salva il cap imposto dal venditore
	cart.dim = 0;
}

void add_to_cart(int movie_id){
	if(cart.dim >= cart_cap){
		printf("Impossibile aggiungere film al carrello, limite raggiunto (%d/%d).\n", cart.dim, cart_cap);
	} else {
		cart.film_id_to_rent[cart.dim] = movie_id;
		cart.dim++;
	}
}

void empty_out_cart(){
	for(int i = 0; i < cart.dim; i++){
		cart.film_id_to_rent[i] = 0;
	}
	cart.dim = 0;
}

void remove_returned_film_from_memory(int id_film_to_remove){
	int idx_film = -1;

	for(int i = 0; i < num_rented_films; i++){
		if(rented_films[i].id == id_film_to_remove){
			idx_film = i;
			break;
		}
	}

	if(idx_film < 0)
		return;

	for(int i = idx_film; i < num_rented_films - 1; i++){
		rented_films[i] = rented_films[i+1];
	}

	num_rented_films--;
}

void remove_from_cart(const char *user_input){
	if(cart.dim <= 0){
		return;
	}

	if(user_input == NULL || user_input[0] == '\0'){
		return;
	}

	char input_copy[100] = {0};
	strncpy(input_copy, user_input, sizeof(input_copy) - 1);

	char *token = strtok(input_copy, ",");
	while(token != NULL){
		while(*token == ' ')
			token++;

		int movie_id = 0;
		int requested = 0;
		int parsed = sscanf(token, "%d(%d)", &movie_id, &requested);
		if(parsed < 1 || movie_id <= 0){
			token = strtok(NULL, ",");
			continue;
		}

		int removed_count = 0;
		if(parsed == 2 && requested > 0){
			int to_remove = requested;
			for(int j = 0; j < cart.dim && to_remove > 0; j++){
				if(cart.film_id_to_rent[j] == movie_id){
					for(int k = j; k < cart.dim - 1; k++)
						cart.film_id_to_rent[k] = cart.film_id_to_rent[k+1];
					cart.film_id_to_rent[cart.dim - 1] = 0;
					cart.dim--;
					removed_count++;
					to_remove--;
					j--;
				}
			}
		} else {
			for(int j = 0; j < cart.dim; j++){
				if(cart.film_id_to_rent[j] == movie_id){
					for(int k = j; k < cart.dim - 1; k++)
						cart.film_id_to_rent[k] = cart.film_id_to_rent[k+1];
					cart.film_id_to_rent[cart.dim - 1] = 0;
					cart.dim--;
					removed_count++;
					j--;
				}
			}
		}

		token = strtok(NULL, ",");
	}
}

void parse_film_ids_to_return(const char *user_input, int *film_ids, int *count){
	*count = 0;

	if(user_input == NULL)
		return;

	char temp[10];
	int temp_idx = 0;
	
	for(int i = 0; user_input[i] != '\0' && *count < MAX_FILMS; i++){
		if(user_input[i] == ',' || user_input[i] == ' '){
			if(temp_idx > 0){
				temp[temp_idx] = '\0';
				int movie_id = atoi(temp);
				if(movie_id > 0){
					// Validazione: film è nei film noleggiati?
					int found = 0;
					for(int j = 0; j < num_rented_films; j++){
						if(rented_films[j].id == movie_id){
							found = 1;
							break;
						}
					}
					if(found){
						film_ids[*count] = movie_id;
						(*count)++;
					}
				}
				temp_idx = 0;
			}
		} else {
			temp[temp_idx++] = user_input[i];
		}
	}
	if(temp_idx > 0){
		temp[temp_idx] = '\0';
		int movie_id = atoi(temp);
		if(movie_id > 0){
			// Validazione: film è nei film noleggiati?
			int found = 0;
			for(int j = 0; j < num_rented_films; j++){
				if(rented_films[j].id == movie_id){
					found = 1;
					break;
				}
			}
			if(found){
				film_ids[*count] = movie_id;
				(*count)++;
			}
		}
	}
}

void convert_date_to_string(time_t timestamp, char *buffer, size_t buffer_size){
	if(timestamp > 0){
		struct tm *timeinfo = localtime(&timestamp);
		strftime(buffer, buffer_size, "%d/%m/%Y %H:%M", timeinfo);
	} else {
		strncpy(buffer, "N/A", buffer_size - 1);
		buffer[buffer_size - 1] = '\0';
	}
}
