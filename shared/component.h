#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <typeindex>

// Base struct for all components
struct Component {
    virtual ~Component() = default;
};

// Component for position and rotation in the world
struct TransformComponent : public Component {
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

// Represents an Axis-Aligned Bounding Box for collision
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

// Component for physics-related properties
struct PhysicsComponent : public Component {
    AABB bounding_box;

    void update_bounding_box(const glm::vec3& position) {
        glm::vec3 size(0.5f, 1.0f, 0.5f);
        bounding_box.min = position - size;
        bounding_box.max = position + size;
    }
};

// Component to tag an entity as being controlled by a player
struct PlayerInputComponent : public Component {
    // This component is just a tag, it holds no data for now
};
