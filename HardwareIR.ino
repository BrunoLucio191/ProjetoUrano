// ==========================================================
// INTERRUPÇÃO (ISR): O OUVIDO BIÔNICO
// Executada automaticamente quando a voltagem no pino muda.
// O 'IRAM_ATTR' garante que ela rode na memória RAM (rápida).
// ==========================================================
void IRAM_ATTR pegarSinalAgora() {
  if (posiTempoBruto < TamMax) {
    temposBrutos[posiTempoBruto++] = micros(); 
  }
}

// ==========================================================
// FUNÇÃO DE CAPTURA (GRAVAÇÃO)
// ==========================================================
// Nota: O valor padrão de espera (5000) está definido no Main.ino,
// por isso não repetimos aqui.
// ==========================================================
// FUNÇÃO DE CAPTURA (GRAVAÇÃO) COM SAÍDA INTELIGENTE
// ==========================================================
bool captureIR(uint16_t* duracoesCapturadas, unsigned int &quantidadeCapturada, uint32_t esperarMs) {
  
  // 1. PREPARAÇÃO
  Serial.println("--- INICIANDO CAPTURA ---");
  posiTempoBruto = 0; 
  digitalWrite(pinLedStatus, HIGH); 

  attachInterrupt(digitalPinToInterrupt(IR_PinoReceber), pegarSinalAgora, CHANGE);

  // 2. JANELA DE TEMPO INTELIGENTE
  unsigned long start = millis();
  
  // Variáveis para detectar o fim do sinal
  unsigned int ultimoTamanho = 0;
  unsigned long tempoUltimaMudanca = millis();

  while (millis() - start < esperarMs) {
    delay(10); // Pequena pausa
    
    // --- LÓGICA DE DETECÇÃO DE FIM DE SINAL ---
    if (posiTempoBruto > 0) {
        // Se o tamanho mudou desde a última vez, o sinal ainda está chegando
        if (posiTempoBruto != ultimoTamanho) {
            ultimoTamanho = posiTempoBruto;
            tempoUltimaMudanca = millis(); // Reseta o cronômetro de silêncio
        }
        else {
            // Se o tamanho NÃO mudou, verificamos há quanto tempo está em silêncio
            // Se passou 200ms sem novos pulsos, assumimos que o sinal acabou
            if (millis() - tempoUltimaMudanca > 200) {
                break; // SAI DO LOOP IMEDIATAMENTE!
            }
        }
    }
    
    // Segurança de estouro de memória
    if (posiTempoBruto >= TamMax) break;
  }

  // 3. FINALIZAÇÃO
  detachInterrupt(digitalPinToInterrupt(IR_PinoReceber)); 
  digitalWrite(pinLedStatus, LOW); 

  // 4. PROCESSAMENTO
  if (posiTempoBruto > 10) { 
    unsigned int tam = (posiTempoBruto > 0) ? (posiTempoBruto - 1) : 0;
    if (tam > TamMax) tam = TamMax;
    
    for (unsigned int i = 0; i < tam; i++) {
      unsigned long diff = temposBrutos[i + 1] - temposBrutos[i];
      if (diff > 65535UL) duracoesCapturadas[i] = 65535;
      else duracoesCapturadas[i] = (uint16_t)diff;
    }
    
    quantidadeCapturada = tam; 
    
    Serial.print("Sucesso! Pulsos: ");
    Serial.println(quantidadeCapturada);
    return true; // Isso envia o "200 OK" pro site instantaneamente

  } else {
    Serial.println("Falha: Timeout ou Ruido.");
    quantidadeCapturada = 0;
    return false;
  }
}

// ==========================================================
// FUNÇÃO DE ENVIO (PLAYBACK)
// ==========================================================
void sendIR(uint16_t* durations, unsigned int length) {
  if (length > 0) {
    digitalWrite(pinLedStatus, HIGH); 
    Serial.print("Enviando IR... Tamanho: ");
    Serial.println(length);
    
    // Envia o sinal bruto na frequência de 38kHz (Padrão AC)
    IrSender.sendRaw(durations, length, 38); 
    
    Serial.println("Enviado.");
    digitalWrite(pinLedStatus, LOW);
  } else {
    Serial.println("Erro: Sinal vazio (Tamanho 0). Grave antes de usar.");
  }
}