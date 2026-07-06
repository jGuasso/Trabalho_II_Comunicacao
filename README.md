# Trabalho II — Camada de Enlace de Dados

**Disciplina:** Comunicação de Dados — 5º Semestre  
**Tema:** Implementação de uma Camada de Enlace simulada sobre sockets UDP

---

## Visão Geral

Este projeto implementa uma simulação da **Camada de Enlace de Dados** (Camada 2 do Modelo OSI) sobre uma comunicação UDP entre dois programas em C++: um **Emissor** e um **Receptor**.

O objetivo é demonstrar, de forma prática, como funciona o processo de **enquadramento (framing) orientado a byte** — a técnica utilizada por protocolos reais (como HDLC e PPP) para delimitar, empacotar e verificar a integridade de dados transmitidos em uma rede.

A comunicação utiliza sockets UDP como camada de transporte subjacente, e a camada de enlace é implementada inteiramente em software, simulando:

- **Enquadramento** com delimitação por bytes de FLAG
- **Byte Stuffing / Destuffing** para transparência de dados
- **Cabeçalho** com endereços MAC simulados e campo de tipo
- **Verificação de erros** com CRC-16 (polinômio gerador)
- **Confirmação** de entrega via mensagens ACK enquadradas

---

## Arquitetura do Projeto

```
Trabalho_II_Comunicacao/
├── emissor.cpp        # Programa emissor (envia mensagens enquadradas)
├── receptor.cpp       # Programa receptor (recebe, verifica e confirma)
├── framing.hpp        # Módulo de enquadramento (byte stuffing, montagem/desmontagem)
├── crc.h              # Módulo de CRC-16 (cálculo e verificação por divisão polinomial)
├── net_compat.hpp     # Camada de compatibilidade de rede (Windows/Linux)
└── CMakeLists.txt     # Configuração de build (CMake)
```

### Diagrama de Componentes

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CAMADA DE APLICAÇÃO                         │
│                     (Mensagem do usuário)                          │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    CAMADA DE ENLACE SIMULADA                       │
│                        (framing.hpp + crc.h)                       │
│                                                                     │
│  ┌────────┬───────────┬───────────┬──────┬───────────┬─────┬──────┐│
│  │  FLAG  │ MAC_DEST  │ MAC_ORIG  │ TIPO │  PAYLOAD  │ CRC │ FLAG ││
│  │  0x7E  │  6 bytes  │  6 bytes  │ 2 B  │  N bytes  │ 2 B │ 0x7E ││
│  └────────┴───────────┴───────────┴──────┴───────────┴─────┴──────┘│
│                                                                     │
│  Funções: montar_quadro() / desmontar_quadro()                     │
│           byte_stuffing() / byte_destuffing()                      │
│           calcular_crc16() / verifica_crc()                        │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       SOCKET UDP (Camada de Transporte)             │
│                        (net_compat.hpp)                             │
│                    sendto() / recvfrom()                            │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Estrutura do Quadro (Frame)

O quadro segue a seguinte estrutura de bytes:

| Campo | Tamanho | Descrição |
|-------|---------|-----------|
| **FLAG** | 1 byte | Delimitador de início (`0x7E`) |
| **MAC Destino** | 6 bytes | Endereço físico de destino (simulado) |
| **MAC Origem** | 6 bytes | Endereço físico de origem (simulado) |
| **Tipo** | 2 bytes | Tipo do quadro: `0x0001` = DATA, `0x0002` = ACK |
| **Payload** | N bytes | Dados da mensagem (com Byte Stuffing aplicado) |
| **CRC-16** | 2 bytes | Código de verificação de erros |
| **FLAG** | 1 byte | Delimitador de fim (`0x7E`) |

**Tamanho mínimo do quadro:** 18 bytes (sem payload)  
**Overhead fixo:** Header (14 bytes) + CRC (2 bytes) + FLAGs (2 bytes) = 18 bytes

---

## Metodologias Utilizadas

### 1. Enquadramento Orientado a Byte (Byte-Oriented Framing)

O enquadramento orientado a byte utiliza **bytes especiais** para delimitar o início e o fim de cada quadro:

