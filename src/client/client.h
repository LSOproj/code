#ifndef CLIENT_H
#define CLIENT_H

#include <signal.h>

// ============================================================================
// SIGNAL HANDLERS
// ============================================================================
void expired_films_signal_handler(int signum);

// ============================================================================
// MAIN MENUS
// ============================================================================
int start_up_menu(void);
int renting_menu(void);
int shopkeeper_menu_display(void);
void register_user(int client_socket);
void login_user(int client_socket);
void rental_menu(int client_socket);
void shopkeeper_menu(int client_socket);

// ============================================================================
// RENTAL MENU HANDLERS
// ============================================================================
void handle_add_films(void);
void handle_modify_cart(void);
void handle_checkout(int client_socket);
void handle_show_rented(int client_socket);
void handle_return(int client_socket);
int handle_logout(void);

// ============================================================================
// SHOPKEEPER MENU HANDLERS
// ============================================================================
void set_cap_films(int client_socket);

// ============================================================================
// UI UTILITY FUNCTIONS
// ============================================================================
void print_films(void);
void print_rented_films(void);
void print_expired_films(void);
void print_cart(void);
void clear_screen(void);
void show_main_view(void);

// ============================================================================
// INPUT UTILITY FUNCTIONS
// ============================================================================
void consume_stdin_line(void);
int read_menu_choice(const char *prompt);

#endif
