#include "../ecs.hpp"

struct TransformComponent {
    float position[3] = {};
    float scale[3] = {};
    float rotation[4] = {};

    TransformComponent(float x_pos, float y_pos, float z_pos) {
        position[0] = x_pos;
        position[1] = y_pos;
        position[2] = z_pos;
    }
};

int main(int argc, char** argv) {
    ecs::Registry registry = ecs::Registry();
    ecs::Entity entity = registry.create_entity();

    TransformComponent* transform = registry.create_component<TransformComponent>(entity, 32, 32, 32);
    std::cout << "[" << transform->position[0] << ", " << transform->position[1] << ", "
              << transform->position[2] << "]\n";

    entity = registry.create_entity();
    transform = registry.create_component<TransformComponent>(entity, 32, 32, 32);
    std::cout << "[" << transform->position[0] << ", " << transform->position[1] << ", "
              << transform->position[2] << "]\n";

    return 0;
}
