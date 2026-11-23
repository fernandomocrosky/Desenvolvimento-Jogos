#include "CollisionManager.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

EntityId CollisionManager::newId() noexcept {
    return m_nextId++;
}

void CollisionManager::add(const Entity& entity) {
    if (contains(entity.id)) {
        throw std::invalid_argument("CollisionManager::add: id ja registrado: " +
                                    std::to_string(entity.id));
    }
    m_entities.push_back(&entity);
}

bool CollisionManager::remove(EntityId id) noexcept {
    const auto it = std::find_if(m_entities.begin(), m_entities.end(),
                                 [id](const Entity* e) { return e->id == id; });
    if (it == m_entities.end()) {
        return false;
    }
    m_entities.erase(it);
    return true;
}

void CollisionManager::clear() noexcept {
    m_entities.clear();
}

bool CollisionManager::contains(EntityId id) const noexcept {
    return std::any_of(m_entities.begin(), m_entities.end(),
                       [id](const Entity* e) { return e->id == id; });
}

std::size_t CollisionManager::size() const noexcept {
    return m_entities.size();
}

std::vector<CollisionManager::Pair> CollisionManager::findCollisions() const {
    // Snapshot of the active entities' boxes.
    struct Item {
        EntityId id;
        AABB     box;
    };
    std::vector<Item> items;
    items.reserve(m_entities.size());
    for (const Entity* e : m_entities) {
        if (e->active) {
            items.push_back({ e->id, e->bounds() });
        }
    }

    // Sort-and-sweep: ordered by the left edge, a box can only touch the boxes
    // that start before its right edge ends.
    std::sort(items.begin(), items.end(),
              [](const Item& a, const Item& b) { return a.box.min.x < b.box.min.x; });

    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < items.size(); ++i) {
        for (std::size_t j = i + 1; j < items.size() && items[j].box.min.x <= items[i].box.max.x; ++j) {
            if (items[i].box.intersects(items[j].box)) {
                pairs.emplace_back(std::min(items[i].id, items[j].id),
                                   std::max(items[i].id, items[j].id));
            }
        }
    }
    std::sort(pairs.begin(), pairs.end());
    return pairs;
}
