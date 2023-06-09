/**
 * @file ecs.hpp
 * @copyright Copyright (c) 2023-present Ewan Robson.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __ECS_HPP__
#define __ECS_HPP__

#include <array>
#include <cassert>
#include <cinttypes>
#include <ctime>
#include <initializer_list>
#include <iostream>
#include <list>
#include <string>
#include <tuple>
#include <vector>

#define ECS_TYPE_CONTRADICTION_ASSERT(CONDITION, TYPE_NAME, OTHER_TYPE_NAME, FUNCTION_NAME)       \
    if (!(CONDITION))                                                                             \
    {                                                                                             \
        std::cout << std::string("ECS ASSERT: cannot ") + FUNCTION_NAME + " " + OTHER_TYPE_NAME + \
                         " in object pool for " + TYPE_NAME;                                      \
        std::exit(-1);                                                                            \
    }

#define ECS_REGISTRY_DEFAULT_POOL_BLOCK_SIZE 30

#define ECS_ENTITY_DESTROYED \
    ecs::Entity { std::string::npos }

namespace ecs {

namespace type_descriptor {

    /**
     * @brief Get the wrapped function name with the type name. Example using clang:
     *
     * constexpr const char* ecs::type_descriptor::get_wrapped_type_name() [with _T = double]
     *
     * @tparam _T Target type
     * @return const char* Example using clang is above
     */
    template<typename _T>
    constexpr const char* get_wrapped_typename()
    {
#if defined(__clang__)
        return __PRETTY_FUNCTION__;
#elif defined(__GNUC__) || defined(__GNUG__)
        return __PRETTY_FUNCTION__;
#elif defind(_MSC_VER)
        return _SIGFUNC;
#else
#    error "Unsuppored compiler ecs system"
#endif
    }

    /**
     * @brief Gets the get_wrapped_type_name<_T>() result and returns the length before the type
     * name appears in the string
     *
     * @return std::size_t Prefix length
     */
    constexpr std::size_t get_wrapped_typename_prefix_length()
    {
        constexpr const char* wrapped_name = get_wrapped_typename<void>();
        constexpr std::size_t wrapped_length = std::char_traits<char>::length(wrapped_name);

        std::size_t offset = 0;
        for (; offset < wrapped_length - 4; offset++)
        {
            if (std::char_traits<char>::compare(wrapped_name + offset, "void", 4) == 0)
                break;
        }
        return offset;
    }

    /**
     * @brief Get the get_wrapped_type_name() result and returns the length after the type name
     * appears in the string
     *
     * @return std::size_t Suffix length
     */
    constexpr std::size_t get_wrapped_typename_suffix_length()
    {
        return std::char_traits<char>::length(get_wrapped_typename<void>()) -
               get_wrapped_typename_prefix_length() - 4;
    }

    /**
     * @brief Checks through the given name for ' ' and ':' indercating compiler differences with
     * the get_wrapped_type_name() function macros used. This is done to remove this bloat in the
     * final result such as namespaces and keywords like struct, enum, class, etc
     *
     * @param typename_str type name as string
     * @param length length for the typename
     * @return returns the length of bloat
     */
    constexpr std::size_t
        get_typename_prefix_bloat_length(const char* typename_str, std::size_t length)
    {
        std::size_t offset = std::string::npos;
        for (std::size_t i = 0; i < length; i++)
        {
            if (typename_str[i] == '<')
                break;

            if (typename_str[i] == ' ' || typename_str[i] == ':')
            {
                // means there is an array and we want that
                if (i + 1 < length && typename_str[i + 1] == '[')
                    break;

                offset = i;
            }
        }

        if (offset != std::string::npos)
            offset += 1;
        return offset;
    }

    /**
     * @brief Calculates the name of the type
     *
     * @tparam _T
     * @return constexpr std::array<char, name_length> Name of the type
     */
    template<typename _T>
    constexpr auto get_name()
    {
        constexpr const char* wrapped_type_name = get_wrapped_typename<_T>();
        constexpr std::size_t wrapped_type_length =
            std::char_traits<char>::length(wrapped_type_name);

        constexpr std::size_t prefix_length = get_wrapped_typename_prefix_length();
        constexpr std::size_t suffix_lenght = get_wrapped_typename_suffix_length();
        constexpr std::size_t substr_length = wrapped_type_length - prefix_length - suffix_lenght;

        constexpr std::size_t bloat_offset =
            get_typename_prefix_bloat_length(wrapped_type_name + prefix_length, substr_length);

        constexpr bool remove_bloat = bloat_offset != std::string::npos;
        std::array<char, remove_bloat ? substr_length - bloat_offset : substr_length> substr;

        constexpr const char* offset_wrapped_type_name =
            remove_bloat ? wrapped_type_name + prefix_length + bloat_offset
                         : wrapped_type_name + prefix_length;

        for (std::size_t i = 0; i < substr.size(); i++)
            substr[i] = offset_wrapped_type_name[i];

        substr[substr.size()] = '\0';
        return substr;
    }

    /**
     * @brief Calculats Fowler–Noll–Vo hash function (FNV) for more information:
     * https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
     *
     * @param str Every hash value generated by this exact string will be the same
     * @return std::uint64_t Unique id based on the String given
     */
    template<std::size_t _Size>
    constexpr std::uint64_t get_hash(const std::array<char, _Size>& str)
    {
        constexpr std::uint64_t prime = 0x00000100000001B3;

        std::uint64_t hash = 0xcbf29ce484222325;
        for (char c : str)
        {
            hash *= prime;
            hash ^= static_cast<std::uint64_t>(c);
        }

        return hash;
    }

} // namespace type_descriptor

