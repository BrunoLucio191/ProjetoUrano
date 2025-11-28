// ==========================================================
// FUNÇÃO: SALVAR TUDO NA FLASH (LittleFS)
// Grava a matriz da RAM para um arquivo de texto permanente.
// O modo "w" APAGA o arquivo anterior e escreve um novo.
// ==========================================================
void salvarTodosOsSinais() {
  File f = LittleFS.open("/sinais.txt", "w"); 
  
  if (!f) {
    Serial.println("ERRO: Falha ao criar/abrir o arquivo sinais.txt!");
    return;
  }
  
  Serial.println("Salvando sinais (Sobrescrevendo arquivo antigo)...");

  for (int i = 0; i < numSinais; i++) {
    // 1. ESCREVE A ETIQUETA (TAG)
    if (i == SIG_LIGAR) f.print("ligar:");
    else if (i == SIG_DESLIGAR) f.print("desligar:");
    
    else if (i == SIG_SWING_ON) f.print("swing_on:");
    else if (i == SIG_SWING_OFF) f.print("swing_off:");
    
    else if (i >= SIG_MODO_1 && i <= SIG_MODO_5) { 
        int modoNum = i - SIG_MODO_1 + 1;
        f.print("modo"); f.print(modoNum); f.print(":"); 
    }
    else if (i >= SIG_TEMP_16 && i <= SIG_TEMP_30) { 
        int tempNum = (i - SIG_TEMP_16) + 16;
        f.print("temp"); f.print(tempNum); f.print(":"); 
    }

    // 2. ESCREVE OS DADOS (CSV)
    for (unsigned int j = 0; j < pegarTamanhoSinal[i]; j++) {
      f.print(duracoes[i][j]); 
      if (j < pegarTamanhoSinal[i] - 1) f.print(","); 
    }
    
    f.println(); 
  }

  f.close(); 
  Serial.println(">> MEMÓRIA: Arquivo substituido com sucesso!");
}

// ==========================================================
// FUNÇÃO: CARREGAR TUDO DA FLASH
// Lê o arquivo de texto e preenche a matriz na RAM.
// ==========================================================
void carregarTodosOsSinais() {
  File f = LittleFS.open("/sinais.txt", "r");
  if (!f) {
    Serial.println("INFO: Arquivo 'sinais.txt' não encontrado. O sistema iniciou vazio.");
    return;
  }
  
  Serial.println(">> MEMÓRIA: Lendo arquivo de sinais...");

  while (f.available()) {
    String linha = f.readStringUntil('\n');
    linha.trim(); 
    
    if (linha.length() < 3) continue; 
    
    int sep = linha.indexOf(':'); 
    if (sep == -1) continue; 

    String nome = linha.substring(0, sep);
    String lista = linha.substring(sep + 1);
    int index = -1;

    // --- IDENTIFICAÇÃO ESTRITA (Sem Retrocompatibilidade) ---
    if (nome == "ligar") index = SIG_LIGAR;
    else if (nome == "desligar") index = SIG_DESLIGAR;
    
    else if (nome == "swing_on") index = SIG_SWING_ON;
    else if (nome == "swing_off") index = SIG_SWING_OFF;
    
    else if (nome.startsWith("modo")) {
      int modo = nome.substring(4).toInt(); 
      if (modo >= 1 && modo <= 5) index = (modo - 1) + SIG_MODO_1;
    }
    else if (nome.startsWith("temp")) {
      int temp = nome.substring(4).toInt(); 
      if (temp >= 16 && temp <= 30) index = (temp - 16) + SIG_TEMP_16; 
    }

    // --- PARSING DOS DADOS ---
    if (index != -1 && index < numSinais) {
        int pos = 0;
        unsigned int arrIndex = 0;
        
        while (pos < lista.length() && arrIndex < TamMax) {
          int comma = lista.indexOf(',', pos);
          String valorStr;
          
          if (comma == -1) { 
              valorStr = lista.substring(pos); 
              pos = lista.length(); 
          } else { 
              valorStr = lista.substring(pos, comma); 
              pos = comma + 1; 
          }
          
          duracoes[index][arrIndex++] = (uint16_t)valorStr.toInt(); 
        }
        pegarTamanhoSinal[index] = arrIndex; 
    }
  }
  
  f.close(); 
  Serial.println(">> MEMÓRIA: Carga completa!");
}