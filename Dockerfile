# Stage di build
FROM ubuntu:24.04 AS build

WORKDIR /app

RUN apt update && \
    apt install -y \
    build-essential \
    libsqlite3-dev \
    sqlite3

COPY src/ ./src/

# Compilazione server
RUN gcc -o server \
    src/server/server.c \
    -lsqlite3

# Compilazione client con linking
RUN gcc -o client \
    src/client/client.c \
    src/client/client_logic.c \
    src/client/client_protocol.c

# Stage di Runtime
FROM ubuntu:24.04

WORKDIR /app

RUN apt update && \
    apt install -y \
    libsqlite3-dev \
    sqlite3 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /app/server /app/server
COPY --from=build /app/client /app/client
COPY src/server/database.db /app/database.db

COPY start.sh /app/start.sh
RUN chmod +x /app/start.sh

RUN mkdir -p /app/persist
RUN mkdir -p /tmp

CMD ["/app/start.sh"]