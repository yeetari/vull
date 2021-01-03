#include <vull/core/World.hh>
#include <vull/support/Vector.hh>

#include <gtest/gtest.h>

namespace {

constexpr float k_delta_time = 1.0F / 60.0F;

struct Foo {
    int val;
    explicit Foo(int val = 0) : val(val) {}
};
struct Bar {
    int val;
    explicit Bar(int val = 0) : val(val) {}
};

TEST(EntitySystemTest, CreateDestroyEntities) {
    World world;
    EXPECT_EQ(world.entity_count(), 0);
    Vector<Entity> entities;
    for (int i = 0; i < 20; i++) {
        entities.push(world.create_entity());
    }
    EXPECT_EQ(world.entity_count(), 20);
    for (auto entity : entities) {
        entity.destroy();
    }
    EXPECT_EQ(world.entity_count(), 0);
}

TEST(EntitySystemTest, AddRemoveComponent) {
    World world;
    auto entity = world.create_entity();
    entity.add<Foo>();
    EXPECT_TRUE(entity.has<Foo>());
    EXPECT_FALSE(entity.has<Bar>());
    entity.add<Bar>();
    EXPECT_TRUE(entity.has<Foo>());
    EXPECT_TRUE(entity.has<Bar>());
    entity.remove<Foo>();
    EXPECT_FALSE(entity.has<Foo>());
    EXPECT_TRUE(entity.has<Bar>());
    entity.remove<Bar>();
    EXPECT_FALSE(entity.has<Foo>());
    EXPECT_FALSE(entity.has<Bar>());
}

TEST(EntitySystemTest, SystemIterate) {
    struct FooBarSystem : public System<FooBarSystem> {
        void update(World *world, float dt) override {
            EXPECT_EQ(dt, k_delta_time);
            EXPECT_EQ(world->get<FooBarSystem>(), this);
            Vector<Entity> foo_entities;
            Vector<Entity> bar_entities;
            Vector<Entity> foo_bar_entities;
            for (auto entity : world->view<Foo>()) {
                foo_entities.push(entity);
            }
            for (auto entity : world->view<Bar>()) {
                bar_entities.push(entity);
            }
            for (auto entity : world->view<Foo, Bar>()) {
                foo_bar_entities.push(entity);
            }
            ASSERT_EQ(foo_entities.size(), 3);
            ASSERT_EQ(bar_entities.size(), 3);
            ASSERT_EQ(foo_bar_entities.size(), 2);
            EXPECT_EQ(foo_entities[0].get<Foo>()->val, 2);
            EXPECT_EQ(foo_entities[1].get<Foo>()->val, 10);
            EXPECT_EQ(foo_entities[2].get<Foo>()->val, 6);
            EXPECT_EQ(bar_entities[0].get<Bar>()->val, 4);
            EXPECT_EQ(bar_entities[1].get<Bar>()->val, 8);
            EXPECT_EQ(bar_entities[2].get<Bar>()->val, 5);
            EXPECT_EQ(foo_bar_entities[0].get<Foo>()->val, 2);
            EXPECT_EQ(foo_bar_entities[0].get<Bar>()->val, 4);
            EXPECT_EQ(foo_bar_entities[1].get<Foo>()->val, 6);
            EXPECT_EQ(foo_bar_entities[1].get<Bar>()->val, 8);
        }
    };
    World world;
    world.add<FooBarSystem>();
    auto a = world.create_entity();
    auto b = world.create_entity();
    auto c = world.create_entity();
    auto d = world.create_entity();
    a.add<Foo>(2);
    a.add<Bar>(4);
    b.add<Foo>(10);
    b.add<Bar>();
    b.remove<Bar>();
    c.add<Bar>(8);
    c.add<Foo>(6);
    d.add<Bar>(5);
    world.update(k_delta_time);
}

} // namespace
