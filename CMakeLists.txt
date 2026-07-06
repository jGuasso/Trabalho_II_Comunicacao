cmake_minimum_required(VERSION 3.15)
project(Trabalho_II_Comunicacao)

# Exige C++17 (necessário para inline variables no crc.h)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cria o executável do Emissor
add_executable(emissor emissor.cpp net_compat.hpp)

# Cria o executável do Receptor
add_executable(receptor receptor.cpp net_compat.hpp)

# Se for Windows, vincula a biblioteca de sockets da Microsoft em ambos
if (WIN32)
    target_link_libraries(emissor ws2_32)
    target_link_libraries(receptor ws2_32)
endif()
