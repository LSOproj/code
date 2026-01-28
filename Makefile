server:
	gcc ./src/server/server.c -o ./src/server/server -lsqlite3

client:
	gcc ./src/client/client.c -o ./src/client/client

clear:
	rm ./src/server/server.c ./src/client/client.c