/**
 * @class Entity
 * @brief Unique Id used to identify groups of components that belong to the same entity
 */
struct Entity
{
    std::size_t id = 0;

    inline operator std::size_t() { return id; }
    inline bool operator==(const Entity& other) const { return id == other.id; }
    inline bool operator!=(const Entity& other) const { return id != other.id; }
};

/**
 * @class ObjectPoolChunk
 * @brief Defines what object this entity is defined, its previous and the next object pool chunk.
 */
struct ObjectPoolChunk
{
    ObjectPoolChunk* next;
    ObjectPoolChunk* prev;
    Entity entity;
};

typedef void (*fnptr_objectpool_type_deconstructor)(std::byte* type);
typedef void (*fnptr_objectpool_type_default_constructor)(std::byte* type);

/**
 * @class ObjectPool
 * @brief
 *
 */
class ObjectPool
{
  public:
    template<typename _T>
    static ObjectPool* create(std::size_t block_size)
    {
        ObjectPool* pool = new ObjectPool(
            std::string(
                type_descriptor::get_name<_T>().data(), type_descriptor::get_name<_T>().size()
            ),
            sizeof(_T), type_descriptor::get_hash(type_descriptor::get_name<_T>()), block_size
        );

        pool->m_type_default_constructor = [](std::byte* target)
        {
            _T* target_type = reinterpret_cast<_T*>(target);
            new (target_type) _T();
        };

        pool->m_type_deconstructor = [](std::byte* target)
        {
            _T* target_type = reinterpret_cast<_T*>(target);
            target_type->~_T();
        };

        return pool;
    }

  public:
    ObjectPool() = default;

    ObjectPool(
        const std::string& name, std::size_t size, std::uint64_t hash, std::size_t block_size
    )
        : m_type_name(name), m_type_size(size), m_type_hash(hash), m_block_size(block_size)
    {
        assert(m_type_name.size() > 0 && "ECS ASSERT: m_type_name must be larget than 0");
        assert(m_type_size > 0 && "ECS ASSERT: m_type_size must be larger than 0");
        assert(m_block_size > 0 && "ECS ASSERT: m_block_size must be larger than 0");
    }

