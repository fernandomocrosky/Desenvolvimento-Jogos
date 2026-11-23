// CRIPTA - jogo de exploração de masmorra visto de cima (SFML 2.x).
//
// O jogador explora uma masmorra gerada proceduralmente, recolhe gemas, foge
// (ou atira) nos Espectros e procura a escada para descer ao próximo nível.
//
//   Setas  : mover           Espaço : atirar
//   R      : recomeçar (após o fim de jogo)   ESC : sair
//
// Usa da biblioteca:
//   Dungeon2D + TileMap  -> cenário (masmorra BSP rasterizada na matriz)
//   CollisionManager     -> lista de todas as coisas e pares que colidiram
//   RigidBody2D          -> física (aceleração, atrito, vento) com dt fixo
//   BehaviorTree         -> decisão do inimigo (Espectro)
//   PathFind             -> perseguição do inimigo pelo A*
//   NPC / Entity         -> base de todas as coisas do jogo
//
// Compilar (da raiz do projeto, numa linha só):
//   g++ -std=c++17 -O2 -Isource source/jogo.cpp source/CollisionManager.cpp
//       source/Dungeon2D.cpp source/TileMap.cpp source/PathFind.cpp
//       source/BehaviorTree.cpp source/RigidBody2D.cpp source/Collision2D.cpp
//       source/Vector2D.cpp -lsfml-graphics -lsfml-window -lsfml-system -o cripta

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <SFML/Graphics.hpp>

#include "BehaviorTree.hpp"
#include "CollisionManager.hpp"
#include "Dungeon2D.hpp"
#include "Global.hpp"
#include "NPC.hpp"
#include "PathFind.hpp"
#include "TileMap.hpp"

namespace {

// --- Configuração ------------------------------------------------------------

constexpr float DT = 1.0f / 60.0f;          // passo fixo da física
constexpr unsigned LARGURA_JANELA = 1280;
constexpr unsigned ALTURA_JANELA  = 720;
constexpr int MAPA_LARGURA = 40;            // em tiles
constexpr int MAPA_ALTURA  = 30;

// Jogador
constexpr float JOGADOR_MEIA_LARGURA = 22.0f;
constexpr float JOGADOR_ACELERACAO   = 2000.0f;   // px/s² quando a seta está apertada
constexpr float ATRITO               = 7.0f;      // 1/s: velocidade máxima ~ 2000/7 ~ 285 px/s
constexpr int   VIDAS_INICIAIS       = 3;
constexpr int   VIDAS_MAXIMAS        = 5;
constexpr float TEMPO_INVULNERAVEL   = 1.5f;      // s após levar dano
constexpr float EMPURRAO_DANO        = 450.0f;    // px/s
constexpr float INTERVALO_TIRO       = 0.25f;
constexpr float VELOCIDADE_TIRO      = 650.0f;
constexpr float DURACAO_TIRO         = 1.5f;
constexpr float DASH_VELOCIDADE      = 950.0f;    // px/s durante a arrancada
constexpr float DASH_DURACAO         = 0.16f;     // s (invulnerável nesse tempo)
constexpr float DASH_RECARGA         = 0.8f;      // s até poder dar outro dash

// Inimigo (Espectro)
constexpr float ESPECTRO_MEIA_LARGURA = 22.0f;
constexpr float ESPECTRO_VEL_VAGAR    = 110.0f;
constexpr float ESPECTRO_VEL_CACAR    = 165.0f;
constexpr int   ESPECTRO_VIDA         = 2;
constexpr float ESPECTRO_RECARGA      = 1.8f;     // s entre bolas de fogo
constexpr float ESPECTRO_VISAO        = 9.0f * TILE_SIZE;
constexpr float ESPECTRO_ALCANCE_TIRO = 6.0f * TILE_SIZE;
constexpr float ESPECTRO_RECALCULO    = 0.4f;     // s entre recálculos do A*
constexpr float VELOCIDADE_BOLA       = 320.0f;
constexpr float DURACAO_BOLA          = 3.0f;
constexpr float ESPECTRO_EMPURRAO     = 380.0f;   // px/s de recuo ao levar tiro
constexpr float ESPECTRO_ATORDOADO    = 0.22f;    // s parado depois do tiro
constexpr float ESPECTRO_PISCAR       = 0.15f;    // s piscando vermelho

// Física do cenário
constexpr float FORCA_VENTO = 900.0f;             // px/s² dentro de uma sala de vento

// Pontuação
constexpr int PONTOS_GEMA     = 50;
constexpr int PONTOS_ESPECTRO = 100;
constexpr int PONTOS_NIVEL    = 500;

// --- Coisas do jogo ----------------------------------------------------------

enum class Tipo { Jogador, Espectro, TiroJogador, BolaDeFogo, Gema, Coracao, Espinho, Escada };

/// Toda coisa do jogo: um NPC (corpo + colisão + sprite + id) com um tipo.
struct Coisa : NPC {
    Tipo tipo{Tipo::Gema};
};

struct Jogador : Coisa {
    int      vidas{VIDAS_INICIAIS};
    float    invulneravel{0.0f};   ///< Segundos restantes sem levar dano.
    float    recarga{0.0f};        ///< Segundos até poder atirar de novo.
    Vector2D mira{1.0f, 0.0f};     ///< Última direção andada (para onde atira).
    float    dash{0.0f};           ///< Segundos restantes da arrancada atual.
    float    recargaDash{0.0f};    ///< Segundos até poder dar outro dash.
    bool     teclaDashAntes{false};///< Para o dash disparar ao APERTAR, não ao segurar.
};

/// Inimigo: fantasma que vaga pela masmorra, persegue o jogador quando o vê
/// e atira bolas de fogo quando está perto. Decide pela árvore de comportamento.
struct Espectro : Coisa {
    int                   vida{ESPECTRO_VIDA};
    float                 recarga{0.0f};
    float                 recalculo{0.0f};
    std::vector<Vector2D> caminho;      ///< Pontos do A* a seguir.
    std::size_t           passo{0};     ///< Próximo ponto do caminho.
    float                 fase{0.0f};   ///< Só para a animação de flutuar.
    float                 piscar{0.0f};      ///< Segundos piscando vermelho (levou tiro).
    float                 atordoado{0.0f};   ///< Segundos recuando, sem decidir nada.
    Vector2D              empurrao;          ///< Velocidade do recuo.
};

struct Projetil : Coisa {
    float duracao{0.0f};   ///< Segundos até sumir sozinho.
};

// --- Efeitos visuais (não são entidades: não colidem) -------------------------

struct Particula {
    Vector2D  pos, vel;
    float     vida{0.0f}, vidaMax{1.0f};
    float     tamanho{4.0f};
    sf::Color cor;
};

struct TextoFlutuante {
    Vector2D    pos;
    std::string texto;
    sf::Color   cor;
    float       vida{0.0f};
};

/// Cópia apagada do jogador deixada pelo dash.
struct Rastro {
    Vector2D pos;
    float    rotacao{0.0f};
    float    vida{0.0f};
};

/// Região retangular (uma sala) onde o vento empurra jogador e projéteis.
struct ZonaVento {
    sf::FloatRect area;
    Vector2D      forca;
};

struct Texturas {
    sf::Texture jogador, espectro, tiro, bola, gema, coracao, espinho, escada;
};

// --- Árvore de comportamento do Espectro --------------------------------------

const BehaviorTree CerebroEspectro(
    Selector(
        Sequence(Condition("JogadorVisivel"), Condition("JogadorPerto"),
                 Condition("ArmaPronta"), Action("Atirar")),
        Sequence(Condition("JogadorVisivel"), Action("Perseguir")),
        Sequence(Condition("TemDestino"), Action("Vagar")),
        Action("EscolherDestino")
    )
);

// --- Estado do jogo -----------------------------------------------------------

struct Jogo {
    const Texturas& tex;
    std::mt19937    rng{std::random_device{}()};

