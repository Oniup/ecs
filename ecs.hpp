#ifndef __ECS_HPP__
#define __ECS_HPP__

#include <cassert>
#include <cinttypes>
#include <ctime>
#include <initializer_list>
#include <iostream>
#include <list>
#include <string>
#include <tuple>
#include <vector>

#define ECS_TYPE_CONTRADICTION_ASSERT(CONDITION, TYPE_NAME, OTHER_TYPE_NAME, FUNCTION_NAME) \
    if (!(CONDITION)) {                                                                     \
        std::string message = std::string("ECS ASSERT: cannot ") + FUNCTION_NAME + " " +    \
                              OTHER_TYPE_NAME + " in object pool for " + TYPE_NAME;         \
        assert(CONDITION&& message.c_str());                                                \
    }

#define ECS_REGISTRY_DEFAULT_POOL_BLOCK_SIZE 30

#define ECS_ENTITY_DESTROYED  \
    Entity {                  \
        std::string::npos, {} \
    }

namespace ecs {

typedef char byte;

struct ComponentID {
    std::size_t hash = 0;
    struct ObjectPoolChunk* ptr = nullptr;
};

struct Entity {
    std::size_t id = 0;
    std::vector<ComponentID> comps = {};

    inline operator std::size_t() { return id; }
};

template<typename _T>
struct TypeInfo {
    static inline const char* get_name() { return typeid(_T).name(); }
    static inline std::size_t get_size() { return static_cast<std::size_t>(sizeof(_T)); }
    static inline std::size_t get_hash() { return typeid(_T).hash_code(); }
};

struct ObjectPoolChunk {
    ObjectPoolChunk* next;
    ObjectPoolChunk* prev;
    Entity ent;
};

class ObjectPool {
  public:
    ObjectPool(const std::string& name, std::size_t size, std::size_t hash, std::size_t block_size)
        : m_type_name(name), m_type_size(size), m_type_hash(hash), m_block_size(block_size) {
        assert(m_type_name.size() > 0 && "ECS ASSERT: m_type_name must be larget than 0");
        assert(m_type_size > 0 && "ECS ASSERT: m_type_size must be larger than 0");
        assert(m_block_size > 0 && "ECS ASSERT: m_block_size must be larger than 0");
    }

    virtual ~ObjectPool() = default;

    inline const std::string& get_name() const { return m_type_name; }
    inline std::size_t get_type_size() const { return m_type_size; }
    inline std::size_t get_type_hash() const { return m_type_hash; }
    inline std::size_t get_block_size() const { return m_block_size; }
    inline const std::list<byte*>& get_blocks() const { return m_blocks; }

    template<typename _T, typename... _Args>
    _T* malloc(Entity entity, _Args... args) {
        ECS_TYPE_CONTRADICTION_ASSERT(
            TypeInfo<_T>::get_name() == m_type_name, m_type_name, TypeInfo<_T>::get_name(), "malloc"
        );

        if (m_freed_locations.size() > 0) {
            _T* object = _construct<_T>(entity, m_freed_locations.back(), args...);
            m_freed_locations.pop_back();

            return object;
        } else if (m_next == nullptr) {
            _allocate_block();
        }

        return _construct<_T>(entity, m_next, args...);
    }

    template<typename _T>
    void free(_T* type) {
        ECS_TYPE_CONTRADICTION_ASSERT(
            TypeInfo<_T>::get_name() == m_type_name, m_type_name, TypeInfo<_T>::get_name(), "free"
        );

        type->~_T();

        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(
            reinterpret_cast<byte*>(type) - sizeof(ObjectPoolChunk)
        );
        chunk->ent = ECS_ENTITY_DESTROYED;
        m_freed_locations.push_back(chunk);
    }

    void free(byte* ptr) {
        call_object_deconstructor(ptr);

        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(
            reinterpret_cast<byte*>(ptr) - sizeof(ObjectPoolChunk)
        );
        chunk->ent = ECS_ENTITY_DESTROYED;
        m_freed_locations.push_back(chunk);
    }

    virtual void call_object_deconstructor(byte* target) = 0;

  private:
    template<typename _T, typename... _Args>
    _T* _construct(Entity entity, ObjectPoolChunk* chunk, _Args... args) {
        entity.comps.push_back(ComponentID{m_type_hash, chunk});
        chunk->ent = entity;

        _T* object =
            reinterpret_cast<_T*>(reinterpret_cast<byte*>(chunk) + sizeof(ObjectPoolChunk));

        new (object) _T(args...);
        m_next = m_next->next;

        return object;
    }

