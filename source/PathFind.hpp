#ifndef GAME_PATHFIND_HPP
#define GAME_PATHFIND_HPP

#include <vector>
#include "Global.hpp"
#include "Vector2D.hpp"

/**
 * @file PathFind.hpp
 * @brief A* shortest path over an integer tile matrix.
 *
 * The matrix is row-major (index = row*width + col); a cell is WALKABLE iff its
 * value is EMPTY_TILE. The matrix is referenced, not copied, and must outlive
 * this object.
 *
 * Search uses 8-CONNECTIVITY (orthogonal and diagonal moves). A diagonal move
 * between two cells is included only when BOTH cells orthogonally adjacent to
 * that diagonal are also empty, so a path never clips the corner of a wall.
 */
class PathFind {
public:
    /**
     * @param grid   Row-major matrix, size width*height.
     * @param width  Number of columns (> 0).
     * @param height Number of rows (> 0).
     */
    PathFind(const int* grid, int width, int height);

    /**
     * @brief Shortest walkable path between two world positions.
     * @param startWorld Continuous start position (pixels).
     * @param goalWorld  Continuous goal position (pixels).
     * @return Way points in continuous world coordinates, each the CENTRE of a
     *         cell along the path, ordered from start to goal. Empty if the
     *         goal is unreachable or an endpoint is not on a walkable in-range
     *         cell.
     * @note Endpoints are mapped to cells with col=floor(x/TILE_SIZE),
     *       row=floor(y/TILE_SIZE).
     */
    [[nodiscard]] std::vector<Vector2D> findPath(const Vector2D& startWorld,
                                                 const Vector2D& goalWorld) const;

private:
    const int* m_grid{nullptr};
    int        m_width{0};
    int        m_height{0};
};

#endif // GAME_PATHFIND_HPP
