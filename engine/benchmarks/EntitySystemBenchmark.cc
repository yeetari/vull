#include <vull/core/World.hh>

#include <benchmark/benchmark.h>

namespace {

constexpr float k_delta_time = 1.0F / 60.0F;

struct Position {
    float x;
    float y;
    Position(float x, float y) : x(x), y(y) {}
};

struct Velocity {
    float x;
    float y;
    Velocity(float x, float y) : x(x), y(y) {}
};

struct PhysicsSystem : public System<PhysicsSystem> {
    void update(World *world, float dt) override {
        for (auto entity : world->view<Position, Velocity>()) {
            auto *position = entity.get<Position>();
            auto *velocity = entity.get<Velocity>();
            position->x += velocity->x * dt;
            position->y += velocity->y * dt;
        }
    }
};

void create_entities(benchmark::State &state) {
    for (auto _ : state) {
        World world;
        for (auto i = 0; i < state.range(); i++) {
            benchmark::DoNotOptimize(world.create_entity());
        }
    }
}

void add_one_component(benchmark::State &state) {
    for (auto _ : state) {
        state.PauseTiming();
        World world;
        Vector<Entity> entities;
        entities.ensure_capacity(state.range());
        for (auto i = 0; i < state.range(); i++) {
            entities.push(world.create_entity());
        }
        state.ResumeTiming();
        for (auto &entity : entities) {
            entity.add<Position>(2, 4);
        }
    }
}

void add_two_components(benchmark::State &state) {
    for (auto _ : state) {
        state.PauseTiming();
        World world;
        Vector<Entity> entities;
        entities.ensure_capacity(state.range());
        for (auto i = 0; i < state.range(); i++) {
            entities.push(world.create_entity());
        }
        state.ResumeTiming();
        for (auto &entity : entities) {
            entity.add<Position>(2, 4);
            entity.add<Velocity>(4, 6);
        }
    }
}

void iterate_one_component(benchmark::State &state) {
    World world;
    for (auto i = 0; i < state.range(); i++) {
        auto entity = world.create_entity();
        entity.add<Position>(2, 4);
    }
    for (auto _ : state) {
        for (auto entity : world.view<Position>()) {
            benchmark::DoNotOptimize(entity.get<Position>());
        }
    }
}

void iterate_two_components(benchmark::State &state) {
    World world;
    for (auto i = 0; i < state.range(); i++) {
        auto entity = world.create_entity();
        entity.add<Position>(2, 4);
        entity.add<Velocity>(4, 6);
    }
    for (auto _ : state) {
        for (auto entity : world.view<Position, Velocity>()) {
            benchmark::DoNotOptimize(entity.get<Position>());
            benchmark::DoNotOptimize(entity.get<Velocity>());
        }
    }
}

void update_systems(benchmark::State &state) {
    World world;
    world.add<PhysicsSystem>();
    for (auto i = 0; i < state.range(); i++) {
        auto entity = world.create_entity();
        entity.add<Position>(2, 4);
        entity.add<Velocity>(4, 6);
    }
    for (auto _ : state) {
        world.update(k_delta_time);
    }
}

BENCHMARK(create_entities)->Arg(100000)->Arg(1000000)->Arg(10000000)->Unit(benchmark::TimeUnit::kMillisecond);
BENCHMARK(add_one_component)->Arg(100000)->Arg(1000000)->Arg(10000000)->Unit(benchmark::TimeUnit::kMillisecond);
BENCHMARK(add_two_components)->Arg(100000)->Arg(1000000)->Arg(10000000)->Unit(benchmark::TimeUnit::kMillisecond);
BENCHMARK(iterate_one_component)->Arg(100000)->Arg(1000000)->Arg(10000000)->Unit(benchmark::TimeUnit::kMillisecond);
BENCHMARK(iterate_two_components)->Arg(100000)->Arg(1000000)->Arg(10000000)->Unit(benchmark::TimeUnit::kMillisecond);
BENCHMARK(update_systems)->Arg(100000)->Arg(1000000)->Arg(10000000)->Unit(benchmark::TimeUnit::kMillisecond);

} // namespace
