server:
	gcc ./src/server/server.c -o ./src/server/server

server:
	gcc ./src/client/client.c -o ./src/client/client -lsqlite3

clear:
	rm ./src/server/server.c ./src/client/client.c
