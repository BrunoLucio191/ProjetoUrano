// ==========================================================
// FUNÇÃO AUXILIAR: TRADUTOR DE COMANDOS
// Converte texto (ex: "SW_ON") para número do índice (ex: 7)
// ==========================================================
int traduzirComando(String cmd) {
    cmd.trim(); // Remove espaços extras
    
    // Comandos Básicos
    if (cmd.equalsIgnoreCase("L")) return SIG_LIGAR;
    if (cmd.equalsIgnoreCase("D")) return SIG_DESLIGAR;
    
    // --- NOVO: SWING DIVIDIDO ---
    if (cmd.equalsIgnoreCase("SW_ON")) return SIG_SWING_ON;   // Índice 7
    if (cmd.equalsIgnoreCase("SW_OFF")) return SIG_SWING_OFF; // Índice 8
    
    // Compatibilidade: Se o site mandar só "SW", assumimos que é LIGAR oscilação
    if (cmd.equalsIgnoreCase("SW")) return SIG_SWING_ON;

    // --- MODOS (M1 a M5) ---
    // Exemplo: MOD3 -> (3-1) + 2 = Índice 4
    if (cmd.startsWith("MOD")) {
        int m = cmd.substring(3).toInt();
        if (m >= 1 && m <= 5) return (m - 1) + SIG_MODO_1;
    }
    
    // --- TEMPERATURAS (T 16 a T 30) ---
    // Exemplo: T 16 -> (16-16) + 9 = Índice 9
    if (cmd.startsWith("T ")) {
        int t = cmd.substring(2).toInt();
        if (t >= 16 && t <= 30) return (t - 16) + SIG_TEMP_16;
    }

    return -1; // Retorna -1 se não encontrou nada conhecido
}

// ==========================================================
// HANDLER: PÁGINA INICIAL (ROOT)
// ==========================================================
void handleRoot(AsyncWebServerRequest *request) {
    String html = "<h1>ESP32 IR Server Online</h1>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Status: O sistema esta pronto para receber comandos do app web.</p>";
    request->send(200, "text/html", html);
}

// ==========================================================
// HANDLER: ENVIAR COMANDO (/enviar)
// ==========================================================
void handleCommand(AsyncWebServerRequest *request) {
    if (!request->hasParam("cmd")) {
        request->send(400, "text/plain", "Erro: Parametro 'cmd' ausente.");
        return;
    }
    
    String cmdStr = request->getParam("cmd")->value();
    
    // Usa a função tradutora que criamos acima
    int indexToSend = traduzirComando(cmdStr);

    if (indexToSend != -1) {
        // Chama a função de envio que está na aba HardwareIR
        sendIR(duracoes[indexToSend], pegarTamanhoSinal[indexToSend]); 
        
        request->send(200, "text/plain", "OK: " + cmdStr);
    } else {
        request->send(400, "text/plain", "Erro: Comando nao reconhecido.");
    }
}