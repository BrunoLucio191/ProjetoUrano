#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <IRremote.hpp>

// ==========================================================
// 1. CONFIGURAÇÕES DE REDE E SERVIDOR
// ==========================================================
const char* ssid ="JESSICA_NET";
const char* password ="14081925";

// Cria o objeto do servidor na porta padrão 80 (HTTP)
// Async significa que ele não trava o processador enquanto envia arquivos
AsyncWebServer server(80); 

// ==========================================================
// 2. CONFIGURAÇÕES DE HARDWARE (PINOS)
// ==========================================================
#define IR_PinoReceber 34 // Onde o sensor IR (3 pernas) está ligado
#define irPinoEnvio    32 // Onde o LED Emissor IR está ligado
#define pinLedStatus   2  // LED da placa para feedback visual

// ==========================================================
// 3. DEFINIÇÃO DE TAMANHOS E ÍNDICES (O MAPA DA MEMÓRIA)
// ==========================================================
#define TamMax         500  // Tamanho máximo de um sinal (pulsos)
#define numSinais      24   // Total de comandos que vamos aprender

// --- COMANDOS BÁSICOS (0-1) ---
#define SIG_LIGAR       0
#define SIG_DESLIGAR    1

// --- MODOS DE OPERAÇÃO (2-6) ---
#define SIG_MODO_1      2 // Frio
#define SIG_MODO_2      3 // Seco
#define SIG_MODO_3      4 // Ventilar
#define SIG_MODO_4      5 // Quente
#define SIG_MODO_5      6 // Auto

// --- CONTROLE DE ALETAS/SWING (7-8) ---
// Dividimos em dois para ter controle total (Fixo e Oscilar)
#define SIG_SWING_ON    7  
#define SIG_SWING_OFF   8  

// --- TEMPERATURAS (9-23) ---
// Listando todas explicitamente como você pediu
#define SIG_TEMP_16     9
#define SIG_TEMP_17     10
#define SIG_TEMP_18     11
#define SIG_TEMP_19     12
#define SIG_TEMP_20     13
#define SIG_TEMP_21     14
#define SIG_TEMP_22     15
#define SIG_TEMP_23     16
#define SIG_TEMP_24     17
#define SIG_TEMP_25     18
#define SIG_TEMP_26     19
#define SIG_TEMP_27     20
#define SIG_TEMP_28     21
#define SIG_TEMP_29     22
#define SIG_TEMP_30     23 
// Fim dos índices (Total = 24 itens, de 0 a 23)

// ==========================================================
// 4. VARIÁVEIS GLOBAIS (MEMÓRIA RAM)
// ==========================================================

// MATRIZ PRINCIPAL: Guarda todos os sinais processados.
// É como uma planilha Excel com 24 linhas (sinais) e 500 colunas (pulsos).
uint16_t duracoes[numSinais][TamMax]; 

// VETOR DE TAMANHOS: Guarda quantos pulsos cada sinal tem de verdade.
// Ex: O sinal de LIGAR tem 150 pulsos, o de DESLIGAR tem 152...
unsigned int pegarTamanhoSinal[numSinais] = {0}; 

// VARIÁVEIS DE RASCUNHO (SCRATCHPAD):
// Usadas apenas durante a captura ultra-rápida (Interrupção).
// 'volatile' avisa o processador que isso muda a qualquer instante.
volatile unsigned long temposBrutos[TamMax];
volatile unsigned int posiTempoBruto = 0;

// ==========================================================
// 5. PROTÓTIPOS (AVISOS PRÉVIOS PARA O COMPILADOR)
// ==========================================================
// Como separamos o código em abas, precisamos avisar aqui que essas funções existem.

// Avisa que captureIR existe e define o padrão de espera para 5000ms aqui.
// (Nas outras abas, não pode repetir o = 5000)
bool captureIR(uint16_t* duracoesCapturadas, unsigned int &quantidadeCapturada, uint32_t esperarMs = 5000);

// Funções que estão na aba 'Memoria.ino'
void salvarTodosOsSinais();
void carregarTodosOsSinais();

// Funções que estão na aba 'Web.ino'
void handleCommand(AsyncWebServerRequest *request);
void handleRoot(AsyncWebServerRequest *request);
int traduzirComando(String cmd);

// Funções que estão na aba 'HardwareIR.ino'
void sendIR(uint16_t* durations, unsigned int length);
void pegarSinalAgora();

