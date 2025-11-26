/* Capture + Save ALL IR signals to a single text file on LittleFS
   - Board: ESP (ESP32 family)
   - Uses IRremote.hpp for sending; capturing via GPIO interrupt timestamps
   - Saves/loads single text file /sinais.txt in a human-readable format
*/

#include <LittleFS.h>
#include <IRremote.hpp>

// --- Configurações de Hardware ---
#define IR_RECEIVER_PIN 34
#define IR_SENDER_PIN   32
#define STATUS_LED_PIN  2
#define MAX_LEN         500  // máximo de "durations" por sinal (uint16_t)
#define NUM_SIGNALS     12   // total de sinais: ligar, desligar, temp16..25

// --- Definição de Índices ---
#define SIG_LIGAR     0
#define SIG_DESLIGAR  1
#define SIG_TEMP_16   2
// Índices de temperatura faltantes, adicionados para clareza:
#define SIG_TEMP_17   3
#define SIG_TEMP_18   4
#define SIG_TEMP_19   5
#define SIG_TEMP_20   6
#define SIG_TEMP_21   7
#define SIG_TEMP_22   8
#define SIG_TEMP_23   9
#define SIG_TEMP_24   10
#define SIG_TEMP_25   11

// --- Buffers de Armazenamento (RAM) ---
uint16_t durations[NUM_SIGNALS][MAX_LEN];
unsigned int capturedSignalLength[NUM_SIGNALS] = {0};

// --- Buffer Temporário para Captura (Volátil) ---
volatile unsigned long irBufferTemp[MAX_LEN];
volatile unsigned int bufferPositionTemp = 0;

// ********************************************
// ROTINAS DE CAPTURA (ISR)
// ********************************************

// 1. Interrupt Service Routine (ISR)
void IRAM_ATTR rxIR_Interrupt_Handler() {
  if (bufferPositionTemp < MAX_LEN) {
    irBufferTemp[bufferPositionTemp++] = micros();
  }
}

// 2. Função de Captura e Conversão
bool captureIR(uint16_t* targetDurations, unsigned int &targetLength, uint32_t waitMs = 5000) {
  Serial.println("Capturando sinal IR... Aponte o controle e aperte o botão.");
  bufferPositionTemp = 0; // Reseta o buffer temporário

  digitalWrite(STATUS_LED_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(IR_RECEIVER_PIN), rxIR_Interrupt_Handler, CHANGE);

  unsigned long start = millis();
  while (millis() - start < waitMs) {
    // espera passiva - ISR preenche o buffer
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

// ********************************************
// ROTINA DE ENVIO
// ********************************************
void sendIR(uint16_t* durationsToSend, unsigned int lengthToSend) {
  if (lengthToSend > 0) {
    Serial.println("Enviando sinal IR...");
    IrSender.sendRaw(durationsToSend, lengthToSend, 38); // 38kHz
    Serial.println("Sinal enviado!");
  } else {
    Serial.println("Nenhum sinal capturado para este comando.");
  }
}

// ********************************************
// PERSISTÊNCIA DE DADOS (LittleFS)
// ********************************************

// Salva todos os sinais capturados para o arquivo /sinais.txt
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
    } else {
      int temp = 16 + (i - SIG_TEMP_16);
      f.print("temp");
      f.print(temp);
      f.print(":");
    }

    // 2. Escreve os DADOS (durações em formato CSV)
    for (unsigned int j = 0; j < capturedSignalLength[i]; j++) {
      f.print(durations[i][j]);
      if (j < capturedSignalLength[i] - 1) f.print(",");
    }
    f.println(); // Quebra de linha para o próximo comando
  }

  f.close();
  Serial.println(">> Todos os sinais foram SALVOS com sucesso em /sinais.txt!");
}

// Carrega todos os sinais do arquivo /sinais.txt para a RAM
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

    // Mapeia o nome do sinal para o índice do array (SIG_LIGAR, SIG_DESLIGAR, etc.)
    if (nome == "ligar") index = SIG_LIGAR;
    else if (nome == "desligar") index = SIG_DESLIGAR;
    else if (nome.startsWith("temp")) {
      int temp = nome.substring(4).toInt();
      if (temp >= 16 && temp <= 25) {
        index = (temp - 16) + SIG_TEMP_16;
      }
    }

    if (index == -1) continue;

    // ** Parsing do CSV **
    int pos = 0;
    unsigned int arrIndex = 0;
    while (pos < lista.length() && arrIndex < MAX_LEN) {
      int comma = lista.indexOf(',', pos);
      String valor;
      if (comma == -1) { // Último valor
        valor = lista.substring(pos);
        pos = lista.length();
      } else {
        valor = lista.substring(pos, comma);
        pos = comma + 1;
      }
      durations[index][arrIndex++] = (uint16_t)valor.toInt(); // Converte string para número
    }
    capturedSignalLength[index] = arrIndex; // Atualiza o comprimento real
  }
  f.close();
  Serial.println(">> Sinais carregados com sucesso. Prontos para uso!");
}

