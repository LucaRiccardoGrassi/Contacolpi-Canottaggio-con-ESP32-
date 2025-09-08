#include<WiFi.h>
#include<WebServer.h>
#include<ESPmDNS.h>
#include <TinyGPS++.h>

#define TXGPS 16
#define RXGPS 17
#define PIN_PER_NODI 35
#define ADC_MAX 4095  
#define TENS_ESP 3.3  
#define PIN_PER_COLPI 32
#define PIN_BOTTONE 25

void notFound();
void sendData();

const char* ssid = "A54DiLuca";
const char* password = "lucaluca";

WebServer server(80);

TinyGPSPlus gps;
double ultimaLat = 0.0;
double ultimaLong = 0.0;
const double distanzaErrore = 5.0;
double distanzaTot = 0.0;
unsigned long TempoUltimoColpo = 0;
float colpiAlMinuto = 0;
bool allenamentoIniziato = false;
unsigned long tempoIniziato = 0;
unsigned long tempo_inizio_allenamento = 0;
bool primoAggiornamento = false;


void setup() {
  Serial.begin(115200); //monitor seriale

  pinMode(PIN_BOTTONE, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ");

  if(MDNS.begin("esp32")){
    Serial.println("Abilito: http://esp32.local/");
  }
  
  server.on("/", homePage);
  server.on("/dati", sendData);
  server.onNotFound(notFound);
  server.begin();

  Serial2.begin(9600, SERIAL_8N1, TXGPS, RXGPS);

}

void loop() {
  server.handleClient();

  static bool lastButtonState = HIGH;  // stato precedente
  bool currentButtonState = digitalRead(PIN_BOTTONE);

  // Rileva transizione da HIGH -> LOW (pulsante premuto)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    if (!allenamentoIniziato) {
      iniziaAllenamento();
    } else {
      stopAllenamento();
    }
    delay(100); // debounce
  }
  lastButtonState = currentButtonState;
}

void iniziaAllenamento(){
  tempo_inizio_allenamento = millis();
  allenamentoIniziato = true;
  primoAggiornamento = true;
  distanzaTot = 0;
  colpiAlMinuto = 0;
  TempoUltimoColpo = 0;
  Serial.println("Allenamento AVVIATO!");
}

void stopAllenamento(){
  allenamentoIniziato = false;
}


void homePage(){
  String html = R"rowliteral(
    <!DOCTYPE html>
    <html lang="it">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <title>Dashboard Allenamento</title>
      <!-- Bootstrap CSS -->
      <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/css/bootstrap.min.css" rel="stylesheet">
    </head>
    <body class="bg-light">

      <div class="container py-4">

        <!-- Titolo centrale -->
        <h1 class="text-center mb-4"> Dashboard Allenamento</h1>

        <!-- Riga passo 500m e colpi/min -->
        <div class="row g-3 mb-3">
          <div class="col-12 col-md-6 p-3 bg-white rounded shadow-sm">
            <h6>Passo (500m)</h6>
            <p id="passo-500m" class="fs-4 fw-bold">0.00</p>
          </div>
          <div class="col-12 col-md-6 p-3 bg-white rounded shadow-sm">
            <h6>Colpi/min</h6>
            <p id="cadenza" class="fs-4 fw-bold">0</p>
          </div>
        </div>

        <!-- Riga tempo e distanza -->
        <div class="row g-3 mb-3">
          <div class="col-12 col-md-6 p-3 bg-white rounded shadow-sm">
            <h6>Tempo</h6>
            <p id="tempo" class="fs-4 fw-bold">00:00</p>
          </div>
          <div class="col-12 col-md-6 p-3 bg-white rounded shadow-sm">
            <h6>Distanza (km)</h6>
            <p id="distanza" class="fs-4 fw-bold">0.00</p>
          </div>
        </div>

        <!-- Riga velocità in km/h della barca e velocità nodi -->
        <div class="row g-3 mb-3">
          <div class="col-12 col-md-6 p-3 bg-white rounded shadow-sm">
            <h6>Velocità (km/h)</h6>
            <p id="velocita-kmh" class="fs-4 fw-bold">0.00</p>
          </div>
          <div class="col-12 col-md-6 p-3 bg-white rounded shadow-sm">
            <h6>Velocità (nodi)</h6>
            <p id="velocita-nodi" class="fs-4 fw-bold">0.00</p>
          </div>
        </div>

      </div>

      <!-- Bootstrap JS (per eventuali componenti dinamici) -->
      <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/js/bootstrap.bundle.min.js"></script>

      <script>
        async function aggiornaDati() {
          try {
            const risposta = await fetch("/dati");
            const data = await risposta.json();
            document.getElementById("passo-500m").textContent = data.passo500;
            document.getElementById("tempo").textContent = data.tempo;
            document.getElementById("distanza").textContent = data.distanza_km;
            document.getElementById("velocita-kmh").textContent = data.velocita_kmh;
            document.getElementById("velocita-nodi").textContent = data.nodi.toFixed(2);
            document.getElementById("cadenza").textContent = data.colpi_min;
          } catch (e) {
            console.error("Errore nella fetch:", e);
          }
        }
        setInterval(aggiornaDati, 100); // aggiorna ogni 100 millisecondi
      </script>

    </body>
    </html>

  )rowliteral";

  server.send(200, "text/html", html);
}

