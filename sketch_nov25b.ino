#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <IRremote.hpp>

// --- CONFIGURAÇÕES DE REDE ---
// SUBSTITUA pelas suas credenciais!
const char* ssid ="JESSICA_NET";
const char* password ="14081925";

// Servidor Web Assíncrono para controle remoto
AsyncWebServer server(80); 

// --- CONFIGURAÇÕES DE HARDWARE E ÍNDICES IR ---
#define IR_RECEIVER_PIN 34 // Pino do Receptor IR (Input only)
#define IR_SENDER_PIN   32 // Pino do Emissor IR
#define STATUS_LED_PIN  2  // LED de Status (Pode ser o LED onboard)

#define MAX_LEN         500  // Máximo de pulsos RAW por sinal (uint16_t)
#define NUM_SIGNALS     23   // Total de comandos: Ligar(1)+Desligar(1)+Modos(5)+Swing(1)+T16-T30(15)

// --- Definição de Índices (23 Sinais: 0 a 22) ---
// Comandos essenciais (0-1)
#define SIG_LIGAR       0
#define SIG_DESLIGAR    1
// Modos de operação (2-6)
#define SIG_MODO_1      2
#define SIG_MODO_2      3
#define SIG_MODO_3      4
#define SIG_MODO_4      5
#define SIG_MODO_5      6
#define SIG_SWING       7 // Oscilador (7)
// Temperaturas 16 a 30 (8-22)
#define SIG_TEMP_16     8
#define SIG_TEMP_17     9
#define SIG_TEMP_18     10
#define SIG_TEMP_19     11
#define SIG_TEMP_20     12
#define SIG_TEMP_21     13
#define SIG_TEMP_22     14
#define SIG_TEMP_23     15
#define SIG_TEMP_24     16
#define SIG_TEMP_25     17
#define SIG_TEMP_26     18
#define SIG_TEMP_27     19
#define SIG_TEMP_28     20
#define SIG_TEMP_29     21
#define SIG_TEMP_30     22 // Último índice
// --- FIM DOS ÍNDICES ---

// --- Buffers de Armazenamento (RAM) ---
uint16_t durations[NUM_SIGNALS][MAX_LEN]; 
unsigned int capturedSignalLength[NUM_SIGNALS] = {0}; 

// Variáveis temporárias (ISR)
volatile unsigned long irBufferTemp[MAX_LEN];
volatile unsigned int bufferPositionTemp = 0;

// ********************************************
// 2. ROTINAS DE CAPTURA (ISR) E ENVIO
// ********************************************

void IRAM_ATTR rxIR_Interrupt_Handler() {
  if (bufferPositionTemp < MAX_LEN) {
    irBufferTemp[bufferPositionTemp++] = micros(); 
  }
}

bool captureIR(uint16_t* targetDurations, unsigned int &targetLength, uint32_t waitMs = 5000) {
  Serial.println("Capturando sinal IR... Aponte o controle e aperte o botão.");
  bufferPositionTemp = 0; 

  digitalWrite(STATUS_LED_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(IR_RECEIVER_PIN), rxIR_Interrupt_Handler, CHANGE);

  unsigned long start = millis();
  while (millis() - start < waitMs) {
    delay(10);
  }

  detachInterrupt(digitalPinToInterrupt(IR_RECEIVER_PIN));
  digitalWrite(STATUS_LED_PIN, LOW);

  if (bufferPositionTemp > 10) { 
    unsigned int len = (bufferPositionTemp > 0) ? (bufferPositionTemp - 1) : 0;
    if (len > MAX_LEN) len = MAX_LEN;
    
    for (unsigned int i = 0; i < len; i++) {
      unsigned long diff = irBufferTemp[i + 1] - irBufferTemp[i];
      if (diff > 65535UL) targetDurations[i] = 65535;
      else targetDurations[i] = (uint16_t)diff;
    }
    
    targetLength = len;
    Serial.println("Sinal capturado com sucesso!");
    Serial.print("Comprimento: ");
    Serial.println(targetLength);
    return true;
  } else {
    Serial.println("Falha na captura. Tente novamente.");
    targetLength = 0;
    return false;
  }
}

void sendIR(uint16_t* durationsToSend, unsigned int lengthToSend) {
  if (lengthToSend > 0) {
    digitalWrite(STATUS_LED_PIN, HIGH); 
    Serial.println("Enviando sinal IR...");
    IrSender.sendRaw(durationsToSend, lengthToSend, 38); 
    Serial.println("Sinal enviado!");
    digitalWrite(STATUS_LED_PIN, LOW);
  } else {
    Serial.println("Nenhum sinal capturado para este comando.");
  }
}

// ********************************************
// 3. PERSISTÊNCIA DE DADOS (LittleFS)
// ********************************************

