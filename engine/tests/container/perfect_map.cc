#include <vull/container/perfect_map.hh>

#include <vull/container/map_entry.hh>
#include <vull/container/vector.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/test.hh>
#include <vull/support/utility.hh>

#include <stddef.h>

using namespace vull;

TEST_CASE(PerfectMap, FromEntries) {
    Vector<MapEntry<String, size_t>> entries;
    entries.push({"Foo", 5});
    entries.push({"Bar", 7});
    entries.push({"Baz", 11});

    auto map = PerfectMap<String, size_t>::from_entries(entries);
    EXPECT(map.contains("Foo"));
    EXPECT(map.contains("Bar"));
    EXPECT(map.contains("Baz"));
    EXPECT(!map.contains("FooBar"));

    EXPECT(!map.get("FooBar"));
    EXPECT(map.get("Foo"));
    EXPECT(*map.get("Bar") == 7);
}
