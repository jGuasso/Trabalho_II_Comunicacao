#ifndef FRAMING_HPP
#define FRAMING_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <cstring>
#include "crc.h"

/* =========================================
   CONSTANTES DO PROTOCOLO DE ENQUADRAMENTO
   ========================================= */

// Byte de FLAG — delimita início e fim do quadro
const uint8_t FLAG_BYTE = 0x7E;

// Byte de ESCAPE — usado para Byte Stuffing
const uint8_t ESC_BYTE = 0x7D;

// Bytes de substituição após o ESC (XOR com 0x20)
// FLAG (0x7E) dentro do payload → ESC + 0x5E
// ESC  (0x7D) dentro do payload → ESC + 0x5D
const uint8_t FLAG_XOR = 0x5E; // 0x7E ^ 0x20
const uint8_t ESC_XOR  = 0x5D; // 0x7D ^ 0x20

// Tipos de quadro
const uint16_t TIPO_DATA = 0x0001;
const uint16_t TIPO_ACK  = 0x0002;

// Tamanho dos campos em bytes
const int TAM_MAC     = 6;
const int TAM_TIPO    = 2;
const int TAM_CRC     = 2;
const int TAM_SEQ = 1;
const int TAM_HEADER = TAM_MAC + TAM_MAC + TAM_TIPO + TAM_SEQ; // 15 vytes pro header

/* =========================================
   ENDEREÇOS MAC SIMULADOS
   ========================================= */

// MAC de origem simulado (ex: AA:AA:AA:AA:AA:AA)
const uint8_t MAC_ORIGEM[TAM_MAC]  = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

// MAC de destino simulado (ex: BB:BB:BB:BB:BB:BB)
const uint8_t MAC_DESTINO[TAM_MAC] = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB};


/* =========================================
   BYTE STUFFING
   ========================================= */

/**
 * @brief Realiza o Byte Stuffing nos dados.
 * 
 * Percorre cada byte dos dados originais. Se encontrar um byte
 * igual a FLAG (0x7E) ou ESC (0x7D), substitui por:
 *   FLAG → ESC + FLAG_XOR (0x7D 0x5E)
 *   ESC  → ESC + ESC_XOR  (0x7D 0x5D)
 * 
 * Isso garante que o byte FLAG nunca aparecerá dentro do payload,
 * evitando que o receptor confunda dados com fim de quadro.
 */
inline std::vector<uint8_t> byte_stuffing(const std::vector<uint8_t>& dados) {
    std::vector<uint8_t> resultado;
    resultado.reserve(dados.size() * 2); // Pior caso: todos os bytes precisam de escape

    for (uint8_t byte : dados) {
        if (byte == FLAG_BYTE) {
            resultado.push_back(ESC_BYTE);
            resultado.push_back(FLAG_XOR);
        } else if (byte == ESC_BYTE) {
            resultado.push_back(ESC_BYTE);
            resultado.push_back(ESC_XOR);
        } else {
            resultado.push_back(byte);
        }
    }

    return resultado;
}

/**
 * @brief Reverte o Byte Stuffing, restaurando os dados originais.
 * 
 * Percorre os dados recebidos. Ao encontrar um ESC (0x7D),
 * lê o próximo byte e aplica XOR com 0x20 para recuperar
 * o valor original:
 *   ESC + 0x5E → FLAG (0x7E)
 *   ESC + 0x5D → ESC  (0x7D)
 */
inline std::vector<uint8_t> byte_destuffing(const std::vector<uint8_t>& dados) {
    std::vector<uint8_t> resultado;
    resultado.reserve(dados.size());

    for (size_t i = 0; i < dados.size(); i++) {
        if (dados[i] == ESC_BYTE && (i + 1) < dados.size()) {
            // O próximo byte foi XOR'ado com 0x20, então revertemos
            resultado.push_back(dados[i + 1] ^ 0x20);
            i++; // Pula o byte seguinte, já foi consumido
        } else {
            resultado.push_back(dados[i]);
        }
    }

    return resultado;
}


/* =========================================
   CRC (VERIFICAÇÃO DE ERROS)
   Integração com o módulo crc.h
   ========================================= */

/**
 * @brief Converte um vetor de bytes (uint8_t) para um vetor de bits (int).
 * 
 * Cada byte é expandido em 8 bits (MSB primeiro), que é o formato
 * esperado pelas funções gera_crc() e verifica_crc() do módulo crc.h.
 */
inline std::vector<int> bytes_para_bits(const std::vector<uint8_t>& bytes) {
    std::vector<int> bits;
    bits.reserve(bytes.size() * 8);
    for (uint8_t byte : bytes) {
        for (int i = 7; i >= 0; i--) {
            bits.push_back((byte >> i) & 1);
        }
    }
    return bits;
}

/**
 * @brief Converte um vetor de bits (int) para um vetor de bytes (uint8_t).
 * 
 * Agrupa cada 8 bits consecutivos em um byte (MSB primeiro).
 * Se o número de bits não for múltiplo de 8, os bits finais
 * incompletos são preenchidos com zeros à direita.
 */
