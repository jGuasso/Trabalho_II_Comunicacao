#include <iostream>
#include <vector>
#include "net_compat.hpp"
#include "framing.hpp"

int main() {
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

    // Configuração do endereço local (porta 8080)
    sockaddr_in receiver_addr;
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = INADDR_ANY; // Aceita pacotes de qualquer interface de rede (IP)
    receiver_addr.sin_port = htons(8080);

    if (bind(sockfd, (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr)) == SOCKET_ERROR) {
        std::cerr << "Erro no Bind. Codigo: " << GETSOCKETERRNO() << std::endl;
        CLOSESOCKET(sockfd);
        EncerrarRede();
        return 1;
    }

    std::cout << "Receptor pronto e aguardando pacotes na porta 8080..." << std::endl;

    char buffer[2048];
    sockaddr_in sender_addr;
    
    socklen_t addr_len = sizeof(sender_addr);
    while (true) {
        int bytes_recebidos = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                                       (struct sockaddr*)&sender_addr, &addr_len);

        if (bytes_recebidos == SOCKET_ERROR) {
            std::cerr << "Erro ao receber dados. Codigo: " << GETSOCKETERRNO() << std::endl;
            break;
        }

        // Desempacotar o quadro recebido (framing)
        std::vector<uint8_t> quadro_bruto(buffer, buffer + bytes_recebidos);
        uint16_t tipo_recebido = 0;
        std::string mensagem = desmontar_quadro(quadro_bruto, &tipo_recebido);

        if (mensagem.empty()) {
            std::cerr << "[Receptor] Quadro inválido ou corrompido. Descartado (sem ACK)." << std::endl;
            continue; // Não envia ACK para quadros corrompidos
        }

        std::cout << ">" << mensagem << std::endl;
        imprimir_quadro_hex(quadro_bruto); // Depuração: exibe o quadro recebido

        // Empacotar e enviar o ACK de volta para o remetente
        std::vector<uint8_t> quadro_ack = montar_quadro("ACK", TIPO_ACK);
        int bytes_enviados = sendto(sockfd, reinterpret_cast<const char*>(quadro_ack.data()), quadro_ack.size(), 0, 
                                    (const struct sockaddr*)&sender_addr, addr_len);
        
        if (bytes_enviados == SOCKET_ERROR) {
            std::cerr << "Erro ao enviar ACK. Codigo: " << GETSOCKETERRNO() << std::endl;
        } else {
            std::cout << "[Receptor] ACK enviado (quadro de " << quadro_ack.size() << " bytes)." << std::endl;
        }
    }

    CLOSESOCKET(sockfd);
    EncerrarRede();
    return 0;
}