#include <vull/ecs/Entity.hh>

#include <vull/ecs/Component.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Test.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/Vector.hh>

#include <stdint.h>

using namespace vull;

namespace vull {

template <typename>
class Function;

} // namespace vull

namespace {

class BaseComp {
    int *m_destruct_count{nullptr};

public:
    static void serialise(BaseComp &, const Function<void(uint8_t)> &) {}

    BaseComp() = default;
    explicit BaseComp(int &destruct_count) : m_destruct_count(&destruct_count) {}
    BaseComp(const BaseComp &) = default;
    BaseComp(BaseComp &&) = delete;
    ~BaseComp() {
        if (m_destruct_count != nullptr) {
            (*m_destruct_count)++;
        }
    }

    BaseComp &operator=(const BaseComp &) = default;
    BaseComp &operator=(BaseComp &&) = delete;
};

struct Foo : BaseComp {
    VULL_DECLARE_COMPONENT(0);
    using BaseComp::BaseComp;
    static Foo deserialise(const Function<uint8_t()> &) { VULL_ENSURE_NOT_REACHED(); }
};

struct Bar : BaseComp {
    VULL_DECLARE_COMPONENT(1);
    using BaseComp::BaseComp;
    static Bar deserialise(const Function<uint8_t()> &) { VULL_ENSURE_NOT_REACHED(); }
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

TEST_SUITE(Entity, {
    ;
    TEST_CASE(CreateDestroy) {
        EntityManager manager;
        Vector<Entity> entities;
        for (int i = 0; i < 20; i++) {
            auto entity = manager.create_entity();
            // IDs should be sequential.
            EXPECT(entity == EntityId(i));
            entities.push(entity);
        }
        for (auto entity : entities) {
            EXPECT(manager.valid(entity));
            entity.destroy();
            EXPECT(!manager.valid(entity));
        }
    }

    TEST_CASE(AddRemoveComponent) {
        EntityManager manager;
        manager.register_component<Foo>();
        manager.register_component<Bar>();

        int foo_destruct_count = 0;
        int bar_destruct_count = 0;

        auto entity = manager.create_entity();
        EXPECT(!entity.has<Foo>());
        EXPECT(!entity.has<Bar>());

        entity.add<Foo>(foo_destruct_count);
        EXPECT(entity.has<Foo>());
        EXPECT(!entity.has<Bar>());

        entity.add<Bar>(bar_destruct_count);
        EXPECT(entity.has<Foo>());
        EXPECT(entity.has<Bar>());
        EXPECT(entity.has<Foo, Bar>());

        entity.remove<Bar>();
        EXPECT(entity.has<Foo>());
        EXPECT(!entity.has<Bar>());

        entity.remove<Foo>();
        entity.add<Bar>(bar_destruct_count);
        EXPECT(!entity.has<Foo>());
        EXPECT(entity.has<Bar>());

        entity.destroy();
        EXPECT(!entity.has<Foo>());
        EXPECT(!entity.has<Bar>());
        EXPECT(!entity.has<Foo, Bar>());
        EXPECT(foo_destruct_count == 1);
        EXPECT(bar_destruct_count == 2);
    }

    TEST_CASE(View) {
        EntityManager manager;
        manager.register_component<Foo>();
        manager.register_component<Bar>();

        Vector<EntityId> foo_entities;
        Vector<EntityId> bar_entities;
        for (int i = 0; i < 500; i++) {
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
        EXPECT(foo_view.size() == 250);
        EXPECT(bar_view.size() == 167);
        EXPECT(foo_bar_view.size() == 84);

        auto contains = [](const auto &view, EntityId entity) {
            for (auto tuple : view) {
                if (vull::get<0>(tuple) == entity) {
                    return true;
                }
            }
            return false;
        };

        for (EntityId entity : foo_entities) {
            EXPECT(contains(foo_view, entity));
            if (entity % 3 == 0) {
                EXPECT(contains(bar_view, entity));
                EXPECT(contains(foo_bar_view, entity));
            } else {
                EXPECT(!contains(bar_view, entity));
                EXPECT(!contains(foo_bar_view, entity));
            }
        }
        for (EntityId entity : bar_entities) {
            EXPECT(contains(bar_view, entity));
            if (entity % 2 == 0) {
                EXPECT(contains(foo_view, entity));
                EXPECT(contains(foo_bar_view, entity));
            } else {
                EXPECT(!contains(foo_view, entity));
                EXPECT(!contains(foo_bar_view, entity));
            }
        }
    }

    TEST_CASE(ViewNoFirstMatch) {
        EntityManager manager;
        manager.register_component<Foo>();
        manager.register_component<Bar>();

        auto entity = manager.create_entity();
        entity.add<Foo>();

        auto view = manager.view<Foo, Bar>();
        EXPECT(!(view.begin() != view.end()));
    }
})