// ********************************************
// UTILITÁRIO
// ********************************************

// Exibe um sinal armazenado no Serial Monitor
void printSignal(int idx) {
  if (idx < 0 || idx >= NUM_SIGNALS || capturedSignalLength[idx] == 0) {
    Serial.println("Sinal vazio ou indice invalido.");
    return;
  }
  
  if (idx == SIG_LIGAR) Serial.print("ligar: ");
  else if (idx == SIG_DESLIGAR) Serial.print("desligar: ");
  else {
    int temp = 16 + (idx - SIG_TEMP_16);
    Serial.print("temp"); Serial.print(temp); Serial.print(": ");
  }
  
  for (unsigned int i = 0; i < capturedSignalLength[idx]; i++) {
    Serial.print(durations[idx][i]);
    if (i < capturedSignalLength[idx] - 1) Serial.print(",");
  }
  Serial.println();
}

// ********************************************
// SETUP & LOOP
// ********************************************

void setup() {
  Serial.begin(115200);           // Use 115200 no Serial Monitor
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(IR_RECEIVER_PIN, INPUT);

  IrSender.begin(IR_SENDER_PIN);

  Serial.println();
  Serial.println("=== IR CAPTURE & SAVE (LittleFS) ===");

  // Inicializa LittleFS (passar 'true' tenta formatar se estiver corrompido)
  if (!LittleFS.begin(true)) {
    Serial.println("ERRO FATAL: Falha ao montar ou formatar LittleFS.");
  } else {
    Serial.println("LittleFS montado.");
    carregarTodosOsSinais(); // Tenta carregar os sinais existentes
  }

  // --- Instruções de Uso ---
  Serial.println("\nComandos via Serial:");
  Serial.println("1 -> capturar LIGAR");
  Serial.println("2 -> capturar DESLIGAR");
  Serial.println("16..25 -> capturar temperatura correspondente");
  Serial.println("L -> enviar LIGAR");
  Serial.println("D -> enviar DESLIGAR");
  Serial.println("T <16-25> -> enviar TEMP (ex: 'T 22')");
  Serial.println("P <nome> -> printar sinal capturado (ex: 'P 20', 'P L')");
  Serial.println("S -> salvar TODOS os sinais na memoria Flash (LittleFS)");
  Serial.println();

  Serial.println("Pronto. Aguarde comandos ou capture sinais.");
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    // Comando de captura por número (1/2/16-25)
    if (cmd == "1") {
      Serial.println("--- Capturando Sinal LIGAR ---");
      while (!captureIR(durations[SIG_LIGAR], capturedSignalLength[SIG_LIGAR])) { delay(200); }
    } else if (cmd == "2") {
      Serial.println("--- Capturando Sinal DESLIGAR ---");
      while (!captureIR(durations[SIG_DESLIGAR], capturedSignalLength[SIG_DESLIGAR])) { delay(200); }
    }
    // single or multi-digit temperature capture
    else if (cmd.toInt() >= 16 && cmd.toInt() <= 25 && cmd.length() <= 3) {
      int temp = cmd.toInt();
      int idx = (temp - 16) + SIG_TEMP_16;
      Serial.print("--- Capturando Sinal TEMP "); Serial.print(temp); Serial.println(" ---");
      while (!captureIR(durations[idx], capturedSignalLength[idx])) { delay(200); }
    }
    
    // Comando de ENVIO
    else if (cmd.equalsIgnoreCase("L")) {
      sendIR(durations[SIG_LIGAR], capturedSignalLength[SIG_LIGAR]);
    } else if (cmd.equalsIgnoreCase("D")) {
      sendIR(durations[SIG_DESLIGAR], capturedSignalLength[SIG_DESLIGAR]);
    } else if (cmd.startsWith("T ")) {
        int temp = cmd.substring(2).toInt();
        if (temp >= 16 && temp <= 25) {
          int idx = (temp - 16) + SIG_TEMP_16;
          sendIR(durations[idx], capturedSignalLength[idx]);
        } else {
          Serial.println("Temperatura T invalida. Use T 16 a T 25.");
        }
    }
    
    // Comando de PRINT
    else if (cmd.startsWith("P ")) {
      // P L, P D ou P 20
      String arg = cmd.substring(2);
      arg.trim();
      if (arg.equalsIgnoreCase("L")) printSignal(SIG_LIGAR);
      else if (arg.equalsIgnoreCase("D")) printSignal(SIG_DESLIGAR);
      else {
        int t = arg.toInt();
        if (t >= 16 && t <= 25) printSignal((t - 16) + SIG_TEMP_16);
        else Serial.println("Argumento P invalido. Use L, D, ou 16..25");
      }
    }
    
    // Comando de SALVAR
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