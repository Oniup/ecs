#include "../ecs.hpp"
#include <gtest/gtest.h>

struct Vector3
{
    float x = 0;
    float y = 0;
    float z = 0;

    Vector3() = default;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vector3(const Vector3& other) : x(other.x), y(other.y), z(other.z) {}

    bool operator==(const Vector3& other) { return x == other.x && y == other.y && z == other.z; }
    bool operator!=(const Vector3& other) { return !(*this == other); }
};

struct TransformComponent
{
    Vector3 position = {};
    Vector3 scale = {};
    Vector3 rotation = {};

    TransformComponent() = default;

    TransformComponent(
        const Vector3& position, const Vector3& scale = {}, const Vector3& rotation = {}
    )
        : position(position), scale(scale), rotation(rotation)
    {
    }
};

struct NameComponent
{
    std::string name = "";

    NameComponent(const std::string& name) : name(name) {}
};

TEST(Registry, create_entity)
{
    ecs::Registry registry = ecs::Registry();
    ecs::Entity entity = registry.create_entity();

    EXPECT_EQ(static_cast<std::size_t>(entity), 0);

    SUCCEED();
}

TEST(Registry, get_target_pool)
{
    ecs::Registry registry = ecs::Registry();
    ecs::Entity entity = registry.create_entity();
    registry.create_component<TransformComponent>(entity);

    EXPECT_TRUE(registry.get_pool<TransformComponent>() != nullptr);
    EXPECT_FALSE(registry.get_pool<NameComponent>() != nullptr);
}

TEST(Registry, destroy_entity)
{
    ecs::Registry registry = ecs::Registry();
    ecs::Entity target;

    for (std::size_t i = 0; i < 10; i++)
    {
        ecs::Entity entity = registry.create_entity();
        if (i == 7)
            target = entity;
    }

    EXPECT_TRUE(target == ecs::Entity{7});
    registry.destroy_entity(target);

    std::vector<ecs::Entity>& entities = registry.get_entities();
    for (std::size_t i = 0; i < entities.size(); i++)
    {
        if (i == 7)
            EXPECT_EQ(static_cast<std::size_t>(entities[i]), std::string::npos);
        else
            EXPECT_EQ(static_cast<std::size_t>(entities[i]), i);
    }
}

TEST(Registry, destroy_entity_with_components)
{
    ecs::Registry registry = ecs::Registry();
    ecs::Entity target;

    for (std::size_t i = 0; i < 10; i++)
    {
        ecs::Entity entity = registry.create_entity();
        registry.create_component<TransformComponent>(entity, Vector3(i, i + 1, i + 2));
        if (i == 7)
        {
            target = entity;
        }
    }

    EXPECT_TRUE(target == ecs::Entity{7});
    registry.destroy_entity(target);

    std::vector<ecs::Entity>& entities = registry.get_entities();
    for (std::size_t i = 0; i < entities.size(); i++)
    {
        if (i == 7)
            EXPECT_EQ(static_cast<std::size_t>(entities[i]), std::string::npos);
        else
            EXPECT_EQ(static_cast<std::size_t>(entities[i]), i);
    }

    ecs::ObjectPool* pool = registry.get_pool<TransformComponent>();
    ecs::byte* byte_data = pool->get_blocks().front() +
                           (sizeof(ecs::ObjectPoolChunk) + sizeof(TransformComponent)) * 7;
    ecs::ObjectPoolChunk* chunk = reinterpret_cast<ecs::ObjectPoolChunk*>(byte_data);

    EXPECT_TRUE(chunk->entity == ECS_ENTITY_DESTROYED);
}

TEST(Registry, destroy_entity_with_components_and_deconstructor)
{
    ecs::Registry registry = ecs::Registry();
    ecs::Entity target;

    for (std::size_t i = 0; i < 10; i++)
    {
        ecs::Entity entity = registry.create_entity();
        registry.create_component<NameComponent>(entity, "entity_" + std::to_string(i));
        if (i == 7)
            target = entity;
    }

    EXPECT_TRUE(target == ecs::Entity{7});
    registry.destroy_entity(target);

    std::vector<ecs::Entity>& entities = registry.get_entities();
    for (std::size_t i = 0; i < entities.size(); i++)
    {
        if (i == 7)
            EXPECT_EQ(static_cast<std::size_t>(entities[i]), std::string::npos);
        else
            EXPECT_EQ(static_cast<std::size_t>(entities[i]), i);
    }

    ecs::ObjectPool* pool = registry.get_pool<NameComponent>();
    ecs::byte* byte_data =
        pool->get_blocks().front() + (sizeof(ecs::ObjectPoolChunk) + sizeof(NameComponent)) * 7;
    ecs::ObjectPoolChunk* chunk = reinterpret_cast<ecs::ObjectPoolChunk*>(byte_data);

    EXPECT_TRUE(chunk->entity == ECS_ENTITY_DESTROYED);
}

TEST(Registry, create_1_component)
{
    ecs::Registry registry = ecs::Registry();
    ecs::Entity entity = registry.create_entity();

    TransformComponent* transform =
        registry.create_component<TransformComponent>(entity, Vector3(1.0f, 2.0f, 3.0f));

    EXPECT_TRUE(transform != nullptr);
    EXPECT_TRUE(transform->position == Vector3(1.0f, 2.0f, 3.0f));

    SUCCEED();
}

TEST(View, has_required_for_1_component)
{
    ecs::Registry registry = ecs::Registry();
    for (std::size_t i = 0; i < 10; i++)
    {
        ecs::Entity entity = registry.create_entity();
        registry.create_component<TransformComponent>(entity, Vector3(i, i, i));
    }

    auto view = ecs::View<TransformComponent>(&registry);
    std::size_t count = 0;
    for (ecs::Entity entity : view)
    {
        bool result = view.has_required(entity);
        EXPECT_TRUE(result);
        if (result)
            count++;
    }

    EXPECT_EQ(count, 10);
}

TEST(View, has_required_for_multiple_components)
{
    ecs::Registry registry = ecs::Registry();
    for (std::size_t i = 0; i < 10; i++)
    {
        ecs::Entity entity = registry.create_entity();
        registry.create_component<TransformComponent>(entity, Vector3(i, i, i));
        if (i > 5)
        {
            registry.create_component<NameComponent>(
                entity, std::string("Entity" + std::to_string(i))
            );
        }
    }

    auto view = ecs::View<TransformComponent, NameComponent>(&registry);
    std::size_t i = 0;
    std::size_t count = 0;
    for (ecs::Entity entity : view)
    {
        bool result = view.has_required(entity);
        if (i > 5)
        {
            EXPECT_TRUE(result);
            count++;
        }
        else
            EXPECT_FALSE(result);

        i++;
    }

    EXPECT_EQ(count, 4);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