    TileMap                         mapa;
    std::unique_ptr<PathFind>       caminhos;
    CollisionManager                colisoes;
    std::vector<std::unique_ptr<Coisa>> coisas;    ///< Tudo menos o jogador.
    std::unordered_map<EntityId, Coisa*> porId;    ///< id -> coisa (inclui o jogador).
    std::vector<ZonaVento>          ventos;
    sf::VertexArray                 piso{sf::Quads};
    std::vector<Particula>          particulas;
    std::vector<TextoFlutuante>     textos;
    std::vector<Rastro>             rastros;
    float                           tremor{0.0f};   ///< Segundos de tela tremendo.

    Jogador jogador;
    int     pontos{0};
    int     nivel{1};
    int     gemasTotal{0};
    bool    fimDeJogo{false};
    bool    descer{false};
    float   relogio{0.0f};   ///< Tempo de jogo, para animações.

    explicit Jogo(const Texturas& t) : tex(t) {}
};

sf::String utf8(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}

float sortear(std::mt19937& rng, float minimo, float maximo) {
    return minimo + (maximo - minimo) * (static_cast<float>(rng()) / static_cast<float>(std::mt19937::max()));
}

/// Espalha @p n partículas a partir de @p pos, em todas as direções.
void explodir(Jogo& jogo, const Vector2D& pos, sf::Color cor, int n, float velMax, float vida = 0.45f) {
    for (int i = 0; i < n; ++i) {
        const float ang = sortear(jogo.rng, 0.0f, 6.2831853f);
        const float vel = sortear(jogo.rng, velMax * 0.3f, velMax);
        Particula p;
        p.pos = pos;
        p.vel = { std::cos(ang) * vel, std::sin(ang) * vel };
        p.vidaMax = p.vida = sortear(jogo.rng, vida * 0.6f, vida);
        p.tamanho = sortear(jogo.rng, 3.0f, 7.0f);
        p.cor = cor;
        jogo.particulas.push_back(p);
    }
}

void textoFlutuante(Jogo& jogo, const Vector2D& pos, const std::string& texto, sf::Color cor) {
    jogo.textos.push_back({ pos, texto, cor, 0.9f });
}

// --- Texturas desenhadas por código (não depende de arquivos de imagem) -------

sf::Texture desenhar(const std::function<void(sf::RenderTarget&)>& pintar) {
    const unsigned lado = static_cast<unsigned>(TILE_SIZE);
    sf::RenderTexture rt;
    rt.create(lado, lado);
    rt.clear(sf::Color::Transparent);
    pintar(rt);
    rt.display();
    sf::Texture t(rt.getTexture());
    t.setSmooth(true);
    return t;
}

sf::CircleShape circulo(float r, float cx, float cy, sf::Color cor) {
    sf::CircleShape c(r);
    c.setOrigin(r, r);
    c.setPosition(cx, cy);
    c.setFillColor(cor);
    return c;
}

Texturas criarTexturas() {
    Texturas t;
    t.jogador = desenhar([](sf::RenderTarget& rt) {
        sf::CircleShape corpo = circulo(27, 32, 32, sf::Color(70, 140, 230));
        corpo.setOutlineThickness(3);
        corpo.setOutlineColor(sf::Color(20, 40, 90));
        rt.draw(corpo);
        rt.draw(circulo(9, 40, 26, sf::Color(230, 245, 255)));   // visor
        rt.draw(circulo(4, 42, 26, sf::Color(20, 40, 90)));
    });
    t.espectro = desenhar([](sf::RenderTarget& rt) {
        const sf::Color cor(170, 120, 230, 235);
        rt.draw(circulo(24, 32, 28, cor));
        sf::RectangleShape saia({48, 22});
        saia.setPosition(8, 28);
        saia.setFillColor(cor);
        rt.draw(saia);
        for (int i = 0; i < 3; ++i) rt.draw(circulo(8, 16.0f + 16.0f * i, 50, cor));   // barra ondulada
        rt.draw(circulo(6, 23, 26, sf::Color(30, 10, 40)));
        rt.draw(circulo(6, 41, 26, sf::Color(30, 10, 40)));
        rt.draw(circulo(2.5f, 23, 26, sf::Color(255, 70, 70)));
        rt.draw(circulo(2.5f, 41, 26, sf::Color(255, 70, 70)));
    });
    t.tiro = desenhar([](sf::RenderTarget& rt) {
        rt.draw(circulo(28, 32, 32, sf::Color(255, 230, 90)));
        rt.draw(circulo(14, 32, 32, sf::Color(255, 255, 230)));
    });
    t.bola = desenhar([](sf::RenderTarget& rt) {
        rt.draw(circulo(30, 32, 32, sf::Color(230, 70, 30)));
        rt.draw(circulo(19, 32, 32, sf::Color(255, 160, 40)));
        rt.draw(circulo(9, 32, 32, sf::Color(255, 240, 150)));
    });
    t.gema = desenhar([](sf::RenderTarget& rt) {
        sf::ConvexShape g(4);
        g.setPoint(0, {32, 4});
        g.setPoint(1, {58, 32});
        g.setPoint(2, {32, 60});
        g.setPoint(3, {6, 32});
        g.setFillColor(sf::Color(80, 230, 220));
        g.setOutlineThickness(-3);
        g.setOutlineColor(sf::Color(20, 120, 130));
        rt.draw(g);
        rt.draw(circulo(6, 26, 24, sf::Color(220, 255, 255)));   // brilho
    });
    t.coracao = desenhar([](sf::RenderTarget& rt) {
        const sf::Color cor(235, 60, 80);
        rt.draw(circulo(15, 20, 24, cor));
        rt.draw(circulo(15, 44, 24, cor));
        sf::ConvexShape ponta(3);
        ponta.setPoint(0, {6, 30});
        ponta.setPoint(1, {58, 30});
        ponta.setPoint(2, {32, 58});
        ponta.setFillColor(cor);
        rt.draw(ponta);
    });
    t.espinho = desenhar([](sf::RenderTarget& rt) {
        sf::RectangleShape base({60, 60});
        base.setPosition(2, 2);
        base.setFillColor(sf::Color(60, 55, 60));
        rt.draw(base);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                sf::ConvexShape e(3);
                const float x = 6.0f + 18.0f * i;
                const float y = 6.0f + 18.0f * j;
                e.setPoint(0, {x, y + 16});
                e.setPoint(1, {x + 16, y + 16});
                e.setPoint(2, {x + 8, y});
                e.setFillColor(sf::Color(200, 200, 210));
                rt.draw(e);
            }
        }
    });
    t.escada = desenhar([](sf::RenderTarget& rt) {
        sf::RectangleShape buraco({60, 60});
        buraco.setPosition(2, 2);
        buraco.setFillColor(sf::Color(15, 10, 10));
        rt.draw(buraco);
        for (int i = 0; i < 5; ++i) {   // degraus cada vez mais escuros
            sf::RectangleShape degrau({52.0f - 6.0f * i, 8});
            degrau.setPosition(6.0f + 3.0f * i, 6.0f + 11.0f * i);
            const sf::Uint8 c = static_cast<sf::Uint8>(190 - 32 * i);
            degrau.setFillColor(sf::Color(c, static_cast<sf::Uint8>(c * 0.7f), static_cast<sf::Uint8>(c * 0.4f)));
            rt.draw(degrau);
        }
    });
    return t;
}

