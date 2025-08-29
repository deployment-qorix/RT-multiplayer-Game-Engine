#pragma once

#include "component.h"
#include <map>
#include <memory>
#include <typeindex>

class Entity {
public:
    Entity(uint32_t id) : id_(id) {}
    uint32_t get_id() const { return id_; }

    template <typename T>
    T* get_component() {
        auto it = components_.find(typeid(T));
        if (it != components_.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }

    template <typename T, typename... Args>
    void add_component(Args&&... args) {a
        components_[typeid(T)] = std::make_unique<T>(std::forward<Args>(args)...);
    }

private:
    uint32_t id_;
    std::map<std::type_index, std::unique_ptr<Component>> components_;
};
