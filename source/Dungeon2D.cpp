#include "Dungeon2D.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <stdexcept>

// Rect convention used throughout: (left, top) and (right, bottom) are the
// corner CELLS and both are inclusive, so a rect spans
// (right - left + 1) x (bottom - top + 1) tiles.

namespace {

constexpr int MIN_ROOM_SIZE  = 5;                   // smallest room: 5 x 5
constexpr int CORRIDOR_WIDTH = 3;                   // corridors are 3 tiles wide
constexpr int ROOM_MARGIN    = 1;                   // wall between a room and its region border
constexpr int MIN_REGION     = MIN_ROOM_SIZE + 2 * ROOM_MARGIN;   // smallest leaf region: 7
constexpr int MAX_LEAF       = 16;                  // regions up to this size may stop splitting
constexpr int STOP_PERCENT   = 40;                  // chance (%) such a region becomes a leaf

int rectWidth(const Rect& r) noexcept  { return r.right - r.left + 1; }
int rectHeight(const Rect& r) noexcept { return r.bottom - r.top + 1; }
int centerX(const Rect& r) noexcept    { return (r.left + r.right) / 2; }
int centerY(const Rect& r) noexcept    { return (r.top + r.bottom) / 2; }

/// Uniform-ish integer in [lo, hi]. std::mt19937's output sequence is fixed by
/// the standard, while std::uniform_int_distribution is implementation-defined;
/// using the raw engine keeps "same seed -> same dungeon" across compilers.
int randRange(std::mt19937& rng, int lo, int hi) {
    const auto span = static_cast<std::uint32_t>(hi - lo + 1);
    return lo + static_cast<int>(static_cast<std::uint32_t>(rng()) % span);
}

void collectLeaves(const BSPNode& node, std::vector<const BSPNode*>& out) {
    if (node.isLeaf()) {
        out.push_back(&node);
        return;
    }
    if (node.left)  { collectLeaves(*node.left, out); }
    if (node.right) { collectLeaves(*node.right, out); }
}

/// Places a random room of at least MIN_ROOM_SIZE inside the leaf's region,
/// keeping ROOM_MARGIN wall tiles to the region border.
void carveRoom(BSPNode& leaf, std::mt19937& rng) {
    const Rect& reg = leaf.region;
    const int w = randRange(rng, MIN_ROOM_SIZE, rectWidth(reg)  - 2 * ROOM_MARGIN);
    const int h = randRange(rng, MIN_ROOM_SIZE, rectHeight(reg) - 2 * ROOM_MARGIN);
    const int left = randRange(rng, reg.left + ROOM_MARGIN, reg.right  - ROOM_MARGIN - (w - 1));
    const int top  = randRange(rng, reg.top  + ROOM_MARGIN, reg.bottom - ROOM_MARGIN - (h - 1));
    leaf.room = Rect{ left, top, left + w - 1, top + h - 1 };
}

/// Adds an L-shaped connection between cells (ax, ay) and (bx, by) as up to
/// two straight corridors, CORRIDOR_WIDTH tiles wide and centred on the path.
/// The two segments overlap at the elbow, so the path is continuous.
void addConnection(std::vector<Corridor>& corridors,
                   int ax, int ay, int bx, int by, bool horizontalFirst) {
    constexpr int half = CORRIDOR_WIDTH / 2;
    const int elbowX = horizontalFirst ? bx : ax;
    const int elbowY = horizontalFirst ? ay : by;

    auto horizontal = [&](int y, int x0, int x1) {
        if (x0 == x1) { return; }
        corridors.push_back(Corridor{ std::min(x0, x1) - half, y - half,
                                      std::max(x0, x1) + half, y + half });
    };
    auto vertical = [&](int x, int y0, int y1) {
        if (y0 == y1) { return; }
        corridors.push_back(Corridor{ x - half, std::min(y0, y1) - half,
                                      x + half, std::max(y0, y1) + half });
    };

    if (horizontalFirst) {
        horizontal(ay, ax, elbowX);
        vertical(elbowX, elbowY, by);
    } else {
        vertical(ax, ay, elbowY);
        horizontal(elbowY, elbowX, bx);
    }
}

/// Joins the two subtrees of an internal node through their closest pair of
/// rooms. Each subtree is already connected, so after this call the whole
/// node is connected too (induction from the leaves up).
void connectChildren(const BSPNode& node, std::vector<Corridor>& corridors, std::mt19937& rng) {
    std::vector<const BSPNode*> a;
    std::vector<const BSPNode*> b;
    collectLeaves(*node.left, a);
    collectLeaves(*node.right, b);

    const BSPNode* bestA = a.front();
    const BSPNode* bestB = b.front();
    long long bestDist = -1;
    for (const BSPNode* la : a) {
        for (const BSPNode* lb : b) {
            const long long dx = centerX(la->room) - centerX(lb->room);
            const long long dy = centerY(la->room) - centerY(lb->room);
            const long long d = dx * dx + dy * dy;
            if (bestDist < 0 || d < bestDist) {
                bestDist = d;
                bestA = la;
                bestB = lb;
            }
        }
    }

    addConnection(corridors,
                  centerX(bestA->room), centerY(bestA->room),
                  centerX(bestB->room), centerY(bestB->room),
                  randRange(rng, 0, 1) == 0);
}

/// Recursively partitions node.region, carves rooms in the leaves and
/// connects every pair of sibling subtrees.
void build(BSPNode& node, std::vector<Corridor>& corridors, std::mt19937& rng) {
    const Rect reg = node.region;
    const int w = rectWidth(reg);
    const int h = rectHeight(reg);

    const bool canSplitV = w >= 2 * MIN_REGION;   // vertical cut: left | right
    const bool canSplitH = h >= 2 * MIN_REGION;   // horizontal cut: top / bottom
    const bool smallEnough = w <= MAX_LEAF && h <= MAX_LEAF;

    if ((!canSplitV && !canSplitH) ||
        (smallEnough && randRange(rng, 0, 99) < STOP_PERCENT)) {
        carveRoom(node, rng);
        return;
    }

    // Prefer cutting the longer side so regions stay roughly square.
    bool vertical;
    if (canSplitV && canSplitH) {
        if (w * 4 > h * 5)      { vertical = true; }
        else if (h * 4 > w * 5) { vertical = false; }
        else                    { vertical = randRange(rng, 0, 1) == 0; }
    } else {
        vertical = canSplitV;
    }

    node.left = std::make_unique<BSPNode>();
    node.right = std::make_unique<BSPNode>();
    if (vertical) {
        // left child: [reg.left, cut - 1], right child: [cut, reg.right]
        const int cut = randRange(rng, reg.left + MIN_REGION, reg.right + 1 - MIN_REGION);
        node.left->region  = Rect{ reg.left, reg.top, cut - 1,   reg.bottom };
        node.right->region = Rect{ cut,      reg.top, reg.right, reg.bottom };
    } else {
        const int cut = randRange(rng, reg.top + MIN_REGION, reg.bottom + 1 - MIN_REGION);
        node.left->region  = Rect{ reg.left, reg.top, reg.right, cut - 1    };
        node.right->region = Rect{ reg.left, cut,     reg.right, reg.bottom };
    }

    build(*node.left, corridors, rng);
    build(*node.right, corridors, rng);
    connectChildren(node, corridors, rng);
}

} // namespace

Dungeon2D::Dungeon2D(int widthTiles, int heightTiles)
    : width(widthTiles), height(heightTiles) {
    if (widthTiles <= 0 || heightTiles <= 0) {
        throw std::invalid_argument("Dungeon2D: width and height must be > 0");
    }
}

void Dungeon2D::generate(unsigned seed) {
    // The smallest room (5 x 5) plus a wall on every side must fit.
    if (width < MIN_REGION || height < MIN_REGION) {
        throw std::invalid_argument("Dungeon2D::generate: dungeon must be at least 7 x 7 tiles");
    }

    // Built into locals first, so the previous dungeon (if any) is replaced
    // only when generation finishes.
    std::mt19937 rng(seed);
    auto newRoot = std::make_unique<BSPNode>();
    newRoot->region = Rect{ 0, 0, width - 1, height - 1 };
    std::vector<Corridor> newCorridors;

    build(*newRoot, newCorridors, rng);

    root = std::move(newRoot);
    corridors = std::move(newCorridors);
}
