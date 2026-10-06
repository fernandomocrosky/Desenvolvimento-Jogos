#include "Collision2D.hpp"

// --- AABB -------------------------------------------------------------------

bool AABB::intersects(const AABB& other) const noexcept {
    // Separating axis test: the boxes are disjoint only if one lies strictly
    // to one side of the other on some axis. Using <= (not <) makes touching
    // edges count as overlapping, as the contract requires.
    return min.x <= other.max.x && other.min.x <= max.x &&
           min.y <= other.max.y && other.min.y <= max.y;
}

// --- Collision2D ------------------------------------------------------------

AABB Collision2D::bounds(const Vector2D& position) const noexcept {
    // Centred on position; with non-negative half-extents the invariant
    // min <= max holds on both axes.
    return AABB{ position - halfExtents, position + halfExtents };
}
