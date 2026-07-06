#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include "net_compat.hpp"
#include "framing.hpp"

std::atomic<bool> executando(true); // Controle de execução da thread de recepção

const uint8_t M = 4;
const uint8_t MAX_SEQ = 16; // 2^m 
const uint8_t S_TAMANHO = 15; // 2^m - 1 

std::mutex mtx_janela;
uint8_t Sf = 0; // Primeiro frame pendente 
uint8_t Sn = 0; // Próximo frame a ser enviado 
std::map<uint8_t, std::vector<uint8_t>> frames_pendentes; // Preserva cópia dos frames 
auto timer_start = std::chrono::steady_clock::now();
bool timer_running = false;

std::vector<std::string> fila_mensagens; // Fila de mensagens digitadas pelo usuário
std::mutex mtx_fila;

void thread_recepcao(SOCKET sockfd) {
    char buffer[2048];
    sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (executando) {
        int bytes_recebidos = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                                       (struct sockaddr*)&sender_addr, &addr_len);
        
        if (bytes_recebidos == SOCKET_ERROR) {
            int err = GETSOCKETERRNO();

            if (executando) {
                std::cerr << "\n[Thread Recepção] Erro ou socket encerrado. Codigo: " << err << std::endl;
            }
            break; // Interrompe se houver erro ou encerramento
        }
        // Desempacotar o quadro recebido (ACK do receptor)
        std::vector<uint8_t> quadro_bruto(buffer, buffer + bytes_recebidos);
        uint16_t tipo_recebido = 0;
        uint8_t ackNo = 0;
        std::string rec_msg = desmontar_quadro(quadro_bruto, &tipo_recebido, &ackNo);

        if (!rec_msg.empty() && tipo_recebido == TIPO_ACK) {
            std::lock_guard<std::mutex> lock(mtx_janela);
            
            // Verifica se o ACK está dentro da janela pendente
            bool ack_valido = false;
            uint8_t temp = Sf;
            while (temp != Sn) {
                if (temp == ackNo) break;
                temp = (temp + 1) % MAX_SEQ;
                if (temp == ackNo) ack_valido = true; // Confirmação cumulativa
            }

            if (ack_valido || ackNo == Sn) { // Se o ackNo confirmar os pendentes
                std::cout << "\n[Thread ACK] Recebido ACK " << (int)ackNo << ". Avançando janela." << std::endl;
                
                // Limpa buffers e desliza a parede esquerda (Sf) [cite: 236]
                while (Sf != ackNo) {
                    frames_pendentes.erase(Sf);
                    Sf = (Sf + 1) % MAX_SEQ;
                }
                
                if (Sf == Sn) {
                    timer_running = false; // StopTimer() 
                } else {
                    timer_start = std::chrono::steady_clock::now(); // Reinicia timer
                }
            }
        }
    }
}

void loop_timer_e_envio(SOCKET sockfd, sockaddr_in receiver_addr) {
    const auto TIMEOUT_MS = std::chrono::milliseconds(2000); // 2 segundos de timeout

    while (executando) {
        mtx_janela.lock();
        
        // 1. Verifica Timeout 
        if (timer_running && (std::chrono::steady_clock::now() - timer_start) > TIMEOUT_MS) {
            std::cout << "\n[TIMEOUT] Timer esgotado para o frame " << (int)Sf << ". Reenviando pendentes." << std::endl;
            
            // Reenvia todos os frames pendentes 
            uint8_t temp = Sf;
            while (temp != Sn) {
                auto& quadro = frames_pendentes[temp];
                sendto(sockfd, reinterpret_cast<const char*>(quadro.data()), quadro.size(), 0, 
                       (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
                temp = (temp + 1) % MAX_SEQ;
            }
            timer_start = std::chrono::steady_clock::now(); // Reinicia timer
        }
        
        // 2. Lógica de leitura de nova mensagem da fila e Envio
        // Calcula a quantidade de frames atualmente pendentes na janela
        int frames_em_voo = (Sn >= Sf) ? (Sn - Sf) : (MAX_SEQ - Sf + Sn);
        
        // Só envia se a janela NÃO estiver repleta
        if (frames_em_voo < S_TAMANHO) {
            std::string nova_mensagem = "";
            
            // Bloqueia a fila rapidinho para pegar a mensagem
            mtx_fila.lock();
            if (!fila_mensagens.empty()) {
                nova_mensagem = fila_mensagens.front();
                fila_mensagens.erase(fila_mensagens.begin()); // Remove da fila
            }
            mtx_fila.unlock();

            if (!nova_mensagem.empty()) {
                // Empacotar a mensagem em um quadro (agora passando o Sn)
                std::vector<uint8_t> quadro = montar_quadro(nova_mensagem, TIPO_DATA, Sn);
                
                // Armazena a cópia para possível retransmissão
                frames_pendentes[Sn] = quadro;

                // Envia o quadro via UDP
                sendto(sockfd, reinterpret_cast<const char*>(quadro.data()), quadro.size(), 0, 
                       (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));

                std::cout << "\n[Enviado] Quadro " << (int)Sn << " disparado (" << quadro.size() << " bytes)." << std::endl;
                
                // Incrementa o número de sequência usando módulo 2^m
                Sn = (Sn + 1) % MAX_SEQ;

                // Se o timer não estiver rodando, inicia ele (para o primeiro frame pendente)
                if (!timer_running) {
                    timer_start = std::chrono::steady_clock::now();
                    timer_running = true;
                }
            }
        }

        mtx_janela.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Evita consumir 100% da CPU
    }
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Erro: Faltam argumentos!" << std::endl;
        std::cerr << "Uso correto: " << argv[0] << " <ENDERECO_IP>" << std::endl;
        return 1;
    }

    if (!IniciarRede()) {
        std::cerr << "Falha ao iniciar a rede!" << std::endl;
        return 1;
    }

    // Criação do socket UDP
    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (!ISVALIDSOCKET(sockfd)) {
        std::cerr << "Erro ao criar socket. Codigo: " << GETSOCKETERRNO() << std::endl;
        EncerrarRede();
        return 1;
    }

    std::string ip_destino = argv[1];

    // Configuração do endereço do receptor (destino)
    sockaddr_in receiver_addr;
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(8080); // Porta de destino
    inet_pton(AF_INET, ip_destino.c_str(), &receiver_addr.sin_addr); // Converte IP string para binário

    // Configuração do endereço local (para recebimento de ACKs)
    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(0); // SO escolhe uma porta livre dinamicamente
    
    if (bind(sockfd, (const struct sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
        std::cerr << "Erro ao fazer bind no emissor. Codigo: " << GETSOCKETERRNO() << std::endl;
    }

    // Inicia threads assíncronas
    std::thread t_recepcao(thread_recepcao, sockfd);
    std::thread t_envio_timer(loop_timer_e_envio, sockfd, receiver_addr);

    std::string mensagem;
    std::cout << "Digite as mensagens para enviar (ou 'sair' para encerrar):" << std::endl;

    while (true) {
        std::getline(std::cin, mensagem);

        if (mensagem == "sair") {
            executando = false; // Sinaliza para as threads pararem
            break;
        }

        if (!mensagem.empty()) {
            std::lock_guard<std::mutex> lock(mtx_fila);
            fila_mensagens.push_back(mensagem);
        }
    }

    // Encerramento e limpeza
    CLOSESOCKET(sockfd);
    t_recepcao.join();
    t_envio_timer.join(); // Aguarda a thread de envio finalizar

    EncerrarRede();
    return 0;
}