void salvarTodosOsSinais() {
  File f = LittleFS.open("/sinais.txt", "w"); 
  if (!f) {
    Serial.println("ERRO: Falha ao abrir /sinais.txt para escrita!");
    return;
  }
  
  for (int i = 0; i < NUM_SIGNALS; i++) {
    // 1. Escreve a TAG (nome do comando)
    if (i == SIG_LIGAR) {
      f.print("ligar:");
    } else if (i == SIG_DESLIGAR) {
      f.print("desligar:");
    } else if (i == SIG_SWING) {
      f.print("swing:");
    } else if (i >= SIG_MODO_1 && i <= SIG_MODO_5){ 
      int modo = i - SIG_MODO_1 + 1;
      f.print("modo"); f.print(modo); f.print(":");
    } else if (i >= SIG_TEMP_16 && i <= SIG_TEMP_30){ // T16 até T30
      int temp = 16 + (i - SIG_TEMP_16); 
      f.print("temp"); f.print(temp); f.print(":");
    }

    // 2. Escreve os DADOS (durações em formato CSV)
    for (unsigned int j = 0; j < capturedSignalLength[i]; j++) {
      f.print(durations[i][j]); 
      if (j < capturedSignalLength[i] - 1) f.print(","); 
    }
    f.println(); 
  }

  f.close(); 
  Serial.println(">> Todos os sinais foram SALVOS com sucesso em /sinais.txt!");
}

void carregarTodosOsSinais() {
  File f = LittleFS.open("/sinais.txt", "r");
  if (!f) {
    Serial.println("INFO: Arquivo /sinais.txt não encontrado. Execute a captura para criar.");
    return;
  }
  Serial.println(">> Carregando sinais da memória Flash...");

  while (f.available()) {
    String linha = f.readStringUntil('\n');
    linha.trim(); 
    if (linha.length() < 3) continue;
    
    int sep = linha.indexOf(':'); 
    if (sep == -1) continue; 

    String nome = linha.substring(0, sep);
    String lista = linha.substring(sep + 1);
    int index = -1;

    // --- Mapeamento da TAG para Índice ---
    if (nome == "ligar") index = SIG_LIGAR;
    else if (nome == "desligar") index = SIG_DESLIGAR;
    else if (nome == "swing") index = SIG_SWING;
    else if (nome.startsWith("modo")) {
      int modo = nome.substring(4).toInt(); 
      if (modo >= 1 && modo <= 5) index = (modo - 1) + SIG_MODO_1;
    }
    else if (nome.startsWith("temp")) {
      int temp = nome.substring(4).toInt(); 
      if (temp >= 16 && temp <= 30) index = (temp - 16) + SIG_TEMP_16; 
    }

    if (index == -1) continue; 

    // ** Parsing do CSV **
    int pos = 0;
    unsigned int arrIndex = 0;
    while (pos < lista.length() && arrIndex < MAX_LEN) {
      int comma = lista.indexOf(',', pos);
      String valor;
      if (comma == -1) { valor = lista.substring(pos); pos = lista.length(); } 
      else { valor = lista.substring(pos, comma); pos = comma + 1; }
      durations[index][arrIndex++] = (uint16_t)valor.toInt(); 
    }
    capturedSignalLength[index] = arrIndex; 
  }
  f.close(); 
  Serial.println(">> Sinais carregados com sucesso. Prontos para uso!");
}

// ********************************************
// 4. LÓGICA DO SERVIDOR WEB
// ********************************************

void handleCommand(AsyncWebServerRequest *request) {
    String commandStr = "";

    if (request->hasParam("cmd")) {
        commandStr = request->getParam("cmd")->value();
        commandStr.trim();
    }
    
    if (commandStr.length() == 0) {
        request->send(400, "text/plain", "Erro: Comando ausente.");
        return;
    }

    String responseMessage = "Comando '" + commandStr + "' recebido: ";
    bool commandExecuted = false;
    int indexToSend = -1;

    if (commandStr.equalsIgnoreCase("L")) { indexToSend = SIG_LIGAR; responseMessage += "LIGAR."; } 
    else if (commandStr.equalsIgnoreCase("D")) { indexToSend = SIG_DESLIGAR; responseMessage += "DESLIGAR."; } 
    else if (commandStr.equalsIgnoreCase("SW")) { indexToSend = SIG_SWING; responseMessage += "SWING."; } 
    
    else if (commandStr.startsWith("MOD") && commandStr.length() == 4) {
        int modo = commandStr.substring(3).toInt();
        if (modo >= 1 && modo <= 5) {
            indexToSend = (modo - 1) + SIG_MODO_1;
            responseMessage += "MODO " + String(modo) + ".";
        }
    }
    
    else if (commandStr.startsWith("T ") && commandStr.length() >= 4) {
        int temp = commandStr.substring(2).toInt();
        if (temp >= 16 && temp <= 30) {
          indexToSend = (temp - 16) + SIG_TEMP_16;
          responseMessage += "TEMP " + String(temp) + "C.";
        }
    }

    if (indexToSend != -1) {
        sendIR(durations[indexToSend], capturedSignalLength[indexToSend]); 
        commandExecuted = true;

        delay(200);
    }

    if (commandExecuted) {
        request->send(200, "text/plain", "OK! " + responseMessage);
    } else {
        request->send(400, "text/plain", "Erro: Comando nao reconhecido ou parametros invalidos.");
    }
}