    void _allocate_block() {
        const std::size_t chunk_size = sizeof(ObjectPoolChunk) + m_type_size;

        byte* block = static_cast<byte*>(std::malloc(sizeof(byte*) * chunk_size * m_block_size));
        m_blocks.push_back(block);
        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(block);
        chunk->next = reinterpret_cast<ObjectPoolChunk*>(block + chunk_size);
        chunk->prev = m_next;

        for (std::size_t i = 1; i < m_block_size; i++) {
            ObjectPoolChunk* current = reinterpret_cast<ObjectPoolChunk*>(block + (chunk_size * i));
            current->next = reinterpret_cast<ObjectPoolChunk*>(block + (chunk_size * (i + 1)));
            current->prev = chunk;
            chunk = current;
        }

        chunk->next = nullptr;
        if (m_next != nullptr) {
            m_next->next = reinterpret_cast<ObjectPoolChunk*>(block);
            m_next = m_next->next;
        } else {
            m_next = reinterpret_cast<ObjectPoolChunk*>(block);
        }
    }

  protected:
    const std::string m_type_name;
    const std::size_t m_type_size;
    const std::size_t m_type_hash;
    const std::size_t m_block_size;
    std::list<byte*> m_blocks = {};
    ObjectPoolChunk* m_next = nullptr;
    std::vector<ObjectPoolChunk*> m_freed_locations = {};
};

template<typename _T>
class CreateObjectPool : public ObjectPool {
  public:
    CreateObjectPool(std::size_t block_size)
        : ObjectPool(
              TypeInfo<_T>::get_name(), TypeInfo<_T>::get_size(), TypeInfo<_T>::get_hash(),
              block_size
          ) {}

    virtual ~CreateObjectPool() override {
        for (std::list<byte*>::iterator it = m_blocks.begin(); it != m_blocks.end(); it++) {
            for (std::size_t i = 0; i < m_block_size; i++) {
                _T* target =
                    reinterpret_cast<_T*>(*it + (sizeof(ObjectPoolChunk) + sizeof(_T) * i));
                target->~_T();
            }

            std::free(*it);
        }
    }

    virtual void call_object_deconstructor(byte* target) override {
        _T* target_type = reinterpret_cast<_T*>(target);
        target_type->~_T();
    }
};

class Registry {
  public:
    Registry() {}

    ~Registry() {
        for (ObjectPool* pool : m_pools) {
            delete pool;
        }
    }

    Entity create_entity() {
        // PERF: Improve the destroyed entities system so that it is not poping back
        if (m_destroyed_entities.size() > 0) {
            Entity ent = m_destroyed_entities.back();
            m_entities[ent] = ent;
            m_destroyed_entities.pop_back();

            return m_entities[ent];
        } else {
            m_entities.push_back(Entity{m_entities.size()});

            return m_entities.back();
        }
    }

    void destroy_Entity(Entity entity) {
        assert(
            entity != ECS_ENTITY_DESTROYED && "ECS ASSERT (destroy_entity(entity)): entity "
                                              "provided id is set to ECS_Entity_DESTRSOYED"
        );
        assert(
            entity < m_entities.size() && "ECS ASSERT (destroy_entity(entity)): entity provided id "
                                          "is greater than m_entities.size()"
        );

        m_destroyed_entities.push_back(entity);
        m_entities[entity] = ECS_ENTITY_DESTROYED;
    }

    template<typename _T, typename... _Args>
    _T* create_component(Entity entity, _Args... args) {
        assert(
            entity != ECS_ENTITY_DESTROYED && "ECS ASSERT (create_component(entity, ...)): cannot "
                                              "create component onto destroyed entity"
        );

        ObjectPool* target = nullptr;
        for (ObjectPool* pool : m_pools) {
            if (pool->get_type_hash() == TypeInfo<_T>::get_hash()) {
                target = pool;
                break;
            }
        }

        if (target == nullptr) {
            target = new CreateObjectPool<_T>(ECS_REGISTRY_DEFAULT_POOL_BLOCK_SIZE);
            m_pools.push_back(target);
        }

        return target->malloc<_T>(entity, args...);
    }

  private:
    std::vector<Entity> m_entities = {};
    std::vector<Entity> m_destroyed_entities = {};
    std::vector<ObjectPool*> m_pools = {};
};

} // namespace ecs

#endif
