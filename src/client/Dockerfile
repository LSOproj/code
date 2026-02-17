# Stage di Build
FROM ubuntu:24.04 AS builder

WORKDIR /build

RUN apt update && apt install -y build-essential

COPY src/client/ src/client/

RUN gcc -o client \
    src/client/client.c \
    src/client/client_logic.c \
    src/client/client_protocol.c \
    -pthread

# Stage di Runtime
FROM ubuntu:24.04

WORKDIR /app

COPY --from=builder /build/client .

ENV SERVER_HOST=server
ENV SERVER_PORT=5200

CMD ["./client"]