    ~ObjectPool()
    {
        for (std::list<std::byte*>::iterator it = m_blocks.begin(); it != m_blocks.end(); it++)
        {
            for (std::size_t i = 0; i < m_block_size; i++)
            {
                ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(
                    *it + (sizeof(ObjectPoolChunk) + m_type_size) * i
                );
                if (chunk->entity != ECS_ENTITY_DESTROYED)
                {
                    if (m_type_deconstructor != nullptr)
                    {
                        std::byte* target = reinterpret_cast<std::byte*>(
                            *it +
                            ((sizeof(ObjectPoolChunk) + m_type_size) * i + sizeof(ObjectPoolChunk))
                        );
                        m_type_deconstructor(target);
                    }
                }
            }

            std::free(*it);
        }
    }

    inline void set_default_constructor(fnptr_objectpool_type_default_constructor constructor)
    {
        m_type_default_constructor = constructor;
    }

    inline void set_deconstructor(fnptr_objectpool_type_deconstructor deconstructor)
    {
        m_type_deconstructor = deconstructor;
    }

    inline const std::string& get_name() const { return m_type_name; }
    inline std::size_t get_type_size() const { return m_type_size; }
    inline std::uint64_t get_type_hash() const { return m_type_hash; }
    inline std::size_t get_block_size() const { return m_block_size; }

    inline const std::vector<ObjectPoolChunk*> get_free_locations() const
    {
        return m_freed_locations;
    }

    inline const std::list<std::byte*>& get_blocks() const { return m_blocks; }
    inline std::list<std::byte*>& get_blocks() { return m_blocks; }

    template<typename _T, typename... _Args>
    _T* malloc(Entity entity, _Args... args)
    {
        ECS_TYPE_CONTRADICTION_ASSERT(
            std::string(
                type_descriptor::get_name<_T>().data(), type_descriptor::get_name<_T>().size()
            ) == m_type_name,
            m_type_name, type_descriptor::get_name<_T>().data(), "malloc"
        );

        if (m_freed_locations.size() > 0)
        {
            _T* object = _construct<_T>(entity, m_freed_locations.back(), args...);
            m_freed_locations.pop_back();

            return object;
        }
        else if (m_next == nullptr)
            _allocate_block();

        return _construct<_T>(entity, m_next, args...);
    }

    std::byte* malloc(Entity entity)
    {
        if (m_freed_locations.size() > 0)
        {
            std::byte* object = _construct(entity, m_freed_locations.back());
            m_freed_locations.pop_back();

            return object;
        }
        else if (m_next == nullptr)
            _allocate_block();

        return _construct(entity, m_next);
    }

    template<typename _T>
    void free(_T* type)
    {
        ECS_TYPE_CONTRADICTION_ASSERT(
            std::string(
                type_descriptor::get_name<_T>().data(), type_descriptor::get_name<_T>().size()
            ) == m_type_name,
            m_type_name, type_descriptor::get_name<_T>().data(), "free"
        );

        type->~_T();

        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(
            reinterpret_cast<std::byte*>(type) - sizeof(ObjectPoolChunk)
        );
        chunk->entity = ECS_ENTITY_DESTROYED;
        m_freed_locations.push_back(chunk);
    }

    void free(std::byte* ptr)
    {
        m_type_deconstructor(ptr);

        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(
            reinterpret_cast<std::byte*>(ptr) - sizeof(ObjectPoolChunk)
        );
        chunk->entity = ECS_ENTITY_DESTROYED;
        m_freed_locations.push_back(chunk);
    }

    void free(ObjectPoolChunk* chunk)
    {
        m_type_deconstructor(reinterpret_cast<std::byte*>(chunk) + sizeof(ObjectPoolChunk));

        chunk->entity = ECS_ENTITY_DESTROYED;
        m_freed_locations.push_back(chunk);
    }

