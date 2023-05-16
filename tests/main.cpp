#include "../ecs.hpp"

struct TransformComponent {
    float position[3];
    float scale[3];
    float rotation[4];

    float model_matrix[4][4];
};

struct MeshRendererComponent {
    std::string path;
};

int main(int argc, char** argv) {
    ecs::ObjectPool* pool = new ecs::CreateObjectPool<TransformComponent>(1);

    pool->malloc<TransformComponent>();

    std::cout << "pool type: " << pool->get_name() << "\n";

    delete pool;
    return 0;
}
