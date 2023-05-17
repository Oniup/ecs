#include "../ecs.hpp"

struct Message {
    std::string message;
    void print_message() { std::cout << message << "\n"; }
};

int main(int argc, char** argv) {
    ecs::ObjectPool* pool = new ecs::CreateObjectPool<Message>(2);

    Message* m1 = pool->malloc<Message>();
    m1->message = "first";
    Message* m2 = pool->malloc<Message>();
    m2->message = "second";
    Message* m3 = pool->malloc<Message>();
    m3->message = "third";
    Message* m4 = pool->malloc<Message>();
    m4->message = "second last message";
    Message* m5 = pool->malloc<Message>();
    m5->message = "last message";

    pool->free(m2);
    m1->print_message();
    m2->print_message();
    m3->print_message();
    m4->print_message();
    m5->print_message();

    delete pool;
    return 0;
}
