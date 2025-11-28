
// EVENTO: Roda assim que a página carrega
document.addEventListener('DOMContentLoaded', () => {
    carregarOpcoesTemperatura('temp-sala');
    configurarPersistenciaIP(); // <--- LINHA NOVA IMPORTANTE
});

// FUNÇÃO: Preenche o select de temperatura dinamicamente (16 a 30)
function carregarOpcoesTemperatura(elementId) {
    const select = document.getElementById(elementId);
    if (!select) return;

    // Limpa opções antigas para não duplicar se chamar de novo
    select.innerHTML = "";

    for(let i = 16; i <= 30; i++) {
        const option = document.createElement('option');
        option.value = `T ${i}`;
        option.textContent = `${i}°C`;
        // Seleciona 24°C como padrão
        if (i === 24) option.selected = true; 
        select.appendChild(option);
    }
}

// FUNÇÃO AUXILIAR: Envia qualquer comando simples via HTTP
async function sendIrCommand(unitId, command) {
    const ipElement = document.getElementById(`ip-${unitId}`);
    const statusElement = document.getElementById(`status-${unitId}`);
    
    // Validação simples
    if (!ipElement || !ipElement.value) {
        statusElement.textContent = "Erro: IP inválido.";
        statusElement.style.color = "red";
        return;
    }

    const ipAddress = ipElement.value;
    const encodedCommand = encodeURIComponent(command);
    const url = `http://${ipAddress}/enviar?cmd=${encodedCommand}`;

    // Reset status visual
    statusElement.textContent = `Enviando ${command}...`;
    statusElement.style.color = '#555';
    
    try {
        const response = await fetch(url);
        const text = await response.text();
        
        if (response.ok) {
            statusElement.textContent = `OK: ${command} enviado.`;
            statusElement.style.color = 'green';
        } else {
            statusElement.textContent = `ERRO (${response.status}): ${text.substring(0, 50)}...`;
            statusElement.style.color = 'orange';
        }
    } catch (error) {
        statusElement.textContent = `ERRO DE REDE: Verifique o IP ou conexão.`;
        statusElement.style.color = 'red';
        console.error("Detalhe do erro:", error);
    }
}

// FUNÇÃO PRINCIPAL: Monta e envia Temperatura + Modo
function sendCommandFromSelectors(unitId) {
    const tempSelect = document.getElementById(`temp-${unitId}`);
    const modoSelect = document.getElementById(`modo-${unitId}`);
    
    const tempCommand = tempSelect.value;
    const modoCommand = modoSelect.value;

    // Envia o comando de Temperatura
    sendIrCommand(unitId, tempCommand); 
    
    // Pequeno delay para garantir que o ESP32 processe o primeiro sinal antes do segundo
    setTimeout(() => {
        sendIrCommand(unitId, modoCommand); 
    }, 600); 
}

// NOVA FUNÇÃO: Controle de Swing (Fixo vs Oscilar)
function acionarSwing(comando) {
    // O comando recebido será 'SW_ON' ou 'SW_OFF' vindo do HTML
    // Estamos fixando 'sala' aqui, mas poderia ser dinâmico
    sendIrCommand('sala', comando);
}

/* --- LÓGICA DO MENU LATERAL --- */
let menuAberto = false;

function toggleMenu() {
    const sidebar = document.getElementById("Sidebar");
    const overlay = document.getElementById("overlay");

    if (!menuAberto) {
        // ABRIR
        sidebar.style.width = "250px"; 
        overlay.style.display = "block"; 
        menuAberto = true;
    } else {
        // FECHAR
        sidebar.style.width = "0"; 
        overlay.style.display = "none"; 
        menuAberto = false;
    }
}

// --- LÓGICA DE NAVEGAÇÃO ENTRE TELAS ---
function showView(viewName) {
    // Esconde todas as telas
    document.getElementById('view-home').style.display = 'none';
    document.getElementById('view-training').style.display = 'none';
    document.getElementById('view-about').style.display = 'none';
    
    // Mostra a tela escolhida
    if (viewName === 'home') {
        document.getElementById('view-home').style.display = 'block';
    } else if (viewName === 'training') {
        document.getElementById('view-training').style.display = 'block';
        // Garante que o select de treino tenha opções carregadas
        carregarOpcoesTemperatura('temp-treino');
    }
    else if (viewName === 'about') {
        document.getElementById('view-about').style.display = 'block'; // <--- Nova lógica
    }
    
    toggleMenu(); // Fecha o menu lateral automaticamente
}

// --- LÓGICA DE TREINAMENTO (Backend) ---

async function capturar(cmd) {
    const ip = document.getElementById('ip-treino').value;
    const status = document.getElementById('status-treino');
    
    // Rota /treinar ativa o modo de gravação no ESP32
    const url = `http://${ip}/treinar?cmd=${encodeURIComponent(cmd)}`;

    status.textContent = `Aguardando sinal IR para '${cmd}'... APONTE AGORA!`;
    status.style.color = "blue";

    try {
        const response = await fetch(url);
        const text = await response.text();

        if (response.ok) {
            status.textContent = `SUCESSO: '${cmd}' capturado!`;
            status.style.color = "green";
        } else {
            status.textContent = `FALHA: ${text}`;
            status.style.color = "red";
        }
    } catch (error) {
        status.textContent = "Erro de Conexão ou Timeout (5s).";
        status.style.color = "red";
    }
}

function capturarTemp() {
    const temp = document.getElementById('temp-treino').value;
    capturar(temp);
}

// NOVA FUNÇÃO: Captura o modo selecionado no dropdown de treino
function capturarModo() {
    const modo = document.getElementById('modo-treino').value;
    capturar(modo);
}

async function salvarNaMemoria() {
    const ip = document.getElementById('ip-treino').value;
    const status = document.getElementById('status-treino');
    
    status.textContent = "Salvando na memória permanente...";
    
    try {
        await fetch(`http://${ip}/salvar`); 
        status.textContent = "Tudo salvo com sucesso na Flash!";
        status.style.color = "purple";
    } catch (e) {
        status.textContent = "Erro ao salvar.";
        status.style.color = "red";
    }
}

// --- LÓGICA DE MEMÓRIA DO IP (LocalStorage) ---
function configurarPersistenciaIP() {
    const inputHome = document.getElementById('ip-sala');
    const inputTreino = document.getElementById('ip-treino');
    
    // 1. Tenta recuperar o IP salvo na memória do navegador
    const ipSalvo = localStorage.getItem('esp32_ip_address');
    
    // Se existir um IP salvo, preenche os campos automaticamente
    if (ipSalvo) {
        if(inputHome) inputHome.value = ipSalvo;
        if(inputTreino) inputTreino.value = ipSalvo;
    }

    // 2. Função para salvar e sincronizar quando você digita
    function atualizarIP(novoIP) {
        // Salva na memória do navegador
        localStorage.setItem('esp32_ip_address', novoIP);
        
        // Sincroniza os dois campos (se você mudar num, muda no outro)
        if(inputHome && inputHome.value !== novoIP) inputHome.value = novoIP;
        if(inputTreino && inputTreino.value !== novoIP) inputTreino.value = novoIP;
    }

    // 3. Adiciona os "ouvintes" nos inputs
    if(inputHome) {
        inputHome.addEventListener('input', (e) => atualizarIP(e.target.value));
    }
    if(inputTreino) {
        inputTreino.addEventListener('input', (e) => atualizarIP(e.target.value));
    }
}