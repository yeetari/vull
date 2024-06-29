#include <vull/container/perfect_map.hh>

#include <vull/container/map_entry.hh>
#include <vull/container/vector.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <stddef.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(PerfectMap, FromEntries) {
    Vector<MapEntry<String, size_t>> entries;
    entries.push({"Foo", 5});
    entries.push({"Bar", 7});
    entries.push({"Baz", 11});

    auto map = PerfectMap<String, size_t>::from_entries(entries);
    EXPECT_THAT(map, is(containing("Foo")));
    EXPECT_THAT(map, is(containing("Bar")));
    EXPECT_THAT(map, is(containing("Baz")));
    EXPECT_THAT(map, is(not_(containing("FooBar"))));

    EXPECT_THAT(map.get("FooBar"), is(null()));
    EXPECT_THAT(map.get("Foo"), is(equal_to(5u)));
    EXPECT_THAT(map.get("Bar"), is(equal_to(7u)));
    EXPECT_THAT(map.get("Baz"), is(equal_to(11u)));
}
