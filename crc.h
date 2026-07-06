#ifndef CRC_H
#define CRC_H

#include <string>
#include <vector>
using namespace std;

inline const vector<int> polinomio_gerador = {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; //crc-16 - x^16 + x^12 + x^5 + 1

inline vector<int> resto(vector<int> dados_com_preenchimento, const vector<int>& polinomio_divisor) {
    int tamanho_polinomio = polinomio_divisor.size();
    int total_de_bits = dados_com_preenchimento.size();

    vector<int> trecho_atual_da_divisao(dados_com_preenchimento.begin(), dados_com_preenchimento.begin() + tamanho_polinomio);

    for (int indice_proximo_bit = tamanho_polinomio; indice_proximo_bit <= total_de_bits; indice_proximo_bit++) {
        if (trecho_atual_da_divisao[0] == 1) {
            for (int indice_bit = 0; indice_bit < tamanho_polinomio; indice_bit++) {
                trecho_atual_da_divisao[indice_bit] ^= polinomio_divisor[indice_bit];
            }
        }
        trecho_atual_da_divisao.erase(trecho_atual_da_divisao.begin());
        if (indice_proximo_bit < total_de_bits) {
            trecho_atual_da_divisao.push_back(dados_com_preenchimento[indice_proximo_bit]);
        }
    }
    return trecho_atual_da_divisao;
}

inline vector<int> gera_crc(const vector<int>& dados_originais, const vector<int>& polinomio_divisor = polinomio_gerador) {
    int tamanho_polinomio = polinomio_divisor.size();
    vector<int> dados_com_preenchimento = dados_originais;
    dados_com_preenchimento.insert(dados_com_preenchimento.end(), tamanho_polinomio - 1, 0);

    vector<int> resto_da_divisao = resto(dados_com_preenchimento, polinomio_divisor);
    vector<int> quadro_com_crc = dados_originais;
    quadro_com_crc.insert(quadro_com_crc.end(), resto_da_divisao.begin(), resto_da_divisao.end());
    return quadro_com_crc;
}

inline bool verifica_crc(const vector<int>& quadro_recebido, const vector<int>& polinomio_divisor = polinomio_gerador) {
    vector<int> resto_da_divisao = resto(quadro_recebido, polinomio_divisor);
    for (int bit_do_resto : resto_da_divisao) {
        if (bit_do_resto != 0){
            return false;
        }
    }
    return true;
}




// conversores auxiliares
inline vector<int> string_para_bits(const string& texto_binario) {
    vector<int> vetor_de_bits;
    for (char caractere : texto_binario) {
        if (caractere == '0' || caractere == '1') {
            vetor_de_bits.push_back(caractere - '0');
        }
    }
    return vetor_de_bits;
}

inline string bits_para_string(const vector<int>& vetor_de_bits) {
    string texto_binario;
    for (int bit : vetor_de_bits) {
        texto_binario += (bit ? '1' : '0');
    }
    return texto_binario;
}

#endif
