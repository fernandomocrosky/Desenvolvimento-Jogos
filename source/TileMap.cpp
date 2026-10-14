#include "TileMap.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <random>
#include <stdexcept>

#include "Dungeon2D.hpp"

namespace {

/// Sets every in-range cell of an inclusive tile rectangle to @p value.
void fillRect(std::vector<int>& cells, int mapWidth, int mapHeight, const Rect& r, int value) {
    const int left   = std::max(r.left, 0);
    const int top    = std::max(r.top, 0);
    const int right  = std::min(r.right, mapWidth - 1);
    const int bottom = std::min(r.bottom, mapHeight - 1);
    for (int row = top; row <= bottom; ++row) {
        for (int col = left; col <= right; ++col) {
            cells[static_cast<std::size_t>(row) * mapWidth + col] = value;
        }
    }
}

void carveLeaves(const BSPNode& node, std::vector<int>& cells, int mapWidth, int mapHeight) {
    if (node.isLeaf()) {
        fillRect(cells, mapWidth, mapHeight, node.room, EMPTY_TILE);
        return;
    }
    if (node.left)  { carveLeaves(*node.left, cells, mapWidth, mapHeight); }
    if (node.right) { carveLeaves(*node.right, cells, mapWidth, mapHeight); }
}

/// Parses one CSV line of comma-separated integers. Rejects empty fields
/// (so a trailing or doubled comma fails) and anything that is not an integer.
bool parseLine(const std::string& line, std::vector<int>& out) {
    const char* p = line.data();
    const char* const end = p + line.size();
    while (true) {
        int value = 0;
        const auto [next, ec] = std::from_chars(p, end, value);
        if (ec != std::errc() || next == p) {
            return false;
        }
        out.push_back(value);
        if (next == end) {
            return true;
        }
        if (*next != ',') {
            return false;
        }
        p = next + 1;
    }
}

} // namespace

// --- Construction from a dungeon --------------------------------------------

void TileMap::loadFromDungeon(const Dungeon2D& dungeon) {
    const int w = std::max(dungeon.width, 0);
    const int h = std::max(dungeon.height, 0);

    // Start solid, then carve rooms (BSP leaves) and corridors.
    std::vector<int> cells(static_cast<std::size_t>(w) * h, WALL_TILE);
    if (dungeon.root) {
        carveLeaves(*dungeon.root, cells, w, h);
    }
    for (const Corridor& c : dungeon.corridors) {
        fillRect(cells, w, h, c, EMPTY_TILE);
    }

    m_width = w;
    m_height = h;
    m_cells = std::move(cells);
}

// --- CSV persistence --------------------------------------------------------

bool TileMap::saveCSV(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    for (int row = 0; row < m_height; ++row) {
        for (int col = 0; col < m_width; ++col) {
            if (col > 0) {
                file << ',';
            }
            file << m_cells[static_cast<std::size_t>(row) * m_width + col];
        }
        file << '\n';
    }
    file.flush();
    return static_cast<bool>(file);
}

bool TileMap::loadCSV(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    // Everything is parsed into locals; members change only on success.
    std::vector<int> cells;
    int w = 0;
    int h = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {   // tolerate CRLF line endings
            line.pop_back();
        }
        const std::size_t before = cells.size();
        if (!parseLine(line, cells)) {
            return false;
        }
        const int count = static_cast<int>(cells.size() - before);
        if (h == 0) {
            w = count;
        } else if (count != w) {
            return false;   // every row must have the same number of columns
        }
        ++h;
    }
    if (file.bad()) {
        return false;
    }

    m_width = w;
    m_height = h;
    m_cells = std::move(cells);
    return true;
}

// --- Queries ----------------------------------------------------------------

int TileMap::width() const noexcept {
    return m_width;
}

int TileMap::height() const noexcept {
    return m_height;
}

int TileMap::at(int col, int row) const {
    // @pre in-range indices.
    if (col < 0 || col >= m_width || row < 0 || row >= m_height) {
        throw std::out_of_range("TileMap::at: cell index out of range");
    }
    return m_cells[static_cast<std::size_t>(row) * m_width + col];
}

bool TileMap::isWall(int col, int row) const {
    return at(col, row) == WALL_TILE;
}

const int* TileMap::data() const noexcept {
    return m_cells.data();
}

// --- Continuous <-> discrete translation ------------------------------------

void TileMap::toIndices(const Vector2D& world, int& col, int& row) const noexcept {
    // floor (not truncation) so negative coordinates map to negative cells.
    col = static_cast<int>(std::floor(world.x / TILE_SIZE));
    row = static_cast<int>(std::floor(world.y / TILE_SIZE));
}

Vector2D TileMap::toWorld(int col, int row) const noexcept {
    return Vector2D((static_cast<float>(col) + 0.5f) * TILE_SIZE,
                    (static_cast<float>(row) + 0.5f) * TILE_SIZE);
}

void TileMap::randomEmptyCell(int& col, int& row) const {
    std::vector<std::size_t> empty;
    for (std::size_t i = 0; i < m_cells.size(); ++i) {
        if (m_cells[i] == EMPTY_TILE) {
            empty.push_back(i);
        }
    }
    // @pre at least one EMPTY_TILE cell exists.
    if (empty.empty()) {
        throw std::logic_error("TileMap::randomEmptyCell: the map has no empty cell");
    }

    thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<std::size_t> pick(0, empty.size() - 1);
    const std::size_t index = empty[pick(rng)];
    col = static_cast<int>(index % static_cast<std::size_t>(m_width));
    row = static_cast<int>(index / static_cast<std::size_t>(m_width));
}