void handleRoot(AsyncWebServerRequest *request) {
    String html = "<h1>ESP32 IR Server Online</h1>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Use o Painel Centralizado no PC e o endpoint /enviar?cmd=...</p>";
    html += "<p>Instrucoes de uso via Serial: 1 (Ligar), 2 (Desligar), M1..M5, 16..30, S (Salvar).</p>";
    request->send(200, "text/html", html);
}

// ********************************************
// 5. SETUP & LOOP PRINCIPAL
// ********************************************
void setup() {
  Serial.begin(115200);           
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(IR_RECEIVER_PIN, INPUT);
  IrSender.begin(IR_SENDER_PIN); 
  
  if (!LittleFS.begin(true)) {
    Serial.println("ERRO FATAL: Falha ao montar LittleFS. Verifique o Partition Scheme.");
  } else {
    Serial.println("LittleFS montado.");
    carregarTodosOsSinais(); 
  }

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  Serial.print("\nConectando-se a: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 30000) { 
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    server.on("/enviar", HTTP_GET, handleCommand); 
    server.on("/", HTTP_GET, handleRoot);         

    server.begin(); 
    Serial.println("Servidor Web iniciado.");
  } else {
    Serial.println("\nFalha na conexao Wi-Fi. O ESP continuara funcionando via Serial.");
  }
}

void loop() {
  // A lógica de controle via Serial deve ser mantida aqui
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    // --- COMANDOS DE CAPTURA SERIAL ---
    if (cmd == "1") {
      while (!captureIR(durations[SIG_LIGAR], capturedSignalLength[SIG_LIGAR])) { delay(200); }
    } else if (cmd == "2") {
      while (!captureIR(durations[SIG_DESLIGAR], capturedSignalLength[SIG_DESLIGAR])) { delay(200); }
    } else if (cmd.equalsIgnoreCase("SWA")) { 
      while (!captureIR(durations[SIG_SWING], capturedSignalLength[SIG_SWING])) { delay(200); }
    } 
    else if (cmd.startsWith("M") && cmd.length() == 2) { 
        int modo = cmd.substring(1).toInt();
        if (modo >= 1 && modo <= 5) {
            int idx = (modo - 1) + SIG_MODO_1;
            Serial.print("--- Capturando Sinal MODO "); Serial.print(modo); Serial.println(" ---");
            while (!captureIR(durations[idx], capturedSignalLength[idx])) { delay(200); }
        } else {
            Serial.println("Modo M invalido. Use M1 a M5.");
        }
    }
    else if (cmd.toInt() >= 16 && cmd.toInt() <= 30 && cmd.length() <= 3) {
      int temp = cmd.toInt();
      int idx = (temp - 16) + SIG_TEMP_16;
      Serial.print("--- Capturando Sinal TEMP "); Serial.print(temp); Serial.println(" ---");
      while (!captureIR(durations[idx], capturedSignalLength[idx])) { delay(200); }
    }
    
    // --- COMANDOS DE ENVIO SERIAL ---
    else if (cmd.equalsIgnoreCase("L")) {
      sendIR(durations[SIG_LIGAR], capturedSignalLength[SIG_LIGAR]);
    } else if (cmd.equalsIgnoreCase("D")) {
      sendIR(durations[SIG_DESLIGAR], capturedSignalLength[SIG_DESLIGAR]);
    } else if (cmd.equalsIgnoreCase("SW")) { 
      sendIR(durations[SIG_SWING], capturedSignalLength[SIG_SWING]);
    } else if (cmd.startsWith("MOD") && cmd.length() == 4) {
        int modo = cmd.substring(3).toInt();
        if (modo >= 1 && modo <= 5) {
            int idx = (modo - 1) + SIG_MODO_1;
            sendIR(durations[idx], capturedSignalLength[idx]);
        } else {
            Serial.println("Modo MOD invalido. Use MOD1 a MOD5.");
        }
    } else if (cmd.startsWith("T ")) {
        int temp = cmd.substring(2).toInt();
        if (temp >= 16 && temp <= 30) {
          int idx = (temp - 16) + SIG_TEMP_16;
          sendIR(durations[idx], capturedSignalLength[idx]);
        } else {
          Serial.println("Temperatura T invalida. Use T 16 a T 30.");
        }
    }
    
    // --- COMANDO DE SALVAMENTO ---
    else if (cmd.equalsIgnoreCase("S")) {
      salvarTodosOsSinais();
    }
    
    else {
      Serial.println("Comando invalido. Veja a lista de comandos no boot.");
    }

    Serial.println("\nAguardando proximo comando...");
  }

  delay(10);
}