// Teste da árvore de comportamento (sem tela): o NPC precisa atravessar uma
// porta que pode estar trancada, fechada ou aberta.
//
// Compilar (da raiz do projeto):
//   g++ -std=c++17 -DRODAR_TESTE -Isource source/teste.cpp source/BehaviorTree.cpp -o teste
//
// Este arquivo só é compilado com -DRODAR_TESTE. Sem isso ele fica vazio, para
// que compilar a pasta source inteira gere um único programa: o jogo (jogo.cpp).

#ifdef RODAR_TESTE

#include <iostream>

#include "BehaviorTree.hpp"

int main() {
    BehaviorTree Arvore(
        Selector(
            Sequence(Condition("Passou"), Action("FechePorta")),
            Sequence(Condition("NaPorta"), Condition("PortaAberta"), Action("Passe")),
            Sequence(Condition("NaPorta"), Condition("PortaDestrancada"), Condition("PortaFechada"), Action("AbrirPorta")),
            Sequence(Condition("NaPorta"), Condition("PortaTrancada"), Action("DestranquePorta")),
            Action("CaminhePorta")
        )
    );

    // Contexto do enunciado: NPC na porta, porta trancada e fechada.
    BTContext context = {
        {"NaPorta", 1},
        {"PortaTrancada", 1},
        {"PortaFechada", 1},
    };
    std::cout << Arvore.tick(context) << '\n';   // DestranquePorta

    // A mesma árvore em cada etapa da travessia, do começo ao fim.
    std::cout << "\nTravessia completa:\n";
    const BTContext etapas[] = {
        {{"PortaTrancada", 1}, {"PortaFechada", 1}},                     // longe da porta
        {{"NaPorta", 1}, {"PortaTrancada", 1}, {"PortaFechada", 1}},     // chegou, trancada
        {{"NaPorta", 1}, {"PortaDestrancada", 1}, {"PortaFechada", 1}},  // destrancou
        {{"NaPorta", 1}, {"PortaDestrancada", 1}, {"PortaAberta", 1}},   // abriu
        {{"Passou", 1}, {"PortaAberta", 1}},                             // atravessou
    };
    for (const BTContext& etapa : etapas) {
        std::cout << "  ";
        for (const auto& [nome, valor] : etapa) {
            std::cout << nome << '=' << valor << ' ';
        }
        std::cout << "-> " << Arvore.tick(etapa) << '\n';
    }
    return 0;
}

#endif // RODAR_TESTE
