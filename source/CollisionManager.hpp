#ifndef GAME_COLLISIONMANAGER_HPP
#define GAME_COLLISIONMANAGER_HPP

#include <cstddef>
#include <utility>
#include <vector>

#include "Global.hpp"
#include "Entity.hpp"

/**
 * @file CollisionManager.hpp
 * @brief Keeps a list of every thing in the game and reports which pairs collide.
 *
 * Every registered thing is an Entity identified by its numeric id. The manager
 * stores a pointer to each one (it does not own them), so it always sees the
 * entity's CURRENT position and shape: there is nothing to synchronise each frame.
 *
 * findCollisions() tests the AABBs of all ACTIVE entities and returns the pairs
 * that overlap (touching counts, as in AABB::intersects). It uses sort-and-sweep
 * on the x axis, so it does not test every pair against every other.
 */
class CollisionManager {
public:
    /// A colliding pair of ids, always with first < second.
    using Pair = std::pair<EntityId, EntityId>;

    /**
     * @brief Hands out a fresh identifier: INITIAL_ENTITY_ID, then +1 each call.
     * @note Never returns PLAYER_ID, which is below the generated range.
     */
    [[nodiscard]] EntityId newId() noexcept;

    /**
     * @brief Registers an entity, identified by entity.id.
     * @pre The id is not already registered.
     * @throws std::invalid_argument if the id is already registered.
     * @note The entity is referenced, not copied: it must stay alive (and at
     *       the same address) until it is removed or the manager is cleared.
     */
    void add(const Entity& entity);

    /// @brief Unregisters the entity with this id. @return false if it was not registered.
    bool remove(EntityId id) noexcept;

    /// @brief Unregisters everything (the id counter is NOT reset).
    void clear() noexcept;

    [[nodiscard]] bool        contains(EntityId id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief Every pair of ACTIVE registered entities whose AABBs overlap.
     * @return Pairs with first < second, sorted ascending; each pair appears once.
     */
    [[nodiscard]] std::vector<Pair> findCollisions() const;

private:
    std::vector<const Entity*> m_entities;
    EntityId                   m_nextId{INITIAL_ENTITY_ID};
};

#endif // GAME_COLLISIONMANAGER_HPP
