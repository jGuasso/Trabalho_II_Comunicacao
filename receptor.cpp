#include <iostream>
#include <vector>
#include <random>
#include "net_compat.hpp"
#include "framing.hpp"

// Configuração do gerador de números aleatórios
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> distrib_porcentagem(1, 100);

/**
 * Retorna true se o evento deve ocorrer, baseado na chance (0 a 100)
 */
bool deve_ocorrer(int chance) {
    return distrib_porcentagem(gen) <= chance;
}

/**
 * Inverte um bit aleatório do quadro para simular ruído na linha
 */
void simular_corrupcao(std::vector<uint8_t>& quadro, int chance_erro) {
    if (deve_ocorrer(chance_erro) && !quadro.empty()) {
        // Escolhe um byte aleatório do quadro
        std::uniform_int_distribution<> distrib_byte(0, quadro.size() - 1);
        int indice_byte = distrib_byte(gen);

        // Escolhe um bit aleatório (0 a 7)
        std::uniform_int_distribution<> distrib_bit(0, 7);
        int indice_bit = distrib_bit(gen);

        // Aplica uma máscara XOR para inverter exatamente aquele bit
        quadro[indice_byte] ^= (1 << indice_bit);
        
        std::cout << "\n[SIMULACAO] Ruído injetado! Bit " << indice_bit 
                  << " do byte " << indice_byte << " foi invertido." << std::endl;
    }
}

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

    uint8_t Rn = 0; // Janela de recepção, próximo frame esperado 
    const uint8_t MAX_SEQ = 16; // 2^m onde m=4

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

        int CHANCE_PERDA = 20; // 20% de chance de perder o quadro inteiro
        int CHANCE_ERRO  = 30; // 30% de chance de corromper um bit

        if (deve_ocorrer(CHANCE_PERDA)) {
            std::cout << "\n[SIMULACAO] Quadro perdido na rede! (Ignorando pacote)" << std::endl;
            continue; // Pula o resto do loop, agindo como se o quadro nunca tivesse chegado
        }

        // Se não foi perdido, pode ter sido corrompido
        simular_corrupcao(quadro_bruto, CHANCE_ERRO);

        uint16_t tipo_recebido = 0;
        uint8_t seq_recebido = 0;
        std::string mensagem = desmontar_quadro(quadro_bruto, &tipo_recebido, &seq_recebido);

        if (mensagem.empty()) {
            std::cerr << "[Receptor] Quadro corrompido. Ignorando." << std::endl;
            continue; // Se um frame for corrompido, o receptor permanece em silêncio 
        }

        if (tipo_recebido == TIPO_DATA) {
            if (seq_recebido == Rn) {
                // Quadro correto e na ordem
                std::cout << "\n[DATA " << (int)seq_recebido << "] " << mensagem << std::endl;
                
                // Desliza a janela e calcula módulo 2^m
                Rn = (Rn + 1) % MAX_SEQ; 
            } else {
                std::cout << "\n[Receptor] Quadro fora de ordem recebido (" << (int)seq_recebido << "). Esperado: " << (int)Rn << ". Descartando." << std::endl;
                // Descarta, mas reenvia o ACK do próximo frame esperado para ajudar o emissor
            }

            // Envia ACK cumulativo com o ackNo do próximo frame esperado
            std::vector<uint8_t> quadro_ack = montar_quadro("ACK", TIPO_ACK, Rn);
            sendto(sockfd, reinterpret_cast<const char*>(quadro_ack.data()), quadro_ack.size(), 0, 
                   (const struct sockaddr*)&sender_addr, addr_len);
            std::cout << "[Receptor] Enviado ACK " << (int)Rn << std::endl;
        }
    }

    CLOSESOCKET(sockfd);
    EncerrarRede();
    return 0;
}