    // PERF: Improve so it isn't doing a linear search
    template<typename _T>
    _T* get_entitys_object(Entity entity)
    {
        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(m_blocks.front());

        while (chunk != nullptr)
        {
            if (chunk->entity == entity)
            {
                std::byte* byte_data = reinterpret_cast<std::byte*>(chunk);
                return reinterpret_cast<_T*>(byte_data + sizeof(ObjectPoolChunk));
            }

            chunk = chunk->next;
        }

        return nullptr;
    }

    std::byte* get_entitys_object(Entity entity)
    {
        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(m_blocks.front());

        while (chunk != nullptr)
        {
            if (chunk->entity == entity)
            {
                std::byte* byte_data = reinterpret_cast<std::byte*>(chunk);
                return byte_data + sizeof(ObjectPoolChunk);
            }

            chunk = chunk->next;
        }

        return nullptr;
    }

    // PERF: Improve so it isn't doing a linear search
    ObjectPoolChunk* get_entitys_object_pool_chunk(Entity entity)
    {
        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(m_blocks.front());

        while (chunk != nullptr)
        {
            if (chunk->entity == entity)
                return reinterpret_cast<ObjectPoolChunk*>(chunk);

            chunk = chunk->next;
        }

        return nullptr;
    }

  private:
    template<typename _T, typename... _Args>
    _T* _construct(Entity entity, ObjectPoolChunk* chunk, _Args... args)
    {
        chunk->entity = entity;

        _T* object =
            reinterpret_cast<_T*>(reinterpret_cast<std::byte*>(chunk) + sizeof(ObjectPoolChunk));

        new (object) _T(args...);
        m_next = m_next->next;

        return object;
    }

    std::byte* _construct(Entity entity, ObjectPoolChunk* chunk)
    {
        chunk->entity = entity;

        std::byte* object = reinterpret_cast<std::byte*>(chunk) + sizeof(ObjectPoolChunk);
        m_type_default_constructor(object);
        m_next = m_next->next;

        return object;
    }

    void _allocate_block()
    {
        const std::size_t chunk_size = sizeof(ObjectPoolChunk) + m_type_size;

        std::byte* block =
            static_cast<std::byte*>(std::malloc(sizeof(std::byte) * chunk_size * m_block_size));
        m_blocks.push_back(block);
        ObjectPoolChunk* chunk = reinterpret_cast<ObjectPoolChunk*>(block);
        chunk->next = reinterpret_cast<ObjectPoolChunk*>(block + chunk_size);
        chunk->entity = ECS_ENTITY_DESTROYED;
        chunk->prev = m_next;

        for (std::size_t i = 1; i < m_block_size; i++)
        {
            ObjectPoolChunk* current = reinterpret_cast<ObjectPoolChunk*>(block + (chunk_size * i));
            current->next = reinterpret_cast<ObjectPoolChunk*>(block + (chunk_size * (i + 1)));
            current->prev = chunk;
            current->entity = ECS_ENTITY_DESTROYED;
            chunk = current;
        }

        chunk->next = nullptr;
        if (m_next != nullptr)
        {
            m_next->next = reinterpret_cast<ObjectPoolChunk*>(block);
            m_next = m_next->next;
        }
        else
            m_next = reinterpret_cast<ObjectPoolChunk*>(block);
    }

  protected:
    const std::string m_type_name = "";
    const std::size_t m_type_size = 0;
    const std::uint64_t m_type_hash = 0;
    const std::size_t m_block_size = 0;
    std::list<std::byte*> m_blocks = {};
    ObjectPoolChunk* m_next = nullptr;
    std::vector<ObjectPoolChunk*> m_freed_locations = {};
    fnptr_objectpool_type_default_constructor m_type_default_constructor = nullptr;
    fnptr_objectpool_type_deconstructor m_type_deconstructor = nullptr;
};

class Registry
{
  public:
    Registry() {}

