// Small demo program linked against the game library (libgame.a):
// generates a dungeon, rasterizes it into a TileMap and exports it as CSV.
//
// Usage: dungeon_export [width] [height] [seed] [output.csv]
//        defaults:        60      40       42     dungeon.csv

#include <cstdio>
#include <exception>
#include <string>

#include "Dungeon2D.hpp"
#include "Global.hpp"
#include "TileMap.hpp"

namespace {

bool parseInt(const char* text, long long& out) {
    try {
        std::size_t used = 0;
        out = std::stoll(text, &used);
        return used == std::string(text).size();
    } catch (const std::exception&) {
        return false;
    }
}

void printMap(const TileMap& map) {
    for (int row = 0; row < map.height(); ++row) {
        for (int col = 0; col < map.width(); ++col) {
            std::putchar(map.isWall(col, row) ? '#' : '.');
        }
        std::putchar('\n');
    }
}

} // namespace

int main(int argc, char** argv) {
    long long width = 60;
    long long height = 40;
    long long seed = 42;
    std::string path = "dungeon.csv";

    if ((argc > 1 && !parseInt(argv[1], width)) ||
        (argc > 2 && !parseInt(argv[2], height)) ||
        (argc > 3 && !parseInt(argv[3], seed)) ||
        width <= 0 || height <= 0 || width > 10000 || height > 10000 || seed < 0) {
        std::fprintf(stderr, "usage: %s [width] [height] [seed] [output.csv]\n", argv[0]);
        return 2;
    }
    if (argc > 4) {
        path = argv[4];
    }

    try {
        // 1. Generate the dungeon geometry (BSP tree + corridors).
        Dungeon2D dungeon(static_cast<int>(width), static_cast<int>(height));
        dungeon.generate(static_cast<unsigned>(seed));

        // 2. Rasterize it into the tile matrix.
        TileMap map;
        map.loadFromDungeon(dungeon);

        // 3. Export with the library's CSV function.
        if (!map.saveCSV(path)) {
            std::fprintf(stderr, "error: could not write '%s'\n", path.c_str());
            return 1;
        }

        // 4. Read it back with loadCSV to confirm the file is valid.
        TileMap check;
        if (!check.loadCSV(path) || check.width() != map.width() || check.height() != map.height()) {
            std::fprintf(stderr, "error: '%s' could not be read back\n", path.c_str());
            return 1;
        }

        printMap(map);
        int empty = 0;
        for (int i = 0; i < map.width() * map.height(); ++i) {
            empty += map.data()[i] == EMPTY_TILE ? 1 : 0;
        }
        std::printf("\n%dx%d tiles, seed %lld, %zu corridors, %d empty / %d wall cells\n",
                    map.width(), map.height(), seed, dungeon.corridors.size(),
                    empty, map.width() * map.height() - empty);
        std::printf("CSV written to: %s  (%d = empty, %d = wall)\n", path.c_str(), EMPTY_TILE, WALL_TILE);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