// --- Cenário: paredes, linha de visão e vento ---------------------------------

/// true se a caixa encosta em alguma parede (ou sai do mapa).
bool tocaParede(const TileMap& mapa, const AABB& caixa) {
    int c0 = 0, r0 = 0, c1 = 0, r1 = 0;
    mapa.toIndices(caixa.min, c0, r0);
    mapa.toIndices(caixa.max - Vector2D(0.01f, 0.01f), c1, r1);   // borda direita não invade o tile vizinho
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            if (c < 0 || r < 0 || c >= mapa.width() || r >= mapa.height() || mapa.isWall(c, r)) {
                return true;
            }
        }
    }
    return false;
}

/// Integra o corpo um passo e desfaz o movimento no eixo que entrou na parede
/// (resolve x e y separados, assim dá para "deslizar" encostado na parede).
/// @return true se bateu em alguma parede.
bool moverComParedes(Entity& e, const TileMap& mapa) {
    const Vector2D antes = e.body.position;
    e.body.integrate(DT);
    const Vector2D depois = e.body.position;
    bool bateu = false;

    e.body.position = { depois.x, antes.y };
    if (tocaParede(mapa, e.bounds())) {
        e.body.position.x = antes.x;
        e.body.velocity.x = 0.0f;
        bateu = true;
    }
    e.body.position.y = depois.y;
    if (tocaParede(mapa, e.bounds())) {
        e.body.position.y = antes.y;
        e.body.velocity.y = 0.0f;
        bateu = true;
    }
    return bateu;
}

/// Anda pelo segmento a-b em passos de 16 px; falha se cruzar uma parede.
bool linhaDeVisao(const TileMap& mapa, const Vector2D& a, const Vector2D& b) {
    const Vector2D d = b - a;
    const int passos = static_cast<int>(d.length() / 16.0f) + 1;
    for (int i = 0; i <= passos; ++i) {
        int c = 0, r = 0;
        mapa.toIndices(a + d * (static_cast<float>(i) / passos), c, r);
        if (c < 0 || r < 0 || c >= mapa.width() || r >= mapa.height() || mapa.isWall(c, r)) {
            return false;
        }
    }
    return true;
}

Vector2D ventoEm(const Jogo& jogo, const Vector2D& p) {
    Vector2D total;
    for (const ZonaVento& z : jogo.ventos) {
        if (z.area.contains(p.x, p.y)) total += z.forca;
    }
    return total;
}

// --- Criação de coisas ---------------------------------------------------------

void prepararSprite(Coisa& c, const sf::Texture& textura) {
    const float meio = TILE_SIZE * 0.5f;
    c.sprite.setTexture(textura, true);
    c.sprite.setOrigin(meio, meio);
    c.sprite.setScale(c.collision.halfExtents.x / meio, c.collision.halfExtents.y / meio);
}

/// Cria uma coisa, dá um id novo, registra no gerente de colisão e no mapa de ids.
template <class T>
T& criar(Jogo& jogo, Tipo tipo, const Vector2D& pos, float meiaLargura, const sf::Texture& textura) {
    auto nova = std::make_unique<T>();
    nova->tipo = tipo;
    nova->id = jogo.colisoes.newId();
    nova->body.position = pos;
    nova->collision.halfExtents = { meiaLargura, meiaLargura };
    prepararSprite(*nova, textura);

    T& ref = *nova;
    jogo.colisoes.add(ref);
    jogo.porId[ref.id] = &ref;
    jogo.coisas.push_back(std::move(nova));
    return ref;
}

void atirar(Jogo& jogo, Tipo tipo, const Vector2D& origem, const Vector2D& direcao) {
    const bool doJogador = (tipo == Tipo::TiroJogador);
    Projetil& p = criar<Projetil>(jogo, tipo, origem, doJogador ? 8.0f : 10.0f,
                                  doJogador ? jogo.tex.tiro : jogo.tex.bola);
    p.body.velocity = direcao * (doJogador ? VELOCIDADE_TIRO : VELOCIDADE_BOLA);
    p.duracao = doJogador ? DURACAO_TIRO : DURACAO_BOLA;
}

// --- Montagem de um nível -------------------------------------------------------

