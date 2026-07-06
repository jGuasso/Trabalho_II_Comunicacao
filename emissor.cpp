#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "net_compat.hpp"
#include "framing.hpp"

std::atomic<bool> executando(true); // Controle de execução da thread de recepção

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
        std::string rec_msg = desmontar_quadro(quadro_bruto, &tipo_recebido);

        if (rec_msg.empty()) {
            std::cerr << "\n[Thread Recepção] Quadro recebido inválido ou corrompido." << std::endl;
        } else if (rec_msg == "ACK" && tipo_recebido == TIPO_ACK) {
            std::cout << "\n[ACK] Mensagem confirmada pelo receptor!" << std::endl;
        }
        
        std::cout << "> ";
        std::cout.flush();
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

    // Inicia thread para escutar ACKs de forma assíncrona
    std::thread t_recepcao(thread_recepcao, sockfd);

    std::string mensagem;
    std::cout << "Digite as mensagens para enviar (ou 'sair' para encerrar):" << std::endl;

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, mensagem);

        if (mensagem == "sair") {
            break;
        }

        // Empacotar a mensagem em um quadro (framing)
        std::vector<uint8_t> quadro = montar_quadro(mensagem, TIPO_DATA);
        imprimir_quadro_hex(quadro); // Depuração: exibe o quadro montado

        // Envia o quadro via UDP
        int bytes_enviados = sendto(sockfd, reinterpret_cast<const char*>(quadro.data()), quadro.size(), 0, 
                                    (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));

        if (bytes_enviados == SOCKET_ERROR) {
            std::cerr << "Erro ao enviar. Codigo: " << GETSOCKETERRNO() << std::endl;
        } else {
            std::cout << "Quadro enviado com sucesso! (" << quadro.size() << " bytes)" << std::endl;
        }
    }

    // Encerramento e limpeza
    executando = false;
    CLOSESOCKET(sockfd);
    t_recepcao.join();

    EncerrarRede();
    return 0;
}