inline std::vector<uint8_t> bits_para_bytes(const std::vector<int>& bits) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < bits.size(); i += 8) {
        uint8_t byte = 0;
        for (int j = 0; j < 8 && (i + j) < bits.size(); j++) {
            byte = (byte << 1) | (bits[i + j] & 1);
        }
        bytes.push_back(byte);
    }
    return bytes;
}

/**
 * @brief Calcula o CRC-16 dos dados fornecidos usando o módulo crc.h.
 * 
 * Converte os bytes para bits, chama gera_crc() do módulo do colega,
 * e extrai os 16 bits finais (o CRC) como um uint16_t.
 * 
 * @param dados Vetor de bytes sobre os quais o CRC será calculado.
 * @return Valor CRC-16 de 2 bytes.
 */
inline uint16_t calcular_crc16(const std::vector<uint8_t>& dados) {
    // Converte bytes para o formato de bits que o crc.h espera
    std::vector<int> dados_bits = bytes_para_bits(dados);

    // gera_crc retorna dados_originais + 16 bits de CRC no final
    std::vector<int> resultado = gera_crc(dados_bits);

    // Extrai os últimos 16 bits (o CRC gerado)
    uint16_t crc = 0;
    size_t inicio_crc = resultado.size() - 16;
    for (size_t i = 0; i < 16; i++) {
        crc = (crc << 1) | (resultado[inicio_crc + i] & 1);
    }

    return crc;
}


/* =========================================
   MONTAGEM DO QUADRO (EMPACOTAMENTO)
   ========================================= */

/**
 * @brief Monta um quadro completo a partir de uma mensagem de texto.
 * 
 * Estrutura do quadro montado:
 * 
 *   [FLAG] [MAC_DEST 6B] [MAC_ORIG 6B] [TIPO 2B] [PAYLOAD com stuffing] [CRC 2B] [FLAG]
 * 
 * Etapas:
 *   1. Converte a mensagem (string) em vetor de bytes (payload cru)
 *   2. Monta o conteúdo interno: Header (MACs + Tipo) + Payload cru
 *   3. Calcula o CRC-16 sobre o conteúdo interno (antes do stuffing)
 *   4. Concatena conteúdo interno + CRC
 *   5. Aplica Byte Stuffing em todo o conteúdo (Header + Payload + CRC)
 *   6. Envolve com FLAGs de início e fim
 * 
 * @param mensagem  Texto a ser enviado como payload.
 * @param tipo      Tipo do quadro (TIPO_DATA ou TIPO_ACK).
 * @return Vetor de bytes contendo o quadro completo pronto para envio.
 */
inline std::vector<uint8_t> montar_quadro(const std::string& mensagem, uint16_t tipo, uint8_t seq_ack_no = 0) {
    std::vector<uint8_t> quadro;

    // --- 1. Converter a mensagem em bytes (payload cru) ---
    std::vector<uint8_t> payload(mensagem.begin(), mensagem.end());

    // --- 2. Montar o conteúdo interno (Header + Payload) ---
    std::vector<uint8_t> conteudo_interno;

    // MAC de Destino (6 bytes)
    conteudo_interno.insert(conteudo_interno.end(), MAC_DESTINO, MAC_DESTINO + TAM_MAC);

    // MAC de Origem (6 bytes)
    conteudo_interno.insert(conteudo_interno.end(), MAC_ORIGEM, MAC_ORIGEM + TAM_MAC);

    // Tipo (2 bytes, Big Endian)
    conteudo_interno.push_back(static_cast<uint8_t>((tipo >> 8) & 0xFF)); // Byte mais significativo
    conteudo_interno.push_back(static_cast<uint8_t>(tipo & 0xFF));        // Byte menos significativo

    // número de Sequência ou ACK (1 byte)
    conteudo_interno.push_back(seq_ack_no);

    // Payload cru
    conteudo_interno.insert(conteudo_interno.end(), payload.begin(), payload.end());

    // --- 3. Calcular CRC-16 sobre o conteúdo interno ---
    uint16_t crc = calcular_crc16(conteudo_interno);

    // --- 4. Adicionar CRC ao final do conteúdo interno ---
    conteudo_interno.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF)); // CRC byte alto
    conteudo_interno.push_back(static_cast<uint8_t>(crc & 0xFF));        // CRC byte baixo

    // --- 5. Aplicar Byte Stuffing em todo o conteúdo (Header + Payload + CRC) ---
    std::vector<uint8_t> conteudo_stuffed = byte_stuffing(conteudo_interno);

    // --- 6. Montar o quadro final com FLAGs ---
    quadro.push_back(FLAG_BYTE);                                                    // FLAG de início
    quadro.insert(quadro.end(), conteudo_stuffed.begin(), conteudo_stuffed.end());   // Conteúdo com stuffing
    quadro.push_back(FLAG_BYTE);                                                    // FLAG de fim

    return quadro;
}