void coletarSalas(const BSPNode* no, std::vector<Rect>& salas) {
    if (!no) return;
    if (no->isLeaf()) {
        salas.push_back(no->room);
        return;
    }
    coletarSalas(no->left.get(), salas);
    coletarSalas(no->right.get(), salas);
}

void montarPiso(Jogo& jogo) {
    const TileMap& m = jogo.mapa;
    jogo.piso.clear();
    for (int r = 0; r < m.height(); ++r) {
        for (int c = 0; c < m.width(); ++c) {
            sf::Color cor;
            if (!m.isWall(c, r)) {
                cor = ((c + r) % 2 == 0) ? sf::Color(52, 47, 60) : sf::Color(58, 53, 66);
            } else {
                // Parede encostada no piso fica mais clara (borda visível).
                bool borda = false;
                for (int dr = -1; dr <= 1 && !borda; ++dr) {
                    for (int dc = -1; dc <= 1 && !borda; ++dc) {
                        const int cc = c + dc, rr = r + dr;
                        borda = cc >= 0 && rr >= 0 && cc < m.width() && rr < m.height() && !m.isWall(cc, rr);
                    }
                }
                cor = borda ? sf::Color(84, 74, 96) : sf::Color(22, 18, 28);
            }
            const float x = c * TILE_SIZE, y = r * TILE_SIZE;
            jogo.piso.append(sf::Vertex({ x, y }, cor));
            jogo.piso.append(sf::Vertex({ x + TILE_SIZE, y }, cor));
            jogo.piso.append(sf::Vertex({ x + TILE_SIZE, y + TILE_SIZE }, cor));
            jogo.piso.append(sf::Vertex({ x, y + TILE_SIZE }, cor));
        }
    }
}

void novoNivel(Jogo& jogo) {
    jogo.colisoes.clear();
    jogo.coisas.clear();
    jogo.porId.clear();
    jogo.ventos.clear();
    jogo.particulas.clear();
    jogo.textos.clear();
    jogo.rastros.clear();
    jogo.tremor = 0.0f;
    jogo.descer = false;

    // 1) Cenário: masmorra BSP -> matriz de tiles.
    Dungeon2D masmorra(MAPA_LARGURA, MAPA_ALTURA);
    masmorra.generate(jogo.rng());
    jogo.mapa.loadFromDungeon(masmorra);
    jogo.caminhos = std::make_unique<PathFind>(jogo.mapa.data(), jogo.mapa.width(), jogo.mapa.height());
    montarPiso(jogo);

    std::vector<Rect> salas;
    coletarSalas(masmorra.root.get(), salas);
    auto centro = [&](const Rect& s) { return jogo.mapa.toWorld((s.left + s.right) / 2, (s.top + s.bottom) / 2); };

    // 2) Jogador no centro da primeira sala.
    Jogador& j = jogo.jogador;
    j.id = PLAYER_ID;
    j.tipo = Tipo::Jogador;
    j.active = true;
    j.collision.halfExtents = { JOGADOR_MEIA_LARGURA, JOGADOR_MEIA_LARGURA };
    j.body.position = centro(salas.front());
    j.body.velocity = {};
    j.body.acceleration = {};
    j.invulneravel = 1.0f;   // um respiro no começo do nível
    j.dash = 0.0f;
    j.recargaDash = 0.0f;
    prepararSprite(j, jogo.tex.jogador);
    jogo.colisoes.add(j);
    jogo.porId[j.id] = &j;

    // 3) Escada na sala mais longe do início.
    std::size_t salaEscada = 0;
    float maiorDist = -1.0f;
    for (std::size_t i = 1; i < salas.size(); ++i) {
        const float d = (centro(salas[i]) - j.body.position).length_squared();
        if (d > maiorDist) { maiorDist = d; salaEscada = i; }
    }
    criar<Coisa>(jogo, Tipo::Escada, centro(salas[salaEscada]), 28.0f, jogo.tex.escada);

    // 4) Salas de vento: ~1/3 das salas (nunca a inicial), direção sorteada.
    std::vector<std::size_t> candidatas;
    for (std::size_t i = 1; i < salas.size(); ++i) candidatas.push_back(i);
    std::shuffle(candidatas.begin(), candidatas.end(), jogo.rng);
    const std::size_t qtdVento = std::min(candidatas.size(), std::max<std::size_t>(2, salas.size() / 3));
    const Vector2D direcoes[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    for (std::size_t k = 0; k < qtdVento; ++k) {
        const Rect& s = salas[candidatas[k]];
        ZonaVento z;
        z.area = sf::FloatRect(s.left * TILE_SIZE, s.top * TILE_SIZE,
                               (s.right - s.left + 1) * TILE_SIZE, (s.bottom - s.top + 1) * TILE_SIZE);
        z.forca = direcoes[jogo.rng() % 4] * FORCA_VENTO;
        jogo.ventos.push_back(z);
    }

    // 5) Itens e inimigos em células vazias sorteadas, sem empilhar.
    std::vector<bool> ocupada(static_cast<std::size_t>(jogo.mapa.width() * jogo.mapa.height()), false);
    auto marcar = [&](const Vector2D& p) {
        int c = 0, r = 0;
        jogo.mapa.toIndices(p, c, r);
        ocupada[static_cast<std::size_t>(r * jogo.mapa.width() + c)] = true;
    };
    marcar(j.body.position);
    marcar(centro(salas[salaEscada]));

    auto sortearCelula = [&](float distanciaMinima, Vector2D& saida) {
        for (int tentativa = 0; tentativa < 300; ++tentativa) {
            int c = 0, r = 0;
            jogo.mapa.randomEmptyCell(c, r);
            const Vector2D p = jogo.mapa.toWorld(c, r);
            if (ocupada[static_cast<std::size_t>(r * jogo.mapa.width() + c)]) continue;
            if ((p - j.body.position).length() < distanciaMinima) continue;
            ocupada[static_cast<std::size_t>(r * jogo.mapa.width() + c)] = true;
            saida = p;
            return true;
        }
        return false;
    };

    Vector2D p;
    jogo.gemasTotal = 0;
    for (int i = 0; i < 10; ++i) {
        if (sortearCelula(0.0f, p)) { criar<Coisa>(jogo, Tipo::Gema, p, 16.0f, jogo.tex.gema); ++jogo.gemasTotal; }
    }
    for (int i = 0; i < 2; ++i) {
        if (sortearCelula(4 * TILE_SIZE, p)) criar<Coisa>(jogo, Tipo::Coracao, p, 16.0f, jogo.tex.coracao);
    }
    for (int i = 0; i < 4 + jogo.nivel; ++i) {
        if (sortearCelula(4 * TILE_SIZE, p)) criar<Coisa>(jogo, Tipo::Espinho, p, 24.0f, jogo.tex.espinho);
    }
    const int qtdEspectros = std::min(3 + 2 * jogo.nivel, 15);
    for (int i = 0; i < qtdEspectros; ++i) {
        if (sortearCelula(8 * TILE_SIZE, p)) {
            Espectro& e = criar<Espectro>(jogo, Tipo::Espectro, p, ESPECTRO_MEIA_LARGURA, jogo.tex.espectro);
            e.recarga = ESPECTRO_RECARGA;
            e.fase = static_cast<float>(jogo.rng() % 628) / 100.0f;
        }
    }
}

void recomecar(Jogo& jogo) {
    jogo.pontos = 0;
    jogo.nivel = 1;
    jogo.fimDeJogo = false;
    jogo.jogador.vidas = VIDAS_INICIAIS;
    jogo.jogador.recarga = 0.0f;
    jogo.jogador.mira = { 1.0f, 0.0f };
    novoNivel(jogo);
}

// --- Atualização (um passo de DT) -----------------------------------------------

void atualizarJogador(Jogo& jogo, bool temFoco) {
    Jogador& j = jogo.jogador;

    Vector2D entrada;
    if (temFoco) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))  entrada.x -= 1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) entrada.x += 1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))    entrada.y -= 1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))  entrada.y += 1.0f;
    }
    if (entrada.length_squared() > 0.0f) {
        entrada.normalize();   // diagonal não fica mais rápida
        j.mira = entrada;
    }

    // Dash: dispara ao apertar Shift (ou Z), se a recarga acabou.
    const bool teclaDash = temFoco && (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ||
                                       sf::Keyboard::isKeyPressed(sf::Keyboard::RShift) ||
                                       sf::Keyboard::isKeyPressed(sf::Keyboard::Z));
    if (teclaDash && !j.teclaDashAntes && j.recargaDash <= 0.0f && j.dash <= 0.0f) {
        j.dash = DASH_DURACAO;
        j.recargaDash = DASH_RECARGA;
        explodir(jogo, j.body.position, sf::Color(150, 200, 255), 10, 160.0f, 0.3f);
    }
    j.teclaDashAntes = teclaDash;

    if (j.dash > 0.0f) {
        // Durante o dash a velocidade é fixa na direção da mira (ignora setas e vento).
        j.body.velocity = j.mira * DASH_VELOCIDADE;
        j.body.acceleration = {};
        j.dash = std::max(0.0f, j.dash - DT);
        if (j.dash <= 0.0f) j.body.velocity = j.mira * (JOGADOR_ACELERACAO / ATRITO);   // sai na velocidade máxima normal
        jogo.rastros.push_back({ j.body.position, std::atan2(j.mira.y, j.mira.x) * 180.0f / 3.14159265f, 0.25f });
    } else {
        // Física: força das setas - atrito + vento (se estiver numa sala de vento).
        j.body.acceleration = entrada * JOGADOR_ACELERACAO - j.body.velocity * ATRITO
                            + ventoEm(jogo, j.body.position);
    }
    moverComParedes(j, jogo.mapa);

    j.recargaDash = std::max(0.0f, j.recargaDash - DT);
    j.invulneravel = std::max(0.0f, j.invulneravel - DT);
    j.recarga = std::max(0.0f, j.recarga - DT);
    if (temFoco && sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && j.recarga <= 0.0f) {
        atirar(jogo, Tipo::TiroJogador, j.body.position, j.mira);
        j.recarga = INTERVALO_TIRO;
    }
}

