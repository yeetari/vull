#pragma once

#include <vull/container/vector.hh>
#include <vull/ecs2/entity.hh>

namespace vull::ecs {

/**
 * @brief The root of the ECS.
 */
class World {
    Vector<Entity, EntityIndex> m_entity_list;
    EntityIndex m_free_head{Entity::null_index()};

public:
    /**
     * @brief Creates a new entity handle.
     *
     * @return the entity handle
     */
    Entity create();

    /**
     * @brief Destroys the given entity and returns its index to the free pool.
     *
     * @param entity a valid entity handle
     */
    void destroy(Entity entity);

    /**
     * @brief Returns true if the given entity handle refers to an active entity.
     *
     * @param entity the handle to check
     */
    bool is_valid(Entity entity) const;
};

} // namespace vull::ecs
