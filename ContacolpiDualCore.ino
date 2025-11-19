#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TinyGPS++.h>
#include<TaskScheduler.h>

#define TXGPS 16
#define RXGPS 17
#define PIN_PER_NODI 35
#define ADC_MAX 4095  
#define TENS_ESP 3.3  
#define PIN_PER_COLPI 32
#define PIN_BOTTONE 25
#define LED_CLIENT 26 
#define ARRAY 10

// Pagina HTML 
const char index_html[] PROGMEM = R"rawliteral(
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
  let websocket;
  const gateway = `ws://${window.location.hostname}/ws`;
  function initWebSocket(){
    websocket = new WebSocket(gateway);
    websocket.onmessage = onMessage;
  }
  function onMessage(event){
    const data = JSON.parse(event.data);
    document.getElementById("velocita-kmh").textContent = data.velocita_kmh.toFixed(2);
    document.getElementById("velocita-nodi").textContent = data.nodi.toFixed(2);
    document.getElementById("cadenza").textContent = data.colpi_min.toFixed(1);
    document.getElementById("distanza").textContent = data.distanza_km.toFixed(2);
    document.getElementById("passo-500m").textContent = data.passo500;
    document.getElementById("tempo").textContent = data.tempo;
  }
  window.addEventListener('load', initWebSocket);
</script>
</body>
</html>
)rawliteral";
// Variabili globali
TinyGPSPlus gps;
double ultimaLat = 0.0, ultimaLong = 0.0, distanzaTot = 0.0, distanzaErrore = 0.0;
unsigned long tempo_inizio_allenamento = 0, TempoUltimoColpo = 0, tempoDebounce = 0;
float colpiAlMinuto = 0, velocita_kmh = 0, nodi = 0;
int min_500 = 0, sec_500 = 0, minuti = 0, secondi = 0, iCorrente = 0;
int arrayNodi[ARRAY];
bool allenamentoInCorso = false; 
bool statoPrecedenteBottone = HIGH;

// Oggetto Scheduler
Scheduler scheduler;

// Dichiaro le Funzioni per lo Scheduling
void taskGPS();
void taskNodi();
void taskColpi();
void taskNotify();
void taskTempo();

// Dichiaro le task per lo Scheduling
Task tGPS(1000 * TASK_MILLISECOND, TASK_FOREVER, &taskGPS);
Task tNodi(400 * TASK_MILLISECOND, TASK_FOREVER, &taskNodi);
Task tColpi(200 * TASK_MILLISECOND, TASK_FOREVER, &taskColpi);
Task tNotify(400 * TASK_MILLISECOND, TASK_FOREVER, &taskNotify);
Task tTempo(100 * TASK_MILLISECOND, TASK_FOREVER, &taskTempo);

// Credenziali Wi-Fi 
const char* ssid = "A54DiLuca";
const char* password = "lucaluca";

// Server e WebSocket 
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Funzione per aggiornare la pagina
void notifyClients(){
  String json = "{";
  json += "\"velocita_kmh\": " + String(velocita_kmh, 2) + ",";
  json += "\"passo500\": \"" + String(min_500) + ":" + (sec_500 < 10 ? "0" : "") + String(sec_500) + "\",";
  json += "\"tempo\": \"" + String(minuti) + ":" + (secondi < 10 ? "0" : "") + String(secondi) + "\",";
  json += "\"distanza_km\": " + String(distanzaTot / 1000.0, 2) + ",";
  json += "\"nodi\": " + String(nodi, 2) + ",";
  json += "\"colpi_min\": " + String(colpiAlMinuto, 1);
  json += "}";
  ws.textAll(json);
}

// Funnzioni per gesione eventi WebSocket
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
  switch(type){
    case WS_EVT_CONNECT:{
      Serial.printf("[WS] Client #%u connesso da %s\n", client->id(), client->remoteIP().toString().c_str());
      digitalWrite(LED_CLIENT, LOW);
      break;
    }
    case WS_EVT_DISCONNECT:{
      Serial.printf("Client disconnesso");
      digitalWrite(LED_CLIENT, HIGH);
      break;
    }
    default:
      break;
  }
}