void seguirCaminho(Espectro& e, float velocidade) {
    e.body.acceleration = {};
    if (e.passo >= e.caminho.size()) {
        e.body.velocity = {};
        return;
    }
    const Vector2D alvo = e.caminho[e.passo];
    const Vector2D falta = alvo - e.body.position;
    const float distancia = falta.length();
    if (distancia <= velocidade * DT) {   // chega neste passo: encaixa no ponto
        e.body.position = alvo;
        e.body.velocity = {};
        ++e.passo;
        return;
    }
    e.body.velocity = falta * (velocidade / distancia);
    e.body.integrate(DT);
}

void atualizarEspectro(Jogo& jogo, Espectro& e) {
    e.piscar = std::max(0.0f, e.piscar - DT);
    e.recarga = std::max(0.0f, e.recarga - DT);

    // Atordoado: só recua (sem atravessar parede) e não decide nada.
    if (e.atordoado > 0.0f) {
        e.atordoado -= DT;
        e.body.velocity = e.empurrao;
        e.body.acceleration = {};
        moverComParedes(e, jogo.mapa);
        e.empurrao *= 0.85f;
        // Saiu do caminho: na volta, recalcula a partir de onde parou.
        e.caminho.clear();
        e.passo = 0;
        e.recalculo = 0.0f;
        return;
    }

    const Vector2D alvo = jogo.jogador.body.position;
    const Vector2D paraJogador = alvo - e.body.position;
    const float distancia = paraJogador.length();
    const bool visivel = distancia <= ESPECTRO_VISAO && linhaDeVisao(jogo.mapa, e.body.position, alvo);

    // O estado do jogo, visto pelo Espectro, montado a cada passo.
    const BTContext estado = {
        { "JogadorVisivel", visivel ? 1 : 0 },
        { "JogadorPerto",   distancia <= ESPECTRO_ALCANCE_TIRO ? 1 : 0 },
        { "ArmaPronta",     e.recarga <= 0.0f ? 1 : 0 },
        { "TemDestino",     e.passo < e.caminho.size() ? 1 : 0 },
    };
    const std::string comando = CerebroEspectro.tick(estado);

    e.recalculo = std::max(0.0f, e.recalculo - DT);
    float velocidade = ESPECTRO_VEL_VAGAR;

    if (comando == "Atirar") {
        if (distancia > EPSILON) atirar(jogo, Tipo::BolaDeFogo, e.body.position, paraJogador / distancia);
        e.recarga = ESPECTRO_RECARGA;
        velocidade = ESPECTRO_VEL_CACAR;   // atira sem parar de andar
    } else if (comando == "Perseguir") {
        velocidade = ESPECTRO_VEL_CACAR;
        if (e.recalculo <= 0.0f) {   // A* até o jogador, mas não a cada passo
            e.caminho = jogo.caminhos->findPath(e.body.position, alvo);
            e.passo = 0;
            e.recalculo = ESPECTRO_RECALCULO;
        }
    } else if (comando == "EscolherDestino") {
        // Um ponto vazio aleatório não muito longe, para vagar até lá.
        int c = 0, r = 0, ec = 0, er = 0;
        jogo.mapa.toIndices(e.body.position, ec, er);
        for (int t = 0; t < 20; ++t) {
            jogo.mapa.randomEmptyCell(c, r);
            if (std::abs(c - ec) <= 8 && std::abs(r - er) <= 8) break;
        }
        e.caminho = jogo.caminhos->findPath(e.body.position, jogo.mapa.toWorld(c, r));
        e.passo = 0;
    }
    // "Vagar": só continua seguindo o caminho atual.

    seguirCaminho(e, velocidade);
}

