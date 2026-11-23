// Teste de um NPC simples na tela: patrulha vai-e-volta entre os pontos A e B,
// decidida pela árvore de comportamento a cada tick.
//
// Usa da biblioteca: NPC (Entity + sprite), RigidBody2D (movimento),
// Collision2D (tamanho) e BehaviorTree (decisão).
//
// Compilar (da raiz do projeto, com o SFML 2.x instalado):
//   g++ -std=c++17 -DRODAR_TESTE -Isource source/teste2.cpp source/BehaviorTree.cpp
//       source/RigidBody2D.cpp source/Collision2D.cpp source/Vector2D.cpp
//       -lsfml-graphics -lsfml-window -lsfml-system -o teste2
//   (tudo numa linha só)
//
// Este arquivo só é compilado com -DRODAR_TESTE. Sem isso ele fica vazio, para
// que compilar a pasta source inteira gere um único programa: o jogo (jogo.cpp).

#ifdef RODAR_TESTE

#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "BehaviorTree.hpp"
#include "Global.hpp"
#include "NPC.hpp"

// O ponto A fica à direita (x cresce) e o B à esquerda, como no enunciado:
// AndeParaA faz x aumentar e AndeParaB faz x diminuir.
struct NPCPatrulha : NPC {
    float pontoA{0.0f};
    float pontoB{0.0f};
    float velocidade{0.0f};    ///< Pixels por segundo.
    bool  direcaoA{true};      ///< Guardada entre ticks (no enunciado era local e se perdia).
    std::string comando;       ///< Último comando, só para mostrar.
};

namespace {

constexpr float DT = 1.0f / 60.0f;   // passo fixo, como pede o RigidBody2D

const BehaviorTree Arvore(
    Selector(
        Sequence(Condition("ChegouPontoA"), Action("InverterDirecao")),
        Sequence(Condition("ChegouPontoB"), Action("InverterDirecao")),
        Sequence(Condition("IndoParaA"), Action("AndeParaA")),
        Sequence(Condition("IndoParaB"), Action("AndeParaB"))
    )
);

//-------------------------------------

void Execucao_Meu_NPC(NPCPatrulha& npc) {
    const float x = npc.body.position.x;

    // O estado do jogo é montado a cada tick. "Chegou" só vale para o ponto
    // para onde o NPC está indo; senão, depois de inverter, ele continuaria
    // "em A" e inverteria de novo para sempre.
    BTContext Estado_do_Jogo;
    if (npc.direcaoA) {
        Estado_do_Jogo = {{"IndoParaA", 1}, {"ChegouPontoA", x >= npc.pontoA ? 1 : 0}};
    } else {
        Estado_do_Jogo = {{"IndoParaB", 1}, {"ChegouPontoB", x <= npc.pontoB ? 1 : 0}};
    }

    npc.comando = Arvore.tick(Estado_do_Jogo);

    if ("InverterDirecao" == npc.comando) npc.direcaoA = !npc.direcaoA;
    if ("AndeParaA" == npc.comando)       npc.body.velocity = {  npc.velocidade, 0.0f };
    if ("AndeParaB" == npc.comando)       npc.body.velocity = { -npc.velocidade, 0.0f };
    if ("InverterDirecao" == npc.comando) npc.body.velocity = { 0.0f, 0.0f };

    npc.body.integrate(DT);
}

//-------------------------------------

void Mostrar_Todos_NPCs_na_Tela(sf::RenderWindow& janela, std::vector<NPCPatrulha>& npcs) {
    janela.clear(sf::Color(30, 30, 40));

    for (NPCPatrulha& npc : npcs) {
        // Trilho da patrulha, com A (verde) e B (vermelho) nas pontas.
        const float y = npc.body.position.y;
        sf::RectangleShape trilho({ npc.pontoA - npc.pontoB, 2.0f });
        trilho.setPosition(npc.pontoB, y - 1.0f);
        trilho.setFillColor(sf::Color(90, 90, 110));
        janela.draw(trilho);

        sf::CircleShape marca(6.0f);
        marca.setOrigin(6.0f, 6.0f);
        marca.setFillColor(sf::Color(80, 200, 120));
        marca.setPosition(npc.pontoA, y);
        janela.draw(marca);
        marca.setFillColor(sf::Color(220, 80, 80));
        marca.setPosition(npc.pontoB, y);
        janela.draw(marca);

        // A posição do corpo é o CENTRO; o sprite é desenhado pelo canto.
        const Vector2D canto = npc.body.position - npc.collision.halfExtents;
        npc.sprite.setPosition(canto.x, canto.y);
        janela.draw(npc.sprite);
    }

    janela.display();
}

/// Gera uma textura 64x64 (TILE_SIZE) por código, para não depender de imagem.
sf::Texture criarTexturaNPC() {
    const unsigned lado = static_cast<unsigned>(TILE_SIZE);
    sf::Image img;
    img.create(lado, lado, sf::Color::Transparent);
    for (unsigned py = 4; py < lado - 4; ++py) {
        for (unsigned px = 4; px < lado - 4; ++px) {
            img.setPixel(px, py, sf::Color(240, 200, 80));
        }
    }
    for (unsigned py = 18; py < 28; ++py) {   // olhos
        for (unsigned px = 18; px < 26; ++px) {
            img.setPixel(px, py, sf::Color::Black);
            img.setPixel(px + 20, py, sf::Color::Black);
        }
    }
    sf::Texture tex;
    tex.loadFromImage(img);
    return tex;
}

} // namespace

//------------------------------------------------

int main() {
    sf::RenderWindow janela(sf::VideoMode(800, 600), "teste2 - NPC patrulha");
    janela.setFramerateLimit(60);

    const sf::Texture textura = criarTexturaNPC();

    // Três NPCs com trilhos e velocidades diferentes.
    std::vector<NPCPatrulha> npcs(3);
    const float dados[3][4] = {
        // y     B      A      velocidade
        { 120.0f, 100.0f, 700.0f, 200.0f },
        { 300.0f, 200.0f, 600.0f, 120.0f },
        { 480.0f, 100.0f, 400.0f, 300.0f },
    };
    for (std::size_t i = 0; i < npcs.size(); ++i) {
        NPCPatrulha& npc = npcs[i];
        npc.id = INITIAL_ENTITY_ID + static_cast<EntityId>(i);
        npc.pontoB = dados[i][1];
        npc.pontoA = dados[i][2];
        npc.velocidade = dados[i][3];
        npc.body.position = { npc.pontoB, dados[i][0] };
        npc.sprite.setTexture(textura);
    }

    // GameLoop: passo fixo, independente do FPS real.
    sf::Clock relogio;
    float acumulado = 0.0f;
    while (janela.isOpen()) {
        sf::Event evento;
        while (janela.pollEvent(evento)) {
            if (evento.type == sf::Event::Closed ||
                (evento.type == sf::Event::KeyPressed && evento.key.code == sf::Keyboard::Escape)) {
                janela.close();
            }
        }

        acumulado += relogio.restart().asSeconds();
        if (acumulado > 0.25f) acumulado = 0.25f;   // evita "espiral" após travadas
        while (acumulado >= DT) {
            for (NPCPatrulha& npc : npcs) {
                Execucao_Meu_NPC(npc);
            }
            acumulado -= DT;
        }

        janela.setTitle("teste2 - NPC 1: " + npcs[0].comando);
        Mostrar_Todos_NPCs_na_Tela(janela, npcs);
    }
    return 0;
}

#endif // RODAR_TESTE
