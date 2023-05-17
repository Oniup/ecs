#ifndef __ECS_HPP__
#define __ECS_HPP__

#include <cassert>
#include <cinttypes>
#include <cwchar>
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

namespace ecs {

typedef char byte;

template<typename _T>
struct TypeInfo {
    static inline const char* get_name() { return typeid(_T).name(); }
    static inline std::size_t get_size() { return static_cast<std::size_t>(sizeof(_T)); }
};

struct ObjectPoolChunk {
    ObjectPoolChunk* next;
    ObjectPoolChunk* prev;
};

class ObjectPool {
  public:
    ObjectPool(const std::string& name, std::size_t type_size, std::size_t block_size)
        : m_type_name(name), m_type_size(type_size), m_block_size(block_size) {
        assert(m_type_name.size() > 0 && "ECS ASSERT: m_type_name must be larget than 0");
        assert(m_type_size > 0 && "ECS ASSERT: m_type_size must be larger than 0");
        assert(m_block_size > 0 && "ECS ASSERT: m_block_size must be larger than 0");
    }

    virtual ~ObjectPool() = default;

    inline const std::string& get_name() const { return m_type_name; }
    inline std::size_t get_type_size() const { return m_type_size; }
    inline std::size_t get_block_size() const { return m_block_size; }
    inline const std::list<byte*>& get_blocks() const { return m_blocks; }

    template<typename _T, typename... _Args>
    _T* malloc(_Args... args) {
        ECS_TYPE_CONTRADICTION_ASSERT(
            TypeInfo<_T>::get_name() == m_type_name, m_type_name, TypeInfo<_T>::get_name(), "malloc"
        );

        if (m_freed_locations.size() > 0) {
            _T* object = _construct<_T>(m_freed_locations.back(), args...);
            m_freed_locations.pop_back();

            return object;
        } else if (m_next == nullptr) {
            _allocate_block();
        }

        return _construct<_T>(reinterpret_cast<byte*>(m_next + sizeof(ObjectPoolChunk)), args...);
    }

    template<typename _T>
    void free(_T* type) {
        ECS_TYPE_CONTRADICTION_ASSERT(
            TypeInfo<_T>::get_name() == m_type_name, m_type_name, TypeInfo<_T>::get_name(), "free"
        );

        type->~_T();
        m_freed_locations.push_back(reinterpret_cast<byte*>(type));
    }

    template<typename _T>
    _T* cast_into(ObjectPoolChunk& ref) {
        ECS_TYPE_CONTRADICTION_ASSERT(
            TypeInfo<_T>::get_name() == m_type_name, m_type_name, TypeInfo<_T>::get_name(),
            "cast_into"
        );

        byte* byte_data = reinterpret_cast<byte*>(&ref) + sizeof(ObjectPoolChunk);
        return reinterpret_cast<_T*>(byte_data);
    }

    virtual void call_object_deconstructor(byte* target) = 0;

  private:
    template<typename _T, typename... _Args>
    _T* _construct(byte* location, _Args... args) {
        _T* object = reinterpret_cast<_T*>(location);
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
    const std::size_t m_block_size;
    std::list<byte*> m_blocks = {};
    ObjectPoolChunk* m_next = nullptr;
    std::vector<byte*> m_freed_locations = {};
};

template<typename _T>
class CreateObjectPool : public ObjectPool {
  public:
    CreateObjectPool(std::size_t block_size)
        : ObjectPool(TypeInfo<_T>::get_name(), TypeInfo<_T>::get_size(), block_size) {}

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

} // namespace ecs

#endif
