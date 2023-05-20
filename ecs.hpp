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

#define ECS_ENTITY_DESTROYED \
    Entity { std::string::npos }

namespace ecs {

typedef char byte;

struct ComponentID {
    std::size_t hash = 0;
    struct ObjectPoolChunk* ptr = nullptr;
};

struct Entity {
    std::size_t id = 0;

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
    Entity entity;
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
    inline std::list<byte*>& get_blocks() { return m_blocks; }

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
        chunk->entity = ECS_ENTITY_DESTROYED;
        m_freed_locations.push_back(chunk);
    }

    void free(byte* ptr) {
        call_object_deconstructor(ptr);

        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(
            reinterpret_cast<byte*>(ptr) - sizeof(ObjectPoolChunk)
        );
        chunk->entity = ECS_ENTITY_DESTROYED;
        m_freed_locations.push_back(chunk);
    }

    void free(ObjectPoolChunk* chunk) {
        call_object_deconstructor(reinterpret_cast<byte*>(chunk) + sizeof(ObjectPoolChunk));

        chunk->entity = ECS_ENTITY_DESTROYED;
        m_freed_locations.push_back(chunk);
    }

    template<typename _T>
    _T* get_from_block_offset(std::size_t offset) {
        assert(
            offset < m_blocks.size() * m_block_size &&
            "ECS ASSERT: cannot access pool data outside block range"
        );


        return nullptr;
    }

    virtual void call_object_deconstructor(byte* target) = 0;

  private:
    template<typename _T, typename... _Args>
    _T* _construct(Entity entity, ObjectPoolChunk* chunk, _Args... args) {
        chunk->entity = entity;

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
            std::size_t id = m_destroyed_entities.back();

            Entity entity = Entity{id};
            m_entities[id] = entity;

            m_destroyed_entities.pop_back();

            return m_entities[entity];
        } else {
            m_entities.push_back(Entity{m_entities.size()});

            return m_entities.back();
        }
    }

    inline std::vector<Entity>& get_entities() { return m_entities; }
    inline const std::vector<Entity>& get_entities() const { return m_entities; }

    template<typename _T>
    ObjectPool* get_pool() {
        for (ObjectPool* pool : m_pools) {
            if (TypeInfo<_T>::get_hash() == pool->get_type_hash()) {
                return pool;
            }
        }

        return nullptr;
    }

    template<typename _T>
    const ObjectPool* get_pool() const {
        return get_pool<_T>();
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

        // TODO: Destroy all components ...

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
    std::vector<std::size_t> m_destroyed_entities = {};
    std::vector<ObjectPool*> m_pools = {};
};

template<typename _T, typename... _Ts>
class ViewIterator {
  public:
    ViewIterator(std::vector<Entity>::iterator iter) : m_iter(iter) {}
    ViewIterator(const ViewIterator& other) : m_iter(other.m_iter) {}

    bool operator==(const ViewIterator& other) { return m_iter == other.m_iter; }
    bool operator!=(const ViewIterator& other) { return m_iter != other.m_iter; }
    Entity& operator*() { return *m_iter; }
    Entity* operator->() { return &*m_iter; }

    ViewIterator& operator++() {
        m_iter++;
        return *this;
    }

    ViewIterator& operator--() {
        m_iter--;
        return *this;
    }

    ViewIterator operator++(int) {
        ViewIterator iter = *this;
        ++(*this);
        return iter;
    }

    ViewIterator operator--(int) {
        ViewIterator iter = *this;
        --(*this);
        return iter;
    }

  private:
    std::vector<Entity>::iterator m_iter;
};

#define ECS_VIEW_SET_TUPLE_INDEX(PTR, INDEX) m_reserved_from_valid.INDEX = PTR

template<typename _T, typename... _Ts>
class View {
  public:
    using Iterator = ViewIterator<_T, _Ts...>;

  public:
    View(Registry* registry) : m_registry(registry) {
        if constexpr (sizeof...(_Ts) > 0) {
            _count_types<_T, _Ts...>();
        } else {
            m_type_count = 1;
        }
    }

    inline Iterator begin() { return Iterator(m_registry->get_entities().begin()); }
    inline Iterator end() { return Iterator(m_registry->get_entities().end()); }

    std::tuple<_T*, _Ts*...> get() { return m_reserved_from_valid; }

    template<typename _Target>
    _Target* get() {
        return std::get<_Target*>(m_reserved_from_valid);
    }

    bool has_required(Entity entity) {
        m_reserved_from_valid = {};
        if constexpr (sizeof...(_Ts) > 0) {
            std::size_t found_types_count = 0;
            fill_result<_T, _Ts...>(entity, 0, found_types_count);

            if (found_types_count == m_type_count) {
                return true;
            }
        } else {
            ObjectPool* target_pool = m_registry->get_pool<_T>();
            if (target_pool != nullptr) {
                _T* target_ptr = target_pool->get_from_block_offset<_T>(entity);

                if (target_ptr != nullptr) {
                    std::get<0>(m_reserved_from_valid) = target_ptr;
                    return true;
                }
            }
        }

        return false;
    }

  private:
    template<typename _Head, typename... _Tail>
    void _count_types() {
        m_type_count++;

        if constexpr (sizeof...(_Tail) > 0) {
            _count_types<_Tail...>();
        }
    }

    template<typename _Head, typename... _Tail>
    void fill_result(Entity entity, std::size_t target_index, std::size_t& valid_count) {
        ObjectPool* target_pool = m_registry->get_pool<_Head>();
        if (target_pool != nullptr) {
            _Head* target_ptr = target_pool->get_from_block_offset<_Head>(entity);

            if (target_ptr != nullptr) {
                std::get<target_index>(m_reserved_from_valid) = target_ptr;

                valid_count++;

                if constexpr (sizeof...(_Tail) > 0) {
                    fill_result(entity, target_index + 1);
                }
            }
        }
    }

  private:
    Registry* m_registry = nullptr;
    std::size_t m_type_count = 0;
    std::tuple<_T*, _Ts*...> m_reserved_from_valid = {};
};

} // namespace ecs

#endif
