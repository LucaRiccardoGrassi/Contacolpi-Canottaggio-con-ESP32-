Il progetto è ideato per rendere visibili i dati principali di un allenamento di canottaggio utilizzando un microcontrollore ESP32. Il sistema acquisisce i dati da un GPS e da sensori analogici, andando a calcolare i dati chiavi e li rende visualizzabili attraverso una pagina web dinamica accessibile tramite WiFi. 

Componenti:

- Fotoresistenza
- Ventola
- Bottone
- GPS Neo-6M
- Led

Il codice sfrutta i due core dell'ESP32 per andare a separare le operzaioni di rete da quella dei sensori; viene implementato inoltre il WebSocket per andare ad aggiornare istantaneamente la dashboard web. Il codice calcola: 

- Passo sui 500m
- Colpi al minuto
- Velocità (km/h e nodi)
- Distanza Totale
- Tempo di allenamento 

Librerie Usate:

- WiFi.h: per collegare l'ESP32 al WiFi
- AsyncTCP.h e ESPAsyncWebServer.h: per la creazione del server HTTP e gestione della comunicazione WebSocket
- TinyGPS++.h: libreria per decodificare le stringhe NMEA proveniente dal modulo GPS estraendo latitudine e longitudine. Inoltre utile per estrarre la velocità tramite funzioni.
- TaskScheduler.h: gestione multi-tasking.

Il codice è organizzato in due task principali eseguite su due core separati per massimizzare le prestazioni.
Task Sensori (Eseguita su Core1), pensata per tutte le operazioni di I/O attraverso una suddivisione in task attraverso il TaskScheduler:

- tGPS (1000 ms): calcola la distanza totalepercorsa, filtra il rumore GPS tramite HDOP, determina la velocità in km/h e il passo sui 500m.
- tNodi (400 ms): legge la ventola ed esegue una media su 10 campioni per dare la velocità in nodi.
- tColpi (200 ms): monitora il fotoresistore per calcolare i colpi al minuto e azzera il valore se non vangono rilevati colpi per 7 secondi.
- tTempo (100 ms): gestisce lo stato del bottone per avviare poi il cronometro
- tNotify (400 ms): aggiorna la pagina web con i nuovi dati tramite notifyClients().

Task Web Eseguita sul Core0 che va a gestire la stabilità della conessione Wi-Fi e gestisce la riconnessione in caso di caduta, monitora lo stato della connessione Wi-Fi e gestisce la riconnessione.

La pagina web è stata scritta tramite HTML e la grafica tramite bootstrap, un framework CSS che fornisce stili già pronti all'uso. L'ho utilizzato solamente per renderlo visibilmente più bello.  