void atualizarProjetil(Jogo& jogo, Projetil& p) {
    p.body.acceleration = ventoEm(jogo, p.body.position);   // o vento desvia os tiros
    p.duracao -= DT;
    if (moverComParedes(p, jogo.mapa) || p.duracao <= 0.0f) {
        p.active = false;
    }
}

void ferirJogador(Jogo& jogo, const Vector2D& origem) {
    Jogador& j = jogo.jogador;
    if (j.invulneravel > 0.0f || j.dash > 0.0f || jogo.fimDeJogo) return;   // dash atravessa o perigo
    --j.vidas;
    j.invulneravel = TEMPO_INVULNERAVEL;
    jogo.tremor = 0.25f;
    explodir(jogo, j.body.position, sf::Color(255, 90, 90), 14, 220.0f);
    textoFlutuante(jogo, j.body.position, "-1 vida", sf::Color(255, 110, 110));
    const Vector2D fuga = j.body.position - origem;
    if (fuga.length() > EPSILON) j.body.velocity += fuga.normalized() * EMPURRAO_DANO;
    if (j.vidas <= 0) jogo.fimDeJogo = true;
}

/// Trata cada par devolvido pelo gerente de colisão.
void tratarColisoes(Jogo& jogo) {
    for (const CollisionManager::Pair& par : jogo.colisoes.findCollisions()) {
        Coisa* a = jogo.porId.at(par.first);
        Coisa* b = jogo.porId.at(par.second);
        if (!a->active || !b->active) continue;   // já consumido por outro par neste passo

        // Ordena para o jogador (ou o tiro do jogador) ficar sempre em 'a'.
        if (b->tipo == Tipo::Jogador || (a->tipo != Tipo::Jogador && b->tipo == Tipo::TiroJogador)) {
            std::swap(a, b);
        }

        if (a->tipo == Tipo::Jogador) {
            switch (b->tipo) {
            case Tipo::Espectro:   ferirJogador(jogo, b->body.position); break;
            case Tipo::Espinho:    ferirJogador(jogo, b->body.position); break;
            case Tipo::BolaDeFogo: ferirJogador(jogo, b->body.position); b->active = false; break;
            case Tipo::Gema:
                jogo.pontos += PONTOS_GEMA;
                b->active = false;
                explodir(jogo, b->body.position, sf::Color(120, 240, 230), 10, 150.0f);
                textoFlutuante(jogo, b->body.position, "+" + std::to_string(PONTOS_GEMA), sf::Color(140, 245, 235));
                break;
            case Tipo::Coracao:
                if (jogo.jogador.vidas < VIDAS_MAXIMAS) {
                    ++jogo.jogador.vidas;
                    b->active = false;
                    explodir(jogo, b->body.position, sf::Color(255, 120, 140), 10, 150.0f);
                    textoFlutuante(jogo, b->body.position, "+1 vida", sf::Color(255, 150, 170));
                }
                break;
            case Tipo::Escada:     jogo.descer = true; break;
            default: break;
            }
        } else if (a->tipo == Tipo::TiroJogador && b->tipo == Tipo::Espectro) {
            a->active = false;
            Espectro* e = static_cast<Espectro*>(b);
            // Feedback do acerto: pisca, recua na direção do tiro e solta faíscas.
            e->piscar = ESPECTRO_PISCAR;
            e->atordoado = ESPECTRO_ATORDOADO;
            const Vector2D direcaoTiro = a->body.velocity;
            e->empurrao = direcaoTiro.length() > EPSILON ? direcaoTiro.normalized() * ESPECTRO_EMPURRAO : Vector2D{};
            explodir(jogo, a->body.position, sf::Color(255, 240, 160), 8, 200.0f, 0.3f);
            if (--e->vida <= 0) {
                e->active = false;
                jogo.pontos += PONTOS_ESPECTRO;
                explodir(jogo, e->body.position, sf::Color(190, 140, 255), 26, 260.0f, 0.7f);
                explodir(jogo, e->body.position, sf::Color(255, 255, 255), 8, 140.0f, 0.4f);
                textoFlutuante(jogo, e->body.position, "+" + std::to_string(PONTOS_ESPECTRO), sf::Color(255, 230, 120));
            }
        }
    }
}

void removerInativos(Jogo& jogo) {
    for (const auto& c : jogo.coisas) {
        if (!c->active) {
            jogo.colisoes.remove(c->id);
            jogo.porId.erase(c->id);
        }
    }
    jogo.coisas.erase(std::remove_if(jogo.coisas.begin(), jogo.coisas.end(),
                                     [](const std::unique_ptr<Coisa>& c) { return !c->active; }),
                      jogo.coisas.end());
}

void atualizarEfeitos(Jogo& jogo) {
    for (Particula& p : jogo.particulas) {
        p.pos += p.vel * DT;
        p.vel *= 0.92f;   // perdem velocidade aos poucos
        p.vida -= DT;
    }
    for (TextoFlutuante& t : jogo.textos) {
        t.pos.y -= 50.0f * DT;   // sobe
        t.vida -= DT;
    }
    for (Rastro& r : jogo.rastros) r.vida -= DT;
    jogo.tremor = std::max(0.0f, jogo.tremor - DT);

    jogo.particulas.erase(std::remove_if(jogo.particulas.begin(), jogo.particulas.end(),
                                         [](const Particula& p) { return p.vida <= 0.0f; }),
                          jogo.particulas.end());
    jogo.textos.erase(std::remove_if(jogo.textos.begin(), jogo.textos.end(),
                                     [](const TextoFlutuante& t) { return t.vida <= 0.0f; }),
                      jogo.textos.end());
    jogo.rastros.erase(std::remove_if(jogo.rastros.begin(), jogo.rastros.end(),
                                      [](const Rastro& r) { return r.vida <= 0.0f; }),
                       jogo.rastros.end());
}