// ==========================================================
// 6. SETUP (INICIALIZAÇÃO DO SISTEMA)
// ==========================================================
void setup() {
  Serial.begin(115200); // Inicia comunicação com o computador via USB
  
  // Configura os pinos físicos
  pinMode(pinLedStatus, OUTPUT); // LED
  pinMode(IR_PinoReceber, INPUT); // Sensor IR
  IrSender.begin(irPinoEnvio);    // LED Emissor
  
  // Tenta montar o sistema de arquivos (HD do ESP32)
  if (!LittleFS.begin(true)) {
    Serial.println("ERRO FATAL: Falha ao montar LittleFS. O salvamento não vai funcionar.");
  } else {
    Serial.println("LittleFS montado com sucesso.");
    // Ao ligar, tenta ler o arquivo 'sinais.txt' para recuperar o que já foi gravado
    carregarTodosOsSinais(); 
  }

  // --- CORREÇÃO DE CORS (CRUCIAL PARA O SITE FUNCIONAR) ---
  // Isso coloca um "crachá VIP" em todas as respostas, permitindo que o navegador
  // aceite os dados mesmo vindo de um IP local diferente.
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // Conexão Wi-Fi
  Serial.print("\nConectando-se a: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  // Aguarda conexão (Timeout de 30 segundos)
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 30000) { 
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n--- WI-FI CONECTADO! ---");
    Serial.print("Endereço IP para acessar o site: ");
    Serial.println(WiFi.localIP());

    // --- DEFINIÇÃO DAS ROTAS (ENDPOINTS) DO SITE ---
    
    // 1. Rota Principal: Mostra uma mensagem se acessar pelo navegador
    server.on("/", HTTP_GET, handleRoot);         
    
    // 2. Rota de Envio: Usada para CONTROLAR o ar (Ligar, Desligar...)
    server.on("/enviar", HTTP_GET, handleCommand); 

    // 3. Rota de Treinamento: Usada para GRAVAR novos sinais
    server.on("/treinar", HTTP_GET, [](AsyncWebServerRequest *request){
       // Verifica se mandou o comando (ex: ?cmd=L)
       if (!request->hasParam("cmd")) { request->send(400, "text/plain", "Erro: Falta parametro cmd"); return; }
       
       String cmd = request->getParam("cmd")->value();
       
       // Traduz "L" para o número 0, "SW_ON" para 7, etc.
       int idx = traduzirComando(cmd); 

       if (idx == -1) { request->send(400, "text/plain", "Comando Desconhecido"); return; }

       // CHAMA A CAPTURA: O ESP32 vai parar aqui por até 5 segundos
       bool sucesso = captureIR(duracoes[idx], pegarTamanhoSinal[idx]);

       if (sucesso) request->send(200, "text/plain", "Sinal Capturado com Sucesso!");
       else request->send(500, "text/plain", "Falha: Tempo esgotou ou Ruido");
    });

    // 4. Rota de Salvamento: Grava da RAM para a Flash
    server.on("/salvar", HTTP_GET, [](AsyncWebServerRequest *request){
       salvarTodosOsSinais(); 
       request->send(200, "text/plain", "Todos os sinais foram salvos na memoria permanente!");
    });

    server.begin(); // Inicia o servidor
    Serial.println("Servidor Web Online.");
  } else {
    Serial.println("\nFalha no Wi-Fi. O ESP continuara funcionando via Monitor Serial.");
  }
}

// ==========================================================
// 7. LOOP (CONTROLE MANUAL VIA SERIAL)
// ==========================================================
// Este loop serve para você testar ou treinar o ESP32 usando o teclado do computador,
// caso o Wi-Fi não esteja funcionando ou para testes rápidos.

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim(); // Remove espaços e quebras de linha
    if (cmd.length() == 0) return;

    // --- TREINAMENTO VIA SERIAL ---
    if (cmd == "1") {
      Serial.println("Treinando LIGAR...");
      while (!captureIR(duracoes[SIG_LIGAR], pegarTamanhoSinal[SIG_LIGAR])) { delay(200); }
    }
    else if (cmd == "2") {
      Serial.println("Treinando DESLIGAR...");
      while (!captureIR(duracoes[SIG_DESLIGAR], pegarTamanhoSinal[SIG_DESLIGAR])) { delay(200); }
    }
    
    // Teste dos Swing separados
    else if (cmd.equalsIgnoreCase("SW_ON")) { 
      Serial.println("Treinando SWING ON...");
      while (!captureIR(duracoes[SIG_SWING_ON], pegarTamanhoSinal[SIG_SWING_ON])) { delay(200); }
    }
    else if (cmd.equalsIgnoreCase("SW_OFF")) { 
      Serial.println("Treinando SWING OFF...");
      while (!captureIR(duracoes[SIG_SWING_OFF], pegarTamanhoSinal[SIG_SWING_OFF])) { delay(200); }
    }

    // Treinamento de Modos (M1 a M5)
    else if (cmd.startsWith("M") && cmd.length() == 2) { 
        int modo = cmd.substring(1).toInt();
        if (modo >= 1 && modo <= 5) {
            int idx = (modo - 1) + SIG_MODO_1;
            Serial.printf("--- Treinando MODO %d ---\n", modo);
            while (!captureIR(duracoes[idx], pegarTamanhoSinal[idx])) { delay(200); }
        } else {
            Serial.println("Erro: Use M1 ate M5.");
        }
    }

    // Treinamento de Temperaturas (16 a 30)
    else if (cmd.toInt() >= 16 && cmd.toInt() <= 30 && cmd.length() <= 3) {
      int temp = cmd.toInt();
      int idx = (temp - 16) + SIG_TEMP_16;
      Serial.printf("--- Treinando TEMP %d ---\n", temp);
      while (!captureIR(duracoes[idx], pegarTamanhoSinal[idx])) { delay(200); }
    }
    
    // --- TESTE DE ENVIO (DISPARAR O LED) ---
    else if (cmd.equalsIgnoreCase("L")) sendIR(duracoes[SIG_LIGAR], pegarTamanhoSinal[SIG_LIGAR]);
    else if (cmd.equalsIgnoreCase("D")) sendIR(duracoes[SIG_DESLIGAR], pegarTamanhoSinal[SIG_DESLIGAR]);
    else if (cmd.equalsIgnoreCase("SON")) sendIR(duracoes[SIG_SWING_ON], pegarTamanhoSinal[SIG_SWING_ON]);
    else if (cmd.equalsIgnoreCase("SOFF")) sendIR(duracoes[SIG_SWING_OFF], pegarTamanhoSinal[SIG_SWING_OFF]);
    
    // --- SALVAR TUDO ---
    else if (cmd.equalsIgnoreCase("S")) {
      salvarTodosOsSinais();
    }
    
    else {
      Serial.println("Comando invalido. Tente: 1, 2, 16..30, M1..M5, SW_ON, SW_OFF ou S (Salvar).");
    }

    Serial.println("\nAguardando proximo comando...");
  }

  delay(10); // Pequena pausa para aliviar a CPU
}