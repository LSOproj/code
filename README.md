# Istruzioni per l'esecuzione utilizzando Docker Compose

Segui questi passaggi per avviare, utilizzare e arrestare il sistema tramite Docker Compose.

## 1. Avvio del sistema
Dalla directory root "/code", eseguire il seguente comando per buildare le immagini ed eseguire i container videorent-server e videorent-client:
```
bash
docker compose up --build -d
```
## 2. Accesso al Client
Per visualizzare il client interattivo:
```
bash
docker attach videorent-client
```

## 3. Visualizzazione log Server
Per visionare i log del server, eseguire in un nuovo terminale il seguente comando:
```
bash
docker compose logs -f server
```

## 4. Arresto
Per terminare l'applicazione:
```
bash
docker compose down
```

## Credenziali del negoziante
Le credenziali del gestore della videoteca sono le seguenti:
```
bash
nome utente: "negoziante"
password: "password"
```