- **FLAG (`0x7E`)**: Marca o início e o fim do quadro
- **ESC (`0x7D`)**: Byte de escape, usado no processo de Byte Stuffing

Este é o mesmo mecanismo utilizado nos protocolos **HDLC** e **PPP** do mundo real.

### 2. Byte Stuffing (Inserção de Bytes de Escape)

O Byte Stuffing resolve o problema de **transparência de dados**: o que acontece quando os dados do payload contêm o mesmo valor do byte FLAG (`0x7E`)? Sem tratamento, o receptor interpretaria esse byte como o fim do quadro prematuramente.

**Regras de substituição no emissor:**
| Byte original no payload | Substituição |
|--------------------------|-------------|
| `0x7E` (FLAG) | `0x7D 0x5E` (ESC + FLAG XOR 0x20) |
| `0x7D` (ESC)  | `0x7D 0x5D` (ESC + ESC XOR 0x20) |

**Reversão no receptor (Destuffing):**  
Ao encontrar `0x7D`, o receptor lê o próximo byte e aplica XOR com `0x20` para recuperar o valor original.

### 3. CRC-16 — Verificação de Erros por Redundância Cíclica

A integridade dos dados é verificada utilizando **CRC-16** com o polinômio gerador:

```
x¹⁶ + x¹² + x⁵ + 1  →  representado como [1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,1]
```

**No emissor:**  
1. O conteúdo interno (Header + Payload) é convertido em um vetor de bits
2. A função `gera_crc()` realiza a divisão polinomial e gera 16 bits de redundância
3. Os 16 bits são anexados ao final do quadro como 2 bytes de CRC

**No receptor:**  
1. O conteúdo recebido (Header + Payload + CRC) é convertido em bits
2. A função `verifica_crc()` refaz a divisão polinomial
3. Se o resto for zero, o quadro está íntegro; caso contrário, é descartado

### 4. Confirmação por ACK (Acknowledge)

Após receber e validar um quadro de dados (`TIPO_DATA`), o receptor responde com um **quadro ACK** (`TIPO_ACK`) também completamente enquadrado, passando pelo mesmo processo de framing (Header + CRC + Byte Stuffing + FLAGs).

Se o quadro recebido estiver corrompido (CRC inválido), o receptor **não envia ACK**, simulando o comportamento de protocolos reais onde a ausência de confirmação pode disparar retransmissão.

### 5. Comunicação via Sockets UDP

A comunicação entre emissor e receptor utiliza **sockets UDP** (`SOCK_DGRAM`):

- **Protocolo:** UDP (User Datagram Protocol) — sem conexão, sem garantia de entrega nativa
- **Porta:** 8080 (receptor escuta nesta porta fixa)
- **Endereçamento:** IPv4 (`AF_INET`)
- **Compatibilidade:** O módulo `net_compat.hpp` abstrai as diferenças entre Winsock2 (Windows) e sockets POSIX (Linux/macOS)

A escolha do UDP é intencional: por não oferecer garantias de entrega na camada de transporte, ele permite que a camada de enlace simulada demonstre seu papel de forma mais visível.

### 6. Multithreading (Emissor)

O emissor utiliza uma **thread secundária** dedicada exclusivamente à recepção de ACKs (`thread_recepcao`), permitindo que o usuário continue digitando e enviando mensagens sem bloquear enquanto aguarda confirmações. A sincronização é feita via `std::atomic<bool>`.

---

## Como Compilar e Executar

### Pré-requisitos

- **CMake** 3.15 ou superior
- **Compilador C++17** (MSVC, GCC ou Clang)
- No Windows: Visual Studio com suporte a C++ instalado

### Compilação

```bash
# Na pasta do projeto
mkdir build
cd build
cmake ..
cmake --build .
```

### Execução

Abrir **dois terminais** na pasta do projeto:

**Terminal 1 — Receptor** (inicia primeiro):
```bash
.\build\Debug\receptor.exe
```

**Terminal 2 — Emissor** (conecta ao receptor):
```bash
# Para testes locais (mesma máquina):
.\build\Debug\emissor.exe 127.0.0.1

# Para testes em rede (outra máquina):
.\build\Debug\emissor.exe <IP_DO_RECEPTOR>
```

