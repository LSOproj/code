CC = gcc
CFLAGS = -Wall #abilita tutti i warning (opzionale solo per debug)
SERVER_DIR = src/server
CLIENT_DIR = src/client

all: server client

server:
	$(CC) $(CFLAGS) $(SERVER_DIR)/server.c -o $(SERVER_DIR)/server -lsqlite3

client:
	$(CC) $(CFLAGS) $(CLIENT_DIR)/client.c $(CLIENT_DIR)/client_logic.c $(CLIENT_DIR)/client_protocol.c -o $(CLIENT_DIR)/client

clean:
	rm -f $(SERVER_DIR)/server $(CLIENT_DIR)/client 
