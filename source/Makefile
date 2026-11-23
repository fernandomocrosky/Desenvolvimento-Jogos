# Compila o jogo CRIPTA a partir de todos os .cpp desta pasta.
#
#   make          -> gera ./cripta
#   ./cripta      -> roda o jogo
#   make clean    -> apaga o executável
#
# Requer g++ (C++17) e SFML 2.x (Linux: sudo apt install libsfml-dev).
# Equivale a:  g++ -std=c++17 -O2 *.cpp -lsfml-graphics -lsfml-window -lsfml-system -o cripta

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LIBS      = -lsfml-graphics -lsfml-window -lsfml-system

cripta: $(wildcard *.cpp) $(wildcard *.hpp)
	$(CXX) $(CXXFLAGS) $(wildcard *.cpp) $(LIBS) -o $@

clean:
	rm -f cripta

.PHONY: clean
