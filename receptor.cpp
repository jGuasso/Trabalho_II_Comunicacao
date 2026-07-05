#include <iostream>
#include "net_compat.hpp"

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

        buffer[bytes_recebidos] = '\0';
        std::cout << ">" << buffer << std::endl;

        // Envia o ACK de volta para o remetente
        std::string ack_msg = "ACK";
        int bytes_enviados = sendto(sockfd, ack_msg.c_str(), ack_msg.length(), 0, 
                                    (const struct sockaddr*)&sender_addr, addr_len);
        
        if (bytes_enviados == SOCKET_ERROR) {
            std::cerr << "Erro ao enviar ACK. Codigo: " << GETSOCKETERRNO() << std::endl;
        } else {
            std::cout << "[Receptor] ACK enviado." << std::endl;
        }
    }

    CLOSESOCKET(sockfd);
    EncerrarRede();
    return 0;
}