// Inizializza WebSocket
void initWebSocket(){
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Funzione GPS
void taskGPS(){

  if (gps.location.isUpdated()) {
    double LatAttuale = gps.location.lat();
    double LongAttuale = gps.location.lng();

    double distanzaPercorsa = TinyGPSPlus::distanceBetween(
      ultimaLat, ultimaLong,
      LatAttuale, LongAttuale
    );

    if(gps.hdop.isValid()){
      double hdop = gps.hdop.hdop();
      if(hdop <= 1.0){
        distanzaErrore = 1.0;
      }
      else if(hdop <= 2.0){
        distanzaErrore = 2.0;
      }
      else if(hdop <= 5.0){
        distanzaErrore = 5.0;
      }
      else if(hdop <= 10.0){
        distanzaErrore = 10.0;
      }
      else{
        distanzaErrore = 50.0;
      }
    }

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

  velocita_kmh = gps.speed.kmph();

  //Passo sui 500 metri
  float tempo500m_sec = 500.0 / (velocita_kmh * 1000.0 / 3600.0);
  min_500 = (int)tempo500m_sec / 60;
  if(min_500 > 10 ){
    min_500 = 0;
  }
  sec_500 = (int)tempo500m_sec % 60;
  if(min_500 == 0){
    sec_500 = 0;
  }

}

void taskNodi(){
  // Calcolo Nodi
  int sommaRaw = 0;
  int raw = analogRead(PIN_PER_NODI);
  float mediaRaw = 0;
  arrayNodi[iCorrente] = raw;
  iCorrente++;
  if(iCorrente>=ARRAY){
    iCorrente = 0;
    for(int i=0; i<ARRAY; i++){
      sommaRaw+= arrayNodi[i];
    }
    mediaRaw = (float)sommaRaw / ARRAY;
    float voltage = (mediaRaw * TENS_ESP) / ADC_MAX; 
    float kmh = voltage * 10.0;
    nodi = kmh / 1.852;
    if(nodi<3.00){
      nodi=0.00;  //Perché rumore del sensore
    }
  }
}

void taskColpi(){
  // Conta Colpi
  int valoreFotoresistore = analogRead(PIN_PER_COLPI);
  if (valoreFotoresistore > 3990 && millis() - TempoUltimoColpo > 200){  
    unsigned long now = millis();
    if (TempoUltimoColpo > 0){
      unsigned long intervallo = now - TempoUltimoColpo; // ms tra 2 colpi
      float sec = intervallo / 1000.0;                   // converto in secondi
      colpiAlMinuto = 60.0 / sec;                        // stima colpi/min
    }
    TempoUltimoColpo = now;
  }
  if (millis() - TempoUltimoColpo > 7000) {
    colpiAlMinuto = 0;
  }
}

void taskNotify(){
  unsigned long millesimiPassati = 0;

  if(allenamentoInCorso) {
    millesimiPassati = millis() - tempo_inizio_allenamento;
  }
  secondi = (millesimiPassati / 1000) % 60;
  minuti  = (millesimiPassati / 60000);

  // Chiamo funzione per inviare dati alla pagina web
  notifyClients();
}

// Tempo di allenamento
void taskTempo(){
  int statoAttualeBottone = digitalRead(PIN_BOTTONE);
  int delay = 50;

  if (statoAttualeBottone != statoPrecedenteBottone) {
    tempoDebounce = millis();
  }

  if ((millis() - tempoDebounce) > delay) {
    if (statoAttualeBottone == LOW && statoPrecedenteBottone == HIGH) {
      allenamentoInCorso = !allenamentoInCorso; // Inverte lo stato

      if (allenamentoInCorso) {
        // Avvia il cronometro
        tempo_inizio_allenamento = millis();
        Serial.println("Allenamento Avviato.");
      } else {
        // Pausa/Stop del cronometro
        Serial.println("Allenamento Messo in Pausa.");
      }
    }
    statoPrecedenteBottone = statoAttualeBottone; // Aggiorna lo stato per il prossimo ciclo
  }
}


// Task Core 0: WebSocket loop e Pagina Web
void taskWeb(void *parameter) {
  bool connesso = false;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (connesso) {
        Serial.println("Disconnesso dal WiFi");
        connesso = false;
      }
      WiFi.reconnect();
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      continue;
    }

    if (!connesso) {
      Serial.println("Connesso al WiFi!");
      Serial.println(WiFi.localIP());
      connesso = true;
    }
    ws.cleanupClients();
    vTaskDelay(400 / portTICK_PERIOD_MS);
  }
}

void taskSensori(void *parameter) {
  Serial.println("Dentro task Sensori");

  // Aggiugo le Task allo Scheduler
  scheduler.init();
  scheduler.addTask(tGPS);
  scheduler.addTask(tNodi);
  scheduler.addTask(tColpi);
  scheduler.addTask(tNotify);
  scheduler.addTask(tTempo);

  tGPS.enable();
  tNodi.enable();
  tColpi.enable();
  tNotify.enable();
  tTempo.enable();
  
  for (;;){
    while (Serial2.available()) {
      //Serial.println("Dentro While in task Sensori");
      char c = Serial2.read();
      Serial2.print(c);
      gps.encode(c);
    }

    scheduler.execute();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup(){
  Serial.begin(115200);

  Serial2.begin(9600, SERIAL_8N1, RXGPS, TXGPS);

  pinMode(LED_CLIENT, OUTPUT);
  digitalWrite(LED_CLIENT, HIGH);
  Serial.println("Settato Led");

  pinMode(PIN_BOTTONE, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Settato Wifi");

  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  server.begin();
  Serial.println("Avviato Server Web");

  xTaskCreatePinnedToCore(taskSensori, "TaskSensori", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskWeb, "TaskWeb", 4096, NULL, 1, NULL, 0);
  Serial.println("Finito Setup");
}

void loop(){
}