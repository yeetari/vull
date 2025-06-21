#include <vull/ecs/entity.hh>

#include <vull/container/vector.hh>
#include <vull/ecs/component.hh>
#include <vull/ecs/entity_id.hh>
#include <vull/support/assert.hh>
#include <vull/support/tuple.hh>
#include <vull/support/utility.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/move_tester.hh>
#include <vull/test/test.hh>

#include <stddef.h>

using namespace vull;
using namespace vull::test::matchers;

namespace vull {

struct Stream;

} // namespace vull

namespace {

class BaseComp : public test::MoveTester {
public:
    static void serialise(BaseComp &, Stream &) {}
    using test::MoveTester::MoveTester;
};

struct Foo : BaseComp {
    VULL_DECLARE_COMPONENT(0);
    using BaseComp::BaseComp;
    static Foo deserialise(Stream &) { VULL_ENSURE_NOT_REACHED(); }
};

struct Bar : BaseComp {
    VULL_DECLARE_COMPONENT(1);
    using BaseComp::BaseComp;
    static Bar deserialise(Stream &) { VULL_ENSURE_NOT_REACHED(); }
};

template <typename... Comps>
Vector<Tuple<Entity, Comps &...>> sum_view(EntityManager &manager) {
    Vector<Tuple<Entity, Comps &...>> matching;
    for (auto tuple : manager.view<Comps...>()) {
        matching.push(tuple);
    }
    return matching;
}

} // namespace

TEST_CASE(Entity, CreateDestroy) {
    EntityManager manager;
    Vector<Entity> entities;
    for (int i = 0; i < 20; i++) {
        auto entity = manager.create_entity();
        // IDs should be sequential.
        EXPECT_THAT(entity, is(equal_to(EntityId(i))));
        entities.push(entity);
    }
    for (auto entity : entities) {
        EXPECT_TRUE(manager.valid(entity));
        entity.destroy();
        EXPECT_FALSE(manager.valid(entity));
    }
}

TEST_CASE(Entity, AddRemoveComponent) {
    EntityManager manager;
    manager.register_component<Foo>();
    manager.register_component<Bar>();

    size_t foo_destruct_count = 0;
    size_t bar_destruct_count = 0;

    auto entity = manager.create_entity();
    EXPECT_FALSE(entity.has<Foo>());
    EXPECT_FALSE(entity.has<Bar>());

    entity.add<Foo>(foo_destruct_count);
    EXPECT_TRUE(entity.has<Foo>());
    EXPECT_FALSE(entity.has<Bar>());

    entity.add<Bar>(bar_destruct_count);
    EXPECT_TRUE(entity.has<Foo>());
    EXPECT_TRUE(entity.has<Bar>());
    EXPECT_TRUE((entity.has<Foo, Bar>()));

    entity.remove<Bar>();
    EXPECT_TRUE(entity.has<Foo>());
    EXPECT_FALSE(entity.has<Bar>());

    entity.remove<Foo>();
    entity.add<Bar>(bar_destruct_count);
    EXPECT_FALSE(entity.has<Foo>());
    EXPECT_TRUE(entity.has<Bar>());

    entity.destroy();
    EXPECT_FALSE(entity.has<Foo>());
    EXPECT_FALSE(entity.has<Bar>());
    EXPECT_FALSE((entity.has<Foo, Bar>()));
    EXPECT_THAT(foo_destruct_count, is(equal_to(1)));
    EXPECT_THAT(bar_destruct_count, is(equal_to(2)));
}

TEST_CASE(Entity, View) {
    EntityManager manager;
    manager.register_component<Foo>();
    manager.register_component<Bar>();

    Vector<EntityId> foo_entities;
    Vector<EntityId> bar_entities;
    for (size_t i = 0; i < 500; i++) {
        auto entity = manager.create_entity();
        if (i % 2 == 0) {
            entity.add<Foo>();
            foo_entities.push(entity);
        }
        if (i % 3 == 0) {
            entity.add<Bar>();
            bar_entities.push(entity);
        }
    }

    auto foo_view = sum_view<Foo>(manager);
    auto bar_view = sum_view<Bar>(manager);
    auto foo_bar_view = sum_view<Foo, Bar>(manager);
    EXPECT_THAT(foo_view.size(), is(equal_to(250)));
    EXPECT_THAT(bar_view.size(), is(equal_to(167)));
    EXPECT_THAT(foo_bar_view.size(), is(equal_to(84)));

    auto contains = [](const auto &view, EntityId entity) {
        for (auto tuple : view) {
            if (vull::get<0>(tuple) == entity) {
                return true;
            }
        }
        return false;
    };

    for (EntityId entity : foo_entities) {
        EXPECT_TRUE(contains(foo_view, entity));
        if (entity % 3 == 0) {
            EXPECT_TRUE(contains(bar_view, entity));
            EXPECT_TRUE(contains(foo_bar_view, entity));
        } else {
            EXPECT_FALSE(contains(bar_view, entity));
            EXPECT_FALSE(contains(foo_bar_view, entity));
        }
    }
    for (EntityId entity : bar_entities) {
        EXPECT_TRUE(contains(bar_view, entity));
        if (entity % 2 == 0) {
            EXPECT_TRUE(contains(foo_view, entity));
            EXPECT_TRUE(contains(foo_bar_view, entity));
        } else {
            EXPECT_FALSE(contains(foo_view, entity));
            EXPECT_FALSE(contains(foo_bar_view, entity));
        }
    }
}

TEST_CASE(Entity, ViewNoFirstMatch) {
    EntityManager manager;
    manager.register_component<Foo>();
    manager.register_component<Bar>();

    auto entity = manager.create_entity();
    entity.add<Foo>();

    auto view = manager.view<Foo, Bar>();
    EXPECT_THAT(view.begin(), is(equal_to(view.end())));
}
