#include "PathFind.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

namespace {

constexpr double ORTHOGONAL_COST = 1.0;
constexpr double DIAGONAL_COST   = 1.41421356237309504880;   // sqrt(2)

/// Maps one world coordinate to a cell index: floor(value / TILE_SIZE).
/// The range is checked BEFORE converting to int, so NaN, infinity and huge
/// values are rejected instead of causing an undefined conversion.
bool toCell(float value, int cellCount, int& cell) noexcept {
    const float index = std::floor(value / TILE_SIZE);
    if (!(index >= 0.0f && index < static_cast<float>(cellCount))) {
        return false;
    }
    cell = static_cast<int>(index);
    return true;
}

/// Exact remaining cost on an empty 8-connected grid. It never overestimates
/// the real cost, which is what makes the path found by A* a shortest one.
double octileDistance(int col, int row, int goalCol, int goalRow) noexcept {
    const int dx = std::abs(col - goalCol);
    const int dy = std::abs(row - goalRow);
    return ORTHOGONAL_COST * std::abs(dx - dy) + DIAGONAL_COST * std::min(dx, dy);
}

Vector2D cellCentre(int col, int row) noexcept {
    return Vector2D((static_cast<float>(col) + 0.5f) * TILE_SIZE,
                    (static_cast<float>(row) + 0.5f) * TILE_SIZE);
}

} // namespace

PathFind::PathFind(const int* grid, int width, int height)
    : m_grid(grid), m_width(width), m_height(height) {
    if (grid == nullptr) {
        throw std::invalid_argument("PathFind: grid must not be null");
    }
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("PathFind: width and height must be > 0");
    }
}

std::vector<Vector2D> PathFind::findPath(const Vector2D& startWorld,
                                         const Vector2D& goalWorld) const {
    int startCol = 0;
    int startRow = 0;
    int goalCol = 0;
    int goalRow = 0;
    if (!toCell(startWorld.x, m_width, startCol) || !toCell(startWorld.y, m_height, startRow) ||
        !toCell(goalWorld.x, m_width, goalCol) || !toCell(goalWorld.y, m_height, goalRow)) {
        return {};
    }

    const std::size_t width = static_cast<std::size_t>(m_width);
    const std::size_t cellCount = width * static_cast<std::size_t>(m_height);
    const auto indexOf = [width](int col, int row) {
        return static_cast<std::size_t>(row) * width + static_cast<std::size_t>(col);
    };
    const auto walkable = [&](int col, int row) {
        return m_grid[indexOf(col, row)] == EMPTY_TILE;
    };

    if (!walkable(startCol, startRow) || !walkable(goalCol, goalRow)) {
        return {};
    }

    const std::size_t startIndex = indexOf(startCol, startRow);
    const std::size_t goalIndex = indexOf(goalCol, goalRow);

    constexpr std::size_t NO_PARENT = std::numeric_limits<std::size_t>::max();
    std::vector<double> costFromStart(cellCount, std::numeric_limits<double>::infinity());
    std::vector<std::size_t> parent(cellCount, NO_PARENT);

    // Open list ordered by f = g + h, smallest first. A cell whose cost improves
    // is pushed again; the outdated entry is skipped when it is popped.
    struct OpenEntry {
        double estimatedTotal;
        double costSoFar;
        std::size_t index;
        bool operator>(const OpenEntry& rhs) const noexcept {
            return estimatedTotal > rhs.estimatedTotal;
        }
    };
    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>> open;

    costFromStart[startIndex] = 0.0;
    open.push(OpenEntry{ octileDistance(startCol, startRow, goalCol, goalRow), 0.0, startIndex });

    while (!open.empty()) {
        const OpenEntry current = open.top();
        open.pop();
        if (current.costSoFar > costFromStart[current.index]) {
            continue;
        }
        if (current.index == goalIndex) {
            break;
        }

        const int col = static_cast<int>(current.index % width);
        const int row = static_cast<int>(current.index / width);
        for (int dRow = -1; dRow <= 1; ++dRow) {
            for (int dCol = -1; dCol <= 1; ++dCol) {
                if (dCol == 0 && dRow == 0) {
                    continue;
                }
                const int nextCol = col + dCol;
                const int nextRow = row + dRow;
                if (nextCol < 0 || nextCol >= m_width || nextRow < 0 || nextRow >= m_height ||
                    !walkable(nextCol, nextRow)) {
                    continue;
                }

                const bool diagonal = dCol != 0 && dRow != 0;
                // A diagonal step needs both orthogonally adjacent cells empty,
                // so the path never clips the corner of a wall.
                if (diagonal && (!walkable(nextCol, row) || !walkable(col, nextRow))) {
                    continue;
                }

                const std::size_t nextIndex = indexOf(nextCol, nextRow);
                const double cost = current.costSoFar + (diagonal ? DIAGONAL_COST : ORTHOGONAL_COST);
                if (cost < costFromStart[nextIndex]) {
                    costFromStart[nextIndex] = cost;
                    parent[nextIndex] = current.index;
                    open.push(OpenEntry{ cost + octileDistance(nextCol, nextRow, goalCol, goalRow),
                                         cost, nextIndex });
                }
            }
        }
    }

    if (goalIndex != startIndex && parent[goalIndex] == NO_PARENT) {
        return {};   // the goal was never reached
    }

    // Walk the parents back from the goal, then reverse to get start -> goal.
    std::vector<Vector2D> path;
    for (std::size_t index = goalIndex; index != NO_PARENT; index = parent[index]) {
        path.push_back(cellCentre(static_cast<int>(index % width), static_cast<int>(index / width)));
    }
    std::reverse(path.begin(), path.end());
    return path;
}