void sendData(){
  /*Calcolo coordinate per velocita in km/h, velocità per 500m e tempo di allenamento*/
  while (Serial2.available()) {
    char c = Serial2.read();
    Serial.print(c);
    gps.encode(c);
    // Se vuoi anche vedere i dati grezzi NMEA:
  }

  //GPS
  if (gps.location.isUpdated()) {
    double LatAttuale = gps.location.lat();
    double LongAttuale = gps.location.lng();

    double distanzaPercorsa = TinyGPSPlus::distanceBetween(
      ultimaLat, ultimaLong,
      LatAttuale, LongAttuale
    );

    if (distanzaPercorsa >= distanzaErrore || ultimaLat == 0.0) {
      ultimaLat = LatAttuale;
      ultimaLong = LongAttuale;
      distanzaTot += distanzaPercorsa;

      Serial.print("Latitudine: ");
      Serial.println(LatAttuale, 6);
      Serial.print("Longitudine: ");
      Serial.println(LongAttuale, 6);
    } else {
      Serial.println("Movimento ignorato (rumore GPS)");
    }
  }

  //Velocità km/h
  float velocita_kmh = gps.speed.kmph();

  //Passo sui 500 metri
  float tempo500m_sec = 500.0 / (velocita_kmh * 1000.0 / 3600.0);
  int min_500 = (int)tempo500m_sec / 60;
  if(min_500 > 10 ){
    min_500 = 0;
  }
  int sec_500 = (int)tempo500m_sec % 60;
  if(min_500 == 0){
    sec_500 = 0;
  }

  //tempo di allenamento
  int minuti = 0;
  int secondi = 0;
  if (allenamentoIniziato) {
    if (primoAggiornamento) {
      minuti = 0;
      secondi = 0;
      primoAggiornamento = false;  // dal prossimo giro calcolo normale
    } else {
      unsigned long millesimiPassati = millis() - tempo_inizio_allenamento;
      secondi = (millesimiPassati / 1000) % 60;
      minuti  = (millesimiPassati / 60000);
    }
  }

  /*Calcolo Nodi */
  int raw = analogRead(PIN_PER_NODI);
  float voltage = (raw * TENS_ESP) / ADC_MAX; 
  float kmh = voltage * 10.0;
  float nodi = kmh / 1.852;
  if(nodi<3.00){
    nodi=0.00;  //Perché rumore del sensore
  }

  /*Calcolo numero Colpi*/
  int valoreFotoresistore = analogRead(PIN_PER_COLPI);

  if (valoreFotoresistore > 3990 && millis() - TempoUltimoColpo > 200) {  
    unsigned long now = millis();

    if (TempoUltimoColpo > 0) {
      unsigned long intervallo = now - TempoUltimoColpo;   // ms tra 2 colpi
      float sec = intervallo / 1000.0;                   // converto in secondi
      colpiAlMinuto = 60.0 / sec;                        // stima colpi/min
    }
    TempoUltimoColpo = now;
  }

  String json = "{";
  json += "\"velocita_kmh\": " + String(velocita_kmh, 2) + ",";
  json += "\"passo500\": \"" + String(min_500) + ":" + (sec_500 < 10 ? "0" : "") + String(sec_500) + "\",";
  json += "\"tempo\": \"" + String(minuti) + ":" + (secondi < 10 ? "0" : "") + String(secondi) + "\",";
  json += "\"distanza_km\": " + String(distanzaTot / 1000.0, 2) + ",";
  json += "\"nodi\": " + String(nodi, 2) + ",";
  json += "\"colpi_min\": " + String(colpiAlMinuto, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void notFound(){
  server.send(404, "text/plain", "Pagina Non trovata");
}