void passo(Jogo& jogo, bool temFoco) {
    jogo.relogio += DT;
    atualizarEfeitos(jogo);
    atualizarJogador(jogo, temFoco);
    // Por índice: atirar() acrescenta coisas ao vetor durante o laço.
    for (std::size_t i = 0; i < jogo.coisas.size(); ++i) {
        Coisa& c = *jogo.coisas[i];
        if (!c.active) continue;
        if (c.tipo == Tipo::Espectro) {
            atualizarEspectro(jogo, static_cast<Espectro&>(c));
        } else if (c.tipo == Tipo::TiroJogador || c.tipo == Tipo::BolaDeFogo) {
            atualizarProjetil(jogo, static_cast<Projetil&>(c));
        }
    }
    tratarColisoes(jogo);
    removerInativos(jogo);

    if (jogo.descer && !jogo.fimDeJogo) {
        jogo.pontos += PONTOS_NIVEL;
        ++jogo.nivel;
        novoNivel(jogo);
    }
}

// --- Desenho ----------------------------------------------------------------------

void desenharVentos(sf::RenderWindow& janela, const Jogo& jogo) {
    const float espaco = 48.0f;
    for (const ZonaVento& z : jogo.ventos) {
        sf::RectangleShape fundo({ z.area.width, z.area.height });
        fundo.setPosition(z.area.left, z.area.top);
        fundo.setFillColor(sf::Color(120, 180, 255, 45));
        janela.draw(fundo);

        // Riscos que correm na direção do vento.
        const Vector2D dir = z.forca.normalized();
        const float desloc = std::fmod(jogo.relogio * 140.0f, espaco);
        sf::RectangleShape risco({ 22.0f, 3.0f });
        risco.setOrigin(11.0f, 1.5f);
        risco.setRotation(std::atan2(dir.y, dir.x) * 180.0f / 3.14159265f);
        risco.setFillColor(sf::Color(190, 220, 255, 150));
        for (float y = z.area.top + espaco / 2; y < z.area.top + z.area.height; y += espaco) {
            for (float x = z.area.left + espaco / 2; x < z.area.left + z.area.width; x += espaco) {
                float px = x + dir.x * desloc, py = y + dir.y * desloc;
                // dá a volta dentro da sala
                if (px > z.area.left + z.area.width) px -= z.area.width;
                if (px < z.area.left) px += z.area.width;
                if (py > z.area.top + z.area.height) py -= z.area.height;
                if (py < z.area.top) py += z.area.height;
                risco.setPosition(px, py);
                janela.draw(risco);
            }
        }
    }
}

void desenharMundo(sf::RenderWindow& janela, Jogo& jogo, const sf::Font* fonte) {
    // Câmera segue o jogador, sem mostrar fora do mapa; treme ao levar dano.
    const float mundoL = jogo.mapa.width() * TILE_SIZE, mundoA = jogo.mapa.height() * TILE_SIZE;
    sf::View camera({ 0, 0, static_cast<float>(LARGURA_JANELA), static_cast<float>(ALTURA_JANELA) });
    const float meioL = LARGURA_JANELA / 2.0f, meioA = ALTURA_JANELA / 2.0f;
    float cx = std::clamp(jogo.jogador.body.position.x, meioL, std::max(meioL, mundoL - meioL));
    float cy = std::clamp(jogo.jogador.body.position.y, meioA, std::max(meioA, mundoA - meioA));
    if (jogo.tremor > 0.0f) {
        const float forca = 10.0f * (jogo.tremor / 0.25f);
        cx += forca * std::sin(jogo.relogio * 90.0f);
        cy += forca * std::cos(jogo.relogio * 70.0f);
    }
    camera.setCenter(cx, cy);
    janela.setView(camera);

    janela.draw(jogo.piso);
    desenharVentos(janela, jogo);

    // Itens embaixo, depois inimigos e projéteis.
    for (int camada = 0; camada < 2; ++camada) {
        for (auto& c : jogo.coisas) {
            const bool item = c->tipo == Tipo::Gema || c->tipo == Tipo::Coracao ||
                              c->tipo == Tipo::Espinho || c->tipo == Tipo::Escada;
            if (item != (camada == 0)) continue;
            float y = c->body.position.y;
            if (c->tipo == Tipo::Espectro) {
                const Espectro& e = static_cast<const Espectro&>(*c);
                y += 4.0f * std::sin(jogo.relogio * 4.0f + e.fase);   // flutua
                // Levou tiro: fica vermelho e "incha" um pouco.
                const float base = ESPECTRO_MEIA_LARGURA / (TILE_SIZE * 0.5f);
                const float inchaco = 1.0f + 0.25f * (e.piscar / ESPECTRO_PISCAR);
                c->sprite.setScale(base * inchaco, base * inchaco);
                c->sprite.setColor(e.piscar > 0.0f ? sf::Color(255, 70, 70)
                                   : e.vida < ESPECTRO_VIDA ? sf::Color(255, 190, 190)   // ferido
                                                            : sf::Color::White);
            } else if (c->tipo == Tipo::Gema || c->tipo == Tipo::Coracao) {
                y += 2.5f * std::sin(jogo.relogio * 3.0f + c->body.position.x);
            }
            c->sprite.setPosition(c->body.position.x, y);
            janela.draw(c->sprite);
        }
    }

    // Rastro do dash: cópias do jogador que se apagam.
    Jogador& j = jogo.jogador;
    for (const Rastro& r : jogo.rastros) {
        sf::Sprite copia = j.sprite;
        copia.setPosition(r.pos.x, r.pos.y);
        copia.setRotation(r.rotacao);
        copia.setColor(sf::Color(150, 200, 255, static_cast<sf::Uint8>(160.0f * r.vida / 0.25f)));
        janela.draw(copia);
    }

    // Jogador pisca enquanto está invulnerável.
    if (j.invulneravel <= 0.0f || std::fmod(j.invulneravel, 0.2f) < 0.1f) {
        j.sprite.setPosition(j.body.position.x, j.body.position.y);
        j.sprite.setRotation(std::atan2(j.mira.y, j.mira.x) * 180.0f / 3.14159265f);
        janela.draw(j.sprite);
    }

    // Partículas por cima de tudo, apagando com o tempo.
    sf::RectangleShape ponto;
    for (const Particula& p : jogo.particulas) {
        const float f = p.vida / p.vidaMax;
        ponto.setSize({ p.tamanho * f + 1.0f, p.tamanho * f + 1.0f });
        ponto.setOrigin(ponto.getSize() / 2.0f);
        ponto.setPosition(p.pos.x, p.pos.y);
        sf::Color cor = p.cor;
        cor.a = static_cast<sf::Uint8>(255.0f * f);
        ponto.setFillColor(cor);
        janela.draw(ponto);
    }

    // Textos que sobem ("+100", "-1 vida").
    if (fonte) {
        for (const TextoFlutuante& t : jogo.textos) {
            sf::Text txt(utf8(t.texto), *fonte, 22);
            sf::Color cor = t.cor;
            cor.a = static_cast<sf::Uint8>(255.0f * std::min(1.0f, t.vida / 0.4f));
            txt.setFillColor(cor);
            txt.setOutlineColor(sf::Color(0, 0, 0, cor.a));
            txt.setOutlineThickness(2.0f);
            const sf::FloatRect b = txt.getLocalBounds();
            txt.setOrigin(b.left + b.width / 2.0f, b.top + b.height / 2.0f);
            txt.setPosition(t.pos.x, t.pos.y - 30.0f);
            janela.draw(txt);
        }
    }
}

