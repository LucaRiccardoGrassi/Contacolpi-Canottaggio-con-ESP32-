Il progetto è ideato per rendere visibili i dati principali di un allenamento di canottaggio attraverso una pagina web visibile a tutti i dispositivi connessi sulla stessa rete. L’hardware si basa su un microcontrollore ESP32, accompagnato da diversi sensori: un modulo GPS per rilevare velocità e distanza, una ventola per stimare la velocità in nodi, e un fotoresistore per il conteggio dei colpi di voga. L’utente può monitorare parametri come tempo, distanza percorsa, velocità in km/h, passo sui 500m e il numero di colpi per minuto. Il sistema include inoltre un pulsante hardware per avviare e interrompere l’allenamento. 

Componenti:

- Fotoresistenza
- Ventola
- Bottone
- GPS Neo-6M 

Librerie Usate:

- WiFi.h: per collegare l'ESP32 al WiFi

- WebServer.h: per la  gestione delle richieste HTTP

- ESPmDNS.h: permette di rendere l'ESP32 un vero e proprio server web, raggiungibile tramite link e non solo tramite indirizzo IP. 

- TinyGPS++.h: libreria per decodificare le stringhe NMEA proveniente dal modulo GPS estraendo latitudine e longitudine. Inoltre utile per estrarre la velocità tramite funzioni.

La pagina web è stata scritta tramite HTML e la grafica tramite bootstrap, un framework CSS che fornisce stili già pronti all'uso. L'ho utilizzato solamente per renderlo visibilmente più bello. Oltre a HTML ho usato codice JavaScript per andare ad aggiornare i dati in tempo reale senza dover ricaricare la pagina.  

All'avvio del programma l'ESP32 si collega alla rete WiFi, viene creato un server web locale visibile a tutti i dispositivi sulla stessa rete. I dati vengono costantemente aggiornati tramite chiamate fetch. Sono stati fatti inoltre dei controlli per risolvere i prolemi riguardanti il rumore che possono ricevere i diversi sensori: per il GPS si ignora un movimento inferiore ai 5m. I nodi inferiori a 3 vengono considerati rumore. 
