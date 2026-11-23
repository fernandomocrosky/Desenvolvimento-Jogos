# Compila o jogo CRIPTA a partir de todos os .cpp desta pasta (SFML 2.x).
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/cripta            (Windows: build\Debug\cripta.exe)
#
# No Windows, aponte para o SFML se ele não for achado sozinho:
#   cmake -S . -B build -DSFML_DIR=C:/SFML-2.6.1/lib/cmake/SFML

cmake_minimum_required(VERSION 3.10)
project(Cripta CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

file(GLOB FONTES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(cripta ${FONTES})
target_link_libraries(cripta sfml-graphics sfml-window sfml-system)
