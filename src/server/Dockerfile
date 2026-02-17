# Stage di Build
FROM ubuntu:24.04 AS builder

WORKDIR /build

RUN apt update && \
    apt install -y \
    build-essential \
    libsqlite3-dev \
    sqlite3

COPY src/server/ src/server/

RUN gcc -o server src/server/server.c -lsqlite3 -pthread

# Stage di Runtime
FROM ubuntu:24.04

WORKDIR /app

RUN apt update && \
    apt install -y libsqlite3-0 && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p src/server

COPY --from=builder /build/server .

COPY --from=builder /build/src/server/database.db ./src/server/database.db

EXPOSE 5200

CMD ["./server"]