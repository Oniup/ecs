# Entity Component System

```cpp
// create registry
ecs::Registry registry = ecs::Registry();

// create entity
ecs::Entity entity = registry.create_entity();

// push component
TransformComponent* transform = registry.create_component<TransformComponent>(entity);
registry.create_component<MeshRendererComponent>(entity);

if (transform != nullptr) {
    ...
}

// get group of components
ecs::Group<TransformComponent, MeshRendererComponent> group =
    registry.get_group<TransformComponent, MeshRendererComponent>();

// looping over group
for (ecs::Entity ent : group) {
    TransformComponent* transform = group.get<TransformComponent>(ent);
    MeshRendererComponent* mesh_renderer = group.get<MeshRendererComponent>(ent);

    ...
}

// remove component
registry.destroy_component<TransformComponent>(entity);

// remove entity and all of its components
registry.destroy_entity(entity);
```
