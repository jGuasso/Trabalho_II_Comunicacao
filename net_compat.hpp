#ifndef NET_COMPAT_HPP
#define NET_COMPAT_HPP

#ifdef _WIN32
    /* =========================================
       CONFIGURAÇÃO PARA WINDOWS (WINSOCK2)
       ========================================= */
    #include <winsock2.h>
    #include <ws2tcpip.h>
    
    #pragma comment(lib, "ws2_32.lib")

    // Macros para unificar as ações de rede
    #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSESOCKET(s) closesocket(s)
    #define GETSOCKETERRNO() (WSAGetLastError())

    // Trazendo o tipo socklen_t do POSIX para o Windows
    typedef int socklen_t;

#else
    /* =========================================
       CONFIGURAÇÃO PARA LINUX/MACOS (POSIX)
       ========================================= */
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <errno.h>

    // Trazendo os tipos e constantes do Windows para o mundo Unix
    typedef int SOCKET;
    const int INVALID_SOCKET = -1;
    const int SOCKET_ERROR = -1;

    // Macros para unificar as ações de rede
    #define ISVALIDSOCKET(s) ((s) >= 0)
    #define CLOSESOCKET(s) close(s)
    #define GETSOCKETERRNO() (errno)
#endif

/* =========================================
   FUNÇÕES DE INICIALIZAÇÃO UNIVERSAIS
   ========================================= */

// Prepara a rede (Executa o WSAStartup no Windows, e não faz nada no Linux)
inline bool IniciarRede() {
#ifdef _WIN32
    WSADATA wsaData;
    return (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#else
    return true; 
#endif
}

// Limpa a memória da rede ao finalizar o programa
inline void EncerrarRede() {
#ifdef _WIN32
    WSACleanup();
#endif
}

#endif // NET_COMPAT_HPP