    ~Registry()
    {
        for (ObjectPool* pool : m_pools)
            delete pool;
    }

    Entity create_entity()
    {
        // PERF: Improve the destroyed entities system so that it is not poping back
        if (m_destroyed_entities.size() > 0)
        {
            std::size_t id = m_destroyed_entities.back();

            Entity entity = Entity{id};
            m_entities[id] = entity;

            m_destroyed_entities.pop_back();

            return m_entities[entity];
        }
        else
        {
            m_entities.push_back(Entity{m_entities.size()});

            return m_entities.back();
        }
    }

    inline std::vector<Entity>& get_entities() { return m_entities; }
    inline std::vector<ObjectPool*>& get_pools() { return m_pools; }
    inline const std::vector<Entity>& get_entities() const { return m_entities; }
    inline const std::vector<ObjectPool*>& get_pools() const { return m_pools; }

    template<typename _T>
    inline ObjectPool* get_pool()
    {
        return get_pool(type_descriptor::get_hash(type_descriptor::get_name<_T>()));
    }

    template<typename _T>
    inline const ObjectPool* get_pool() const
    {
        return get_pool(type_descriptor::get_hash(type_descriptor::get_name<_T>()));
    }

    inline const ObjectPool* get_pool(std::uint64_t hash) const { return get_pool(hash); }

    ObjectPool* get_pool(std::uint64_t hash)
    {
        for (ObjectPool* pool : m_pools)
        {
            if (pool->get_type_hash() == hash)
                return pool;
        }

        return nullptr;
    }

    template<typename _T>
    _T* get_component(Entity entity)
    {
        ObjectPool* pool = get_pool<_T>();
        if (pool != nullptr)
            return pool->get_entitys_object<_T>(entity);

        return nullptr;
    }

    void* get_component(Entity entity, std::uint64_t hash)
    {
        ObjectPool* pool = get_pool(hash);
        if (pool != nullptr)
        {
            ObjectPoolChunk* chunk = pool->get_entitys_object_pool_chunk(entity);
            if (chunk != nullptr)
            {
                std::byte* byte_data = reinterpret_cast<std::byte*>(chunk);
                return reinterpret_cast<void*>(byte_data + sizeof(ObjectPoolChunk));
            }
        }
        return nullptr;
    }

    const void* get_component(Entity entity, std::uint64_t hash) const
    {
        return get_component(entity, hash);
    }

    void destroy_entity(Entity entity)
    {
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

        for (ObjectPool* pool : m_pools)
        {
            // PERF: Improve so it isn't doing a linear search
            ObjectPoolChunk* chunk = pool->get_entitys_object_pool_chunk(entity);

            if (chunk != nullptr)
            {
                if (chunk->entity == entity)
                    pool->free(chunk);
            }
        }
    }

    template<typename _T, typename... _Args>
    _T* create_component(Entity entity, _Args... args)
    {
        assert(
            entity != ECS_ENTITY_DESTROYED && "ECS ASSERT (create_component(entity, ...)): cannot "
                                              "create component onto destroyed entity"
        );

        ObjectPool* target = get_pool<_T>();
        if (target == nullptr)
        {
            target = ObjectPool::create<_T>(ECS_REGISTRY_DEFAULT_POOL_BLOCK_SIZE);
            m_pools.push_back(target);
        }

        return target->malloc<_T>(entity, args...);
    }

    std::byte* create_component(
        Entity entity, std::uint64_t id, const std::string& name, std::size_t size,
        fnptr_objectpool_type_deconstructor deconstructor,
        fnptr_objectpool_type_default_constructor default_constsructor,
        std::size_t block_size = ECS_REGISTRY_DEFAULT_POOL_BLOCK_SIZE
    )
    {
        assert(
            entity != ECS_ENTITY_DESTROYED && "ECS ASSERT (create_component(entity, ...)): cannot "
                                              "create component onto destroyed entity"
        );

        ObjectPool* target = get_pool(id);
        if (target == nullptr)
        {
            target = new ObjectPool(name, size, id, block_size);
            target->set_deconstructor(deconstructor);
            target->set_default_constructor(default_constsructor);
            m_pools.push_back(target);
        }

        return target->malloc(entity);
    }

  private:
    std::vector<Entity> m_entities = {};
    std::vector<std::size_t> m_destroyed_entities = {};
    std::vector<ObjectPool*> m_pools = {};
};

template<typename _T, typename... _Ts>
class ViewIterator
{
  public:
    ViewIterator(std::vector<Entity>::iterator iter) : m_iter(iter) {}
    ViewIterator(const ViewIterator& other) : m_iter(other.m_iter) {}

    bool operator==(const ViewIterator& other) { return m_iter == other.m_iter; }
    bool operator!=(const ViewIterator& other) { return m_iter != other.m_iter; }
    Entity& operator*() { return *m_iter; }
    Entity* operator->() { return &*m_iter; }

    ViewIterator& operator++()
    {
        m_iter++;
        return *this;
    }

    ViewIterator& operator--()
    {
        m_iter--;
        return *this;
    }

    ViewIterator operator++(int)
    {
        ViewIterator iter = *this;
        ++(*this);
        return iter;
    }

    ViewIterator operator--(int)
    {
        ViewIterator iter = *this;
        --(*this);
        return iter;
    }

  private:
    std::vector<Entity>::iterator m_iter;
};

template<typename _T, typename... _Ts>
class View
{
  public:
    using Iterator = ViewIterator<_T, _Ts...>;

  public:
    View(Registry* registry) : m_registry(registry)
    {
        if constexpr (sizeof...(_Ts) > 0)
            _count_types<_T, _Ts...>();
        else
            m_type_count = 1;
    }

    ~View() = default;

    inline Iterator begin() { return Iterator(m_registry->get_entities().begin()); }
    inline Iterator end() { return Iterator(m_registry->get_entities().end()); }

    std::tuple<_T*, _Ts*...> get() { return m_reserved_from_valid; }

    template<typename _Target>
    _Target* get()
    {
        return std::get<_Target*>(m_reserved_from_valid);
    }

    bool has_required(Entity entity)
    {
        if (entity == ECS_ENTITY_DESTROYED)
            return false;

        m_reserved_from_valid = {};
        if constexpr (sizeof...(_Ts) > 0)
        {
            std::size_t found_types_count = 0;
            fill_result<_T, _Ts...>(entity, 0, found_types_count);

            if (found_types_count == m_type_count)
                return true;
        }
        else
        {
            // PERF: Improve so it is not linearly searching for components from the beginning of
            // the pool for every entity
            ObjectPool* target_pool = m_registry->get_pool<_T>();
            if (target_pool != nullptr)
            {
                _T* target_ptr = target_pool->get_entitys_object<_T>(entity);
                if (target_ptr != nullptr)
                {
                    std::get<0>(m_reserved_from_valid) = target_ptr;
                    return true;
                }
            }
        }

        return false;
    }

  private:
    template<typename _Head, typename... _Tail>
    void _count_types()
    {
        m_type_count++;

        if constexpr (sizeof...(_Tail) > 0)
            _count_types<_Tail...>();
    }

    template<typename _Head, typename... _Tail>
    void fill_result(Entity entity, std::size_t target_index, std::size_t& valid_count)
    {
        ObjectPool* target_pool = m_registry->get_pool<_Head>();
        if (target_pool != nullptr)
        {
            _Head* target_ptr = target_pool->get_entitys_object<_Head>(entity);
            if (target_ptr != nullptr)
            {
                std::get<_Head*>(m_reserved_from_valid) = target_ptr;
                valid_count++;

                if constexpr (sizeof...(_Tail) > 0)
                    fill_result<_Tail...>(entity, target_index + 1, valid_count);
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