void desenharTexto(sf::RenderWindow& janela, const sf::Font& fonte, const std::string& s,
                   unsigned tamanho, float x, float y, bool centralizar = false,
                   sf::Color cor = sf::Color::White) {
    sf::Text t(utf8(s), fonte, tamanho);
    t.setFillColor(cor);
    t.setOutlineColor(sf::Color::Black);
    t.setOutlineThickness(2.0f);
    if (centralizar) {
        const sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(b.left + b.width / 2.0f, b.top + b.height / 2.0f);
    }
    t.setPosition(x, y);
    janela.draw(t);
}

void desenharPlacar(sf::RenderWindow& janela, const Jogo& jogo, const sf::Font* fonte) {
    janela.setView(janela.getDefaultView());
    const std::string placar = "Vidas: " + std::to_string(jogo.jogador.vidas) +
                               "     Pontos: " + std::to_string(jogo.pontos) +
                               "     Nível: " + std::to_string(jogo.nivel);
    if (!fonte) {   // sem fonte: placar no título da janela
        janela.setTitle(utf8("Cripta - " + placar));
        return;
    }

    sf::RectangleShape barra({ static_cast<float>(LARGURA_JANELA), 44.0f });
    barra.setFillColor(sf::Color(0, 0, 0, 150));
    janela.draw(barra);
    desenharTexto(janela, *fonte, placar, 24, 16.0f, 8.0f);
    if (ventoEm(jogo, jogo.jogador.body.position).length_squared() > 0.0f) {
        desenharTexto(janela, *fonte, "~ vento ~", 22, LARGURA_JANELA - 140.0f, 10.0f, false,
                      sf::Color(170, 210, 255));
    }
    // Dash: pronto ou barra de recarga.
    const Jogador& j = jogo.jogador;
    const float xDash = 560.0f;
    if (j.recargaDash <= 0.0f) {
        desenharTexto(janela, *fonte, "Dash: pronto", 22, xDash, 10.0f, false, sf::Color(150, 210, 255));
    } else {
        desenharTexto(janela, *fonte, "Dash:", 22, xDash, 10.0f, false, sf::Color(130, 150, 180));
        sf::RectangleShape fundo({ 100.0f, 10.0f });
        fundo.setPosition(xDash + 70.0f, 19.0f);
        fundo.setFillColor(sf::Color(60, 60, 80));
        janela.draw(fundo);
        sf::RectangleShape barra({ 100.0f * (1.0f - j.recargaDash / DASH_RECARGA), 10.0f });
        barra.setPosition(xDash + 70.0f, 19.0f);
        barra.setFillColor(sf::Color(150, 210, 255));
        janela.draw(barra);
    }
    desenharTexto(janela, *fonte, "Setas: mover   Espaço: atirar   Shift ou Z: dash   Ache a escada para descer",
                  16, 16.0f, ALTURA_JANELA - 28.0f, false, sf::Color(200, 200, 200));

    if (jogo.fimDeJogo) {
        sf::RectangleShape veu({ static_cast<float>(LARGURA_JANELA), static_cast<float>(ALTURA_JANELA) });
        veu.setFillColor(sf::Color(0, 0, 0, 170));
        janela.draw(veu);
        const float cx = LARGURA_JANELA / 2.0f, cy = ALTURA_JANELA / 2.0f;
        desenharTexto(janela, *fonte, "FIM DE JOGO", 64, cx, cy - 70.0f, true, sf::Color(235, 70, 80));
        desenharTexto(janela, *fonte, "Pontos: " + std::to_string(jogo.pontos) +
                                      "   Nível: " + std::to_string(jogo.nivel), 30, cx, cy + 10.0f, true);
        desenharTexto(janela, *fonte, "R: jogar de novo     ESC: sair", 22, cx, cy + 70.0f, true,
                      sf::Color(200, 200, 200));
    }
}

bool carregarFonte(sf::Font& fonte) {
    const char* caminhos[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (const char* c : caminhos) {
        if (fonte.loadFromFile(c)) return true;
    }
    return false;
}

} // namespace

// ----------------------------------------------------------------------------------

int main() {
    sf::RenderWindow janela(sf::VideoMode(LARGURA_JANELA, ALTURA_JANELA), "Cripta");
    janela.setFramerateLimit(60);
    janela.setKeyRepeatEnabled(false);

    sf::Font fonte;
    const bool temFonte = carregarFonte(fonte);

    const Texturas texturas = criarTexturas();
    Jogo jogo(texturas);
    recomecar(jogo);

    // GameLoop com passo fixo, como no teste2.
    sf::Clock relogio;
    float acumulado = 0.0f;
    while (janela.isOpen()) {
        sf::Event evento;
        while (janela.pollEvent(evento)) {
            if (evento.type == sf::Event::Closed) janela.close();
            if (evento.type == sf::Event::KeyPressed) {
                if (evento.key.code == sf::Keyboard::Escape) janela.close();
                if (evento.key.code == sf::Keyboard::R && jogo.fimDeJogo) recomecar(jogo);
            }
        }

        acumulado += relogio.restart().asSeconds();
        if (acumulado > 0.25f) acumulado = 0.25f;
        while (acumulado >= DT) {
            if (!jogo.fimDeJogo) passo(jogo, janela.hasFocus());
            acumulado -= DT;
        }

        janela.clear(sf::Color(22, 18, 28));
        desenharMundo(janela, jogo, temFonte ? &fonte : nullptr);
        desenharPlacar(janela, jogo, temFonte ? &fonte : nullptr);
        janela.display();
    }
    return 0;
}
