#!/bin/bash

# Definiamo i percorsi
DB_SEED="/app/default_database/database.db"      # database di default fornito nell'immagine
DB_PERSIST="/app/persisted_database/database.db" # database nel volume per la persistenza effettiva
DB_LINK="/app/database.db"                       # database cercato dal codice C (nella stessa dir dell'eseguibile del server)

echo "[INFO] Verifica database..."

if [ ! -f "$DB_PERSIST" ]; then
    echo "[INFO] Primo avvio, inizializzando il database di default iniziale."
    cp "$DB_SEED" "$DB_PERSIST"
fi

# collegamento simbolico per far puntare "/app/database.db" (cercato da server.c) al volume
ln -sf "$DB_PERSIST" "$DB_LINK"

echo "[INFO] Avvio il Server..."

# Avvio server background
/app/server &

SERVER_PID=$!

# Viene dato il tempo al server di fare operazioni di inizializzazione
sleep 2

# Avvio client foreground
echo "[WRAPPER] Avvio il Client..."
/app/client

echo "[INFO] Client chiuso"
kill $SERVER_PID
echo "[INFO] Server chiuso"