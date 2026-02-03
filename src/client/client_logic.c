#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client_logic.h"

int get_movie_idx_by_id(int movie_id){
	int i = 0;
	while(i < num_films_avaible){
		if((avaible_films[i].id) == movie_id)
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

void parse_film_ids(char *input, int *film_ids, int *count){
	*count = 0;
	char temp[10];
	int temp_idx = 0;
	
	for(int i = 0; input[i] != '\0' && *count < 5; i++){
		if(input[i] == ',' || input[i] == ' '){
			if(temp_idx > 0){
				temp[temp_idx] = '\0';
				int movie_id = atoi(temp);
				if(movie_id > 0){
					// Validazione: film esiste?
					int idx = get_movie_idx_by_id(movie_id);
					if(idx < 0){
						printf("ID %d non valido.\n", movie_id);
					} else if(avaible_films[idx].available_copies <= 0){
						printf("Non ci sono copie disponibili per '%s'.\n", avaible_films[idx].title);
					} else {
						// Film già nel carrello?
						int in_cart = get_cart_count_by_id(movie_id);
						if(in_cart > 0){
							printf("Film '%s' è già nel carrello. Non è possibile aggiungere lo stesso film più volte.\n", avaible_films[idx].title);
						} else {
							film_ids[*count] = movie_id;
							(*count)++;
						}
					}
				}
				temp_idx = 0;
			}
		} else {
			temp[temp_idx++] = input[i];
		}
	}
	if(temp_idx > 0){
		temp[temp_idx] = '\0';
		int movie_id = atoi(temp);
		if(movie_id > 0){
			// Validazione: film esiste?
			int idx = get_movie_idx_by_id(movie_id);
			if(idx < 0){
				printf("ID %d non valido.\n", movie_id);
			} else if(avaible_films[idx].available_copies <= 0){
				printf("Non ci sono copie disponibili per '%s'.\n", avaible_films[idx].title);
			} else {
				// Film già nel carrello?
				int in_cart = get_cart_count_by_id(movie_id);
				if(in_cart > 0){
					printf("Film '%s' è già nel carrello. Non è possibile aggiungere lo stesso film più volte.\n", avaible_films[idx].title);
				} else {
					film_ids[*count] = movie_id;
					(*count)++;
				}
			}
		}
	}
}

void init_cart(void){
	//teoricamente ogni volta al boot up 
	//si salva il cap imposto dal venditore
	cart.dim = 0;
}

void add_to_cart(int movie_id){
	if(cart.dim >= cart_cap)
		printf("Impossibile aggiungere  al carrello, limite raggiunto.");
	else{
		cart.film_id_to_rent[cart.dim] = movie_id;
		cart.dim++;
	}
}

void remove_from_cart(char *report, size_t report_size){
	if(cart.dim <= 0){
		if(report && report_size > 0)
			snprintf(report, report_size, "Il Carrello e' vuoto, nulla da rimuovere.\n");
		return;
	}

	printf("Inserire gli ID dei film da rimuovere (separati da virgola): ");
	char input[100] = {0};
	scanf(" %99[^\n]", input);
	getchar(); // Consumare il newline rimasto

	if(input[0] == '\0'){
		if(report && report_size > 0)
			snprintf(report, report_size, "Nessun ID inserito.\n");
		return;
	}

	if(report && report_size > 0)
		report[0] = '\0';
	int report_len = 0;

	char *token = strtok(input, ",");
	int found_any = 0;
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
		found_any = 1;

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

		if(report && report_size > 0){
			if(removed_count > 0){
				int idx = get_movie_idx_by_id(movie_id);
				if(idx >= 0){
					report_len += snprintf(report + report_len, report_size - report_len,
						"Rimosso: %s (x%d)\n", avaible_films[idx].title, removed_count);
				} else {
					report_len += snprintf(report + report_len, report_size - report_len,
						"Rimosso ID %d (x%d)\n", movie_id, removed_count);
				}
			} else {
				report_len += snprintf(report + report_len, report_size - report_len,
					"ID %d non presente nel carrello.\n", movie_id);
			}
		}

		token = strtok(NULL, ",");
	}

	if(!found_any && report && report_size > 0){
		snprintf(report, report_size, "Nessun ID inserito.\n");
	}
}