### Uso

1. O receptor exibirá: `Receptor pronto e aguardando pacotes na porta 8080...`
2. No emissor, digite uma mensagem e pressione Enter
3. O emissor exibirá o quadro montado em hexadecimal e confirmará o envio
4. O receptor exibirá a mensagem recebida, o quadro em hex, e enviará um ACK
5. O emissor receberá o ACK e exibirá `[ACK] Mensagem confirmada pelo receptor!`
6. Digite `sair` no emissor para encerrar

---

## Exemplo de Execução

**Emissor:**
```
Digite as mensagens para enviar (ou 'sair' para encerrar):
> Hello World
[Framing] Quadro (30 bytes): 7E BB BB BB BB BB BB AA AA AA AA AA AA 00 01 48 65 6C 6C 6F 20 57 6F 72 6C 64 XX XX 7E
Quadro enviado com sucesso! (30 bytes)

[ACK] Mensagem confirmada pelo receptor!
```

**Receptor:**
```
Receptor pronto e aguardando pacotes na porta 8080...
>Hello World
[Framing] Quadro (30 bytes): 7E BB BB BB BB BB BB AA AA AA AA AA AA 00 01 48 65 6C 6C 6F 20 57 6F 72 6C 64 XX XX 7E
[Receptor] ACK enviado (quadro de 22 bytes).
```

*(XX XX representam os bytes de CRC calculados dinamicamente)*

---

## Fluxo Completo da Comunicação

```
 EMISSOR                                           RECEPTOR
    │                                                  │
    │  1. Usuário digita "Hello"                       │
    │  2. montar_quadro("Hello", TIPO_DATA)            │
    │     ├─ Monta Header (MACs + Tipo)                │
    │     ├─ Calcula CRC-16                            │
    │     ├─ Aplica Byte Stuffing                      │
    │     └─ Adiciona FLAGs                            │
    │  3. sendto(quadro) ─────────────────────────────►│
    │                                                  │  4. recvfrom(buffer)
    │                                                  │  5. desmontar_quadro(buffer)
    │                                                  │     ├─ Verifica FLAGs
    │                                                  │     ├─ Byte Destuffing
    │                                                  │     ├─ Verifica CRC-16 ✓
    │                                                  │     └─ Extrai "Hello"
    │                                                  │  6. Exibe: >Hello
    │                                                  │  7. montar_quadro("ACK", TIPO_ACK)
    │◄─────────────────────────────────────────────────│  8. sendto(quadro_ack)
    │  9. recvfrom(buffer)                             │
    │ 10. desmontar_quadro(buffer) → "ACK"             │
    │ 11. Exibe: [ACK] Mensagem confirmada!            │
    │                                                  │
```

---

## Descrição dos Arquivos

| Arquivo | Descrição |
|---------|-----------|
| `emissor.cpp` | Programa principal do emissor. Lê mensagens do teclado, empacota em quadros e envia via UDP. Recebe ACKs em thread separada. |
| `receptor.cpp` | Programa principal do receptor. Aguarda quadros na porta 8080, desempacota, exibe a mensagem e responde com ACK enquadrado. |
| `framing.hpp` | Módulo de enquadramento. Contém todas as funções de montagem/desmontagem de quadros, byte stuffing/destuffing e integração com CRC. |
| `crc.h` | Módulo de CRC-16. Implementa cálculo e verificação de CRC por divisão polinomial sobre vetores de bits. |
| `net_compat.hpp` | Camada de compatibilidade de rede. Abstrai diferenças entre Winsock2 (Windows) e sockets POSIX (Linux/macOS) com macros e typedefs unificados. |
| `CMakeLists.txt` | Configuração do sistema de build CMake. Define os targets `emissor` e `receptor` com C++17. |

---

## Tecnologias e Ferramentas

- **Linguagem:** C++17
- **Build System:** CMake 3.15+
- **Rede:** Sockets UDP (Winsock2 / POSIX)
- **Concorrência:** `std::thread` + `std::atomic`
- **Plataformas:** Windows (testado) / Linux / macOS (compatível via `net_compat.hpp`)
