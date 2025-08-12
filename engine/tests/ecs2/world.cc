#include <vull/ecs2/entity.hh>
#include <vull/ecs2/world.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(EcsWorld, Recycle) {
    ecs::World world;
    auto foo = world.create();
    auto bar = world.create();
    EXPECT_TRUE(world.is_valid(foo));
    EXPECT_TRUE(world.is_valid(bar));

    world.destroy(foo);
    EXPECT_FALSE(world.is_valid(foo));
    EXPECT_TRUE(world.is_valid(bar));

    auto baz = world.create();
    EXPECT_FALSE(world.is_valid(foo));
    EXPECT_TRUE(world.is_valid(bar));
    EXPECT_TRUE(world.is_valid(baz));
    EXPECT_THAT(baz.index(), is(equal_to(foo.index())));
    EXPECT_THAT(baz.version(), is(equal_to(1)));
}

TEST_CASE(EcsWorld, SequentialId) {
    ecs::World world;
    for (ecs::EntityIndex i = 0; i < 20; i++) {
        auto entity = world.create();
        EXPECT_THAT(entity.index(), is(equal_to(i)));
        EXPECT_THAT(entity.version(), is(equal_to(0)));
    }
}

TEST_CASE(EcsWorld, IsValidOob) {
    ecs::World world;
    EXPECT_FALSE(world.is_valid(ecs::Entity::null()));
    EXPECT_FALSE(world.is_valid(ecs::Entity::make(10, 0)));
}

TEST_CASE(EcsWorld, VersionLimit) {
    ecs::World world;
    auto first = world.create();
    world.destroy(first);
    for (int i = 0; i < ecs::Entity::null_version(); i++) {
        auto entity = world.create();
        world.destroy(entity);
        EXPECT_FALSE(world.is_valid(entity));
    }
    world.create();
    EXPECT_FALSE(world.is_valid(first));
}