/* =========================================
   DESMONTAGEM DO QUADRO (DESEMPACOTAMENTO)
   ========================================= */

/**
 * @brief Desmonta um quadro recebido e extrai a mensagem original.
 * 
 * Etapas:
 *   1. Verifica presença das FLAGs de início e fim
 *   2. Extrai o conteúdo entre as FLAGs
 *   3. Aplica Byte Destuffing para restaurar os dados originais
 *   4. Separa Header, Payload e CRC
 *   5. Verifica o CRC (integridade)
 *   6. Retorna a mensagem original como string
 * 
 * @param quadro_bruto Vetor de bytes recebido da rede.
 * @param tipo_recebido Ponteiro opcional para armazenar o tipo do quadro recebido.
 * @return String com a mensagem extraída, ou string vazia se o quadro for inválido.
 */
inline std::string desmontar_quadro(const std::vector<uint8_t>& quadro_bruto, uint16_t* tipo_recebido = nullptr, uint8_t* seq_ack_recebido = nullptr) {
    
    // --- 1. Verificar FLAGs de início e fim ---
    if (quadro_bruto.size() < 2) {
        std::cerr << "[Framing] Quadro muito pequeno para ser válido." << std::endl;
        return "";
    }

    if (quadro_bruto.front() != FLAG_BYTE || quadro_bruto.back() != FLAG_BYTE) {
        std::cerr << "[Framing] FLAGs de início/fim não encontradas." << std::endl;
        return "";
    }

    // --- 2. Extrair conteúdo entre as FLAGs ---
    std::vector<uint8_t> conteudo_stuffed(quadro_bruto.begin() + 1, quadro_bruto.end() - 1);

    if (conteudo_stuffed.empty()) {
        std::cerr << "[Framing] Quadro vazio (sem conteúdo entre FLAGs)." << std::endl;
        return "";
    }

    // --- 3. Aplicar Byte Destuffing ---
    std::vector<uint8_t> conteudo = byte_destuffing(conteudo_stuffed);

    // --- 4. Verificar tamanho mínimo: Header (14) + CRC (2) = 16 bytes ---
    if (conteudo.size() < static_cast<size_t>(TAM_HEADER + TAM_CRC)) {
        std::cerr << "[Framing] Conteúdo menor que o mínimo esperado (Header + CRC)." << std::endl;
        return "";
    }

    // --- 5. Separar os campos ---

    // Header: MACs e Tipo (primeiros 14 bytes)
    // uint8_t mac_dest[TAM_MAC], mac_orig[TAM_MAC]; // Disponíveis para uso futuro
    // std::memcpy(mac_dest, &conteudo[0], TAM_MAC);
    // std::memcpy(mac_orig, &conteudo[TAM_MAC], TAM_MAC);

    uint16_t tipo = (static_cast<uint16_t>(conteudo[TAM_MAC * 2]) << 8) 
                  |  static_cast<uint16_t>(conteudo[TAM_MAC * 2 + 1]);

    if (tipo_recebido != nullptr) {
        *tipo_recebido = tipo;
    }

    if (seq_ack_recebido != nullptr) {
        *seq_ack_recebido = conteudo[TAM_MAC * 2 + TAM_TIPO];
    }

    // Payload (entre o header e o CRC)
    std::vector<uint8_t> payload(conteudo.begin() + TAM_HEADER, conteudo.end() - TAM_CRC);

    // --- 6. Verificar CRC ---
    // Converte o conteúdo inteiro (Header + Payload + CRC) para bits
    // e usa verifica_crc() do módulo crc.h. Se o resto da divisão
    // pelo polinômio gerador for zero, o quadro está íntegro.
    std::vector<int> conteudo_bits = bytes_para_bits(conteudo);
    if (!verifica_crc(conteudo_bits)) {
        std::cerr << "[Framing] ERRO DE CRC: Quadro corrompido! Descartando." << std::endl;
        return "";
    }

    // --- 7. Retornar a mensagem extraída ---
    std::string mensagem(payload.begin(), payload.end());
    return mensagem;
}


/* =========================================
   FUNÇÃO DE DEPURAÇÃO
   ========================================= */

/**
 * @brief Imprime o conteúdo de um quadro em formato hexadecimal.
 * 
 * Útil para depuração: exibe cada byte do quadro em hexadecimal,
 * permitindo visualizar as FLAGs, os bytes de escape inseridos
 * pelo Byte Stuffing, e os campos do cabeçalho.
 * 
 * @param quadro Vetor de bytes do quadro a ser exibido.
 */
inline void imprimir_quadro_hex(const std::vector<uint8_t>& quadro) {
    std::cout << "[Framing] Quadro (" << quadro.size() << " bytes): ";
    for (uint8_t byte : quadro) {
        // Imprime cada byte como 2 dígitos hexadecimais maiúsculos
        std::cout << std::hex << std::uppercase;
        if (byte < 0x10) std::cout << "0";
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl; // Restaura formato decimal
}

#endif // FRAMING_HPP
