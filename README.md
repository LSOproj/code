# Istruzioni per l'esecuzione in locale
## 1. Prerequisiti
Per installare il compilatore gcc, con altri strumenti assistenziali, ed l'engine sqlite per il database:
```bash
sudo apt install build-essential libsqlite3-dev
```

## 2. Compilazione
Dalla root "/code", per compilare correttamente server e client utilizzando "Makefile":
```bash
make
```

## 3. Avvio del server
Aprire un nuovo terminale, recarsi nella root "/code" ed eseguire:
```bash
./src/server/server
```
Prima di procedere con l'avvio del client, attendere che il server sia correttamente inizializzato

## 4. Avvio del client
Aprire un nuovo terminale, recarsi nella root "/code" ed eseguire:
```bash
./src/client/client
```

# Istruzioni per l'esecuzione utilizzando Docker Compose
## 1. Avvio del sistema
Dalla directory root "/code", eseguire il seguente comando per buildare le immagini ed eseguire i container videorent-server e videorent-client:
```bash
docker compose up --build -d
```
## 2. Accesso al Client
Per visualizzare il client interattivo:
```bash
docker attach videorent-client
```

## 3. Visualizzazione log Server
Per visionare i log del server, eseguire in un nuovo terminale il seguente comando:
```bash
docker compose logs -f server
```

## 4. Arresto
Per terminare l'applicazione:
```bash
docker compose down
```

# Credenziali del negoziante
Le credenziali del gestore della videoteca sono le seguenti:
```
nome utente: "negoziante"
password: "password"
```