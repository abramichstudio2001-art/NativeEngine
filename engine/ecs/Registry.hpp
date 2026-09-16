#pragma once

#include "engine/ecs/Components.hpp"
#include <unordered_map>
#include <typeindex>
#include <any>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace NativeEngine::ECS {

using Entity = uint32_t;
constexpr Entity NullEntity = 0;

class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    virtual void Remove(Entity entity) = 0;
    virtual bool Has(Entity entity) const = 0;
};

template<typename T>
class ComponentStorage : public IComponentStorage {
public:
    T& Add(Entity entity, T component) {
        if (Has(entity)) {
            m_Data[entity] = std::move(component);
        } else {
            m_DenseToEntity.push_back(entity);
            m_Data[entity] = std::move(component);
        }
        return m_Data[entity];
    }

    void Remove(Entity entity) override {
        m_Data.erase(entity);
        auto it = std::find(m_DenseToEntity.begin(), m_DenseToEntity.end(), entity);
        if (it != m_DenseToEntity.end()) {
            m_DenseToEntity.erase(it);
        }
    }

    bool Has(Entity entity) const override {
        return m_Data.find(entity) != m_Data.end();
    }

    T& Get(Entity entity) {
        return m_Data.at(entity);
    }

    const T& Get(Entity entity) const {
        return m_Data.at(entity);
    }

    const std::vector<Entity>& GetEntities() const {
        return m_DenseToEntity;
    }

private:
    std::unordered_map<Entity, T> m_Data;
    std::vector<Entity> m_DenseToEntity;
};

class Registry {
public:
    Registry() = default;

    Entity CreateEntity(const std::string& name = "Entity") {
        Entity entity = ++m_NextEntityId;
        m_Entities.push_back(entity);
        AddComponent<TagComponent>(entity, TagComponent{ .tag = name });
        AddComponent<TransformComponent>(entity, TransformComponent{});
        AddComponent<HierarchyComponent>(entity, HierarchyComponent{});
        return entity;
    }

    void DestroyEntity(Entity entity) {
        auto it = std::find(m_Entities.begin(), m_Entities.end(), entity);
        if (it != m_Entities.end()) {
            m_Entities.erase(it);
            for (auto& [type, storage] : m_Storages) {
                storage->Remove(entity);
            }
        }
    }

    template<typename T, typename... Args>
    T& AddComponent(Entity entity, Args&&... args) {
        auto storage = GetOrCreateStorage<T>();
        return storage->Add(entity, T{ std::forward<Args>(args)... });
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        auto storage = GetStorage<T>();
        if (storage) {
            storage->Remove(entity);
        }
    }

    template<typename T>
    bool HasComponent(Entity entity) const {
        auto storage = GetStorage<T>();
        return storage && storage->Has(entity);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        auto storage = GetStorage<T>();
        if (!storage || !storage->Has(entity)) {
            throw std::runtime_error("Component not found on entity");
        }
        return storage->Get(entity);
    }

    template<typename T>
    const T& GetComponent(Entity entity) const {
        auto storage = GetStorage<T>();
        if (!storage || !storage->Has(entity)) {
            throw std::runtime_error("Component not found on entity");
        }
        return storage->Get(entity);
    }

    const std::vector<Entity>& GetEntities() const {
        return m_Entities;
    }

    template<typename... Components>
    std::vector<Entity> View() const {
        std::vector<Entity> result;
        for (Entity e : m_Entities) {
            if ((HasComponent<Components>(e) && ...)) {
                result.push_back(e);
            }
        }
        return result;
    }

private:
    template<typename T>
    std::shared_ptr<ComponentStorage<T>> GetStorage() const {
        auto typeIdx = std::type_index(typeid(T));
        auto it = m_Storages.find(typeIdx);
        if (it != m_Storages.end()) {
            return std::static_pointer_cast<ComponentStorage<T>>(it->second);
        }
        return nullptr;
    }

    template<typename T>
    std::shared_ptr<ComponentStorage<T>> GetOrCreateStorage() {
        auto typeIdx = std::type_index(typeid(T));
        auto it = m_Storages.find(typeIdx);
        if (it != m_Storages.end()) {
            return std::static_pointer_cast<ComponentStorage<T>>(it->second);
        }
        auto storage = std::make_shared<ComponentStorage<T>>();
        m_Storages[typeIdx] = storage;
        return storage;
    }

    Entity m_NextEntityId{ 0 };
    std::vector<Entity> m_Entities;
    std::unordered_map<std::type_index, std::shared_ptr<IComponentStorage>> m_Storages;
};

} // namespace NativeEngine::ECS
