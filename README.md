# Entity Component System

small, simple, and templated single header ECS library that doesn't use any dependencies other
than the C++ standard library

below is an example of the libraries main features

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

// get view (aka group or 1) of components pushed to the registry
auto view = ecs::View<TransformComponent, MeshRendererComponent>(&registry);

// looping over view and get indervidual entities components
for (ecs::Entity entity : view) {
    if (view.has_required(entity)) {
        // either obtain entities through get<_T>():
        TransformComponent* transform = view.get<TransformComponent>();
        MeshRendererComponent* mesh_renderer = group.get<MeshRendererComponent>();
        // or
        auto [transform, mesh_renderer] = view.get();

        // your logic ...
    }
}

// remove component
registry.destroy_component<TransformComponent>(entity);

// remove entity and all of its components
registry.destroy_entity